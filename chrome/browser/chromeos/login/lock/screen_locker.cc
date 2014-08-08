// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/lock/screen_locker.h"

#include <string>
#include <vector>

#include "ash/ash_switches.h"
#include "ash/audio/sounds.h"
#include "ash/desktop_background/desktop_background_controller.h"
#include "ash/shell.h"
#include "ash/wm/lock_state_controller.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/timer/timer.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/auth/login_performer.h"
#include "chrome/browser/chromeos/login/lock/webui_screen_locker.h"
#include "chrome/browser/chromeos/login/login_utils.h"
#include "chrome/browser/chromeos/login/supervised/supervised_user_authentication.h"
#include "chrome/browser/chromeos/login/ui/user_adding_screen.h"
#include "chrome/browser/chromeos/login/users/supervised_user_manager.h"
#include "chrome/browser/chromeos/login/users/user_manager.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/webui/chromeos/login/screenlock_icon_provider.h"
#include "chrome/browser/ui/webui/chromeos/login/screenlock_icon_source.h"
#include "chrome/common/chrome_switches.h"
#include "chromeos/audio/chromeos_sounds.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager_client.h"
#include "chromeos/login/auth/authenticator.h"
#include "chromeos/login/auth/extended_authenticator.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/user_manager/user_type.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"
#include "media/audio/sounds/sounds_manager.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

using base::UserMetricsAction;
using content::BrowserThread;

namespace chromeos {

namespace {

// Timeout for unlock animation guard - some animations may be required to run
// on successful authentication before unlocking, but we want to be sure that
// unlock happens even if animations are broken.
const int kUnlockGuardTimeoutMs = 400;

// Observer to start ScreenLocker when locking the screen is requested.
class ScreenLockObserver : public SessionManagerClient::StubDelegate,
                           public content::NotificationObserver,
                           public UserAddingScreen::Observer {
 public:
  ScreenLockObserver() : session_started_(false) {
    registrar_.Add(this,
                   chrome::NOTIFICATION_SESSION_STARTED,
                   content::NotificationService::AllSources());
    DBusThreadManager::Get()->GetSessionManagerClient()->SetStubDelegate(this);
  }

  virtual ~ScreenLockObserver() {
    if (DBusThreadManager::IsInitialized()) {
      DBusThreadManager::Get()->GetSessionManagerClient()->SetStubDelegate(
          NULL);
    }
  }

  bool session_started() const { return session_started_; }

  // SessionManagerClient::StubDelegate overrides:
  virtual void LockScreenForStub() OVERRIDE {
    ScreenLocker::HandleLockScreenRequest();
  }

  // NotificationObserver overrides:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE {
    if (type == chrome::NOTIFICATION_SESSION_STARTED)
      session_started_ = true;
    else
      NOTREACHED() << "Unexpected notification " << type;
  }

  // UserAddingScreen::Observer overrides:
  virtual void OnUserAddingFinished() OVERRIDE {
    UserAddingScreen::Get()->RemoveObserver(this);
    ScreenLocker::HandleLockScreenRequest();
  }

 private:
  bool session_started_;
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ScreenLockObserver);
};

ScreenLockObserver* g_screen_lock_observer = NULL;

}  // namespace

// static
ScreenLocker* ScreenLocker::screen_locker_ = NULL;

//////////////////////////////////////////////////////////////////////////////
// ScreenLocker, public:

ScreenLocker::ScreenLocker(const user_manager::UserList& users)
    : users_(users),
      locked_(false),
      start_time_(base::Time::Now()),
      auth_status_consumer_(NULL),
      incorrect_passwords_count_(0),
      weak_factory_(this) {
  DCHECK(!screen_locker_);
  screen_locker_ = this;

  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  media::SoundsManager* manager = media::SoundsManager::Get();
  manager->Initialize(SOUND_LOCK,
                      bundle.GetRawDataResource(IDR_SOUND_LOCK_WAV));
  manager->Initialize(SOUND_UNLOCK,
                      bundle.GetRawDataResource(IDR_SOUND_UNLOCK_WAV));

  ash::Shell::GetInstance()->
      lock_state_controller()->SetLockScreenDisplayedCallback(
          base::Bind(base::IgnoreResult(&ash::PlaySystemSoundIfSpokenFeedback),
                     static_cast<media::SoundsManager::SoundKey>(
                         chromeos::SOUND_LOCK)));
}

void ScreenLocker::Init() {
  authenticator_ = LoginUtils::Get()->CreateAuthenticator(this);
  extended_authenticator_ = new ExtendedAuthenticator(this);
  delegate_.reset(new WebUIScreenLocker(this));
  delegate_->LockScreen();

  // Ownership of |icon_image_source| is passed.
  screenlock_icon_provider_.reset(new ScreenlockIconProvider);
  ScreenlockIconSource* screenlock_icon_source =
      new ScreenlockIconSource(screenlock_icon_provider_->AsWeakPtr());
  content::URLDataSource::Add(
      GetAssociatedWebUI()->GetWebContents()->GetBrowserContext(),
      screenlock_icon_source);
}

void ScreenLocker::OnAuthFailure(const AuthFailure& error) {
  content::RecordAction(UserMetricsAction("ScreenLocker_OnLoginFailure"));
  if (authentication_start_time_.is_null()) {
    LOG(ERROR) << "Start time is not set at authentication failure";
  } else {
    base::TimeDelta delta = base::Time::Now() - authentication_start_time_;
    VLOG(1) << "Authentication failure: " << delta.InSecondsF() << " second(s)";
    UMA_HISTOGRAM_TIMES("ScreenLocker.AuthenticationFailureTime", delta);
  }

  EnableInput();
  // Don't enable signout button here as we're showing
  // MessageBubble.

  delegate_->ShowErrorMessage(incorrect_passwords_count_++ ?
                                  IDS_LOGIN_ERROR_AUTHENTICATING_2ND_TIME :
                                  IDS_LOGIN_ERROR_AUTHENTICATING,
                              HelpAppLauncher::HELP_CANT_ACCESS_ACCOUNT);

  if (auth_status_consumer_)
    auth_status_consumer_->OnAuthFailure(error);
}

void ScreenLocker::OnAuthSuccess(const UserContext& user_context) {
  incorrect_passwords_count_ = 0;
  if (authentication_start_time_.is_null()) {
    if (!user_context.GetUserID().empty())
      LOG(ERROR) << "Start time is not set at authentication success";
  } else {
    base::TimeDelta delta = base::Time::Now() - authentication_start_time_;
    VLOG(1) << "Authentication success: " << delta.InSecondsF() << " second(s)";
    UMA_HISTOGRAM_TIMES("ScreenLocker.AuthenticationSuccessTime", delta);
  }

  const user_manager::User* user =
      UserManager::Get()->FindUser(user_context.GetUserID());
  if (user) {
    if (!user->is_active())
      UserManager::Get()->SwitchActiveUser(user_context.GetUserID());
  } else {
    NOTREACHED() << "Logged in user not found.";
  }

  authentication_capture_.reset(new AuthenticationParametersCapture());
  authentication_capture_->user_context = user_context;

  // Add guard for case when something get broken in call chain to unlock
  // for sure.
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&ScreenLocker::UnlockOnLoginSuccess,
          weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(kUnlockGuardTimeoutMs));
  delegate_->AnimateAuthenticationSuccess();
}

void ScreenLocker::UnlockOnLoginSuccess() {
  DCHECK(base::MessageLoopForUI::IsCurrent());
  if (!authentication_capture_.get()) {
    LOG(WARNING) << "Call to UnlockOnLoginSuccess without previous " <<
      "authentication success.";
    return;
  }

  if (auth_status_consumer_) {
    auth_status_consumer_->OnAuthSuccess(authentication_capture_->user_context);
  }
  authentication_capture_.reset();
  weak_factory_.InvalidateWeakPtrs();

  VLOG(1) << "Hiding the lock screen.";
  chromeos::ScreenLocker::Hide();
}

void ScreenLocker::Authenticate(const UserContext& user_context) {
  LOG_ASSERT(IsUserLoggedIn(user_context.GetUserID()))
      << "Invalid user trying to unlock.";

  authentication_start_time_ = base::Time::Now();
  delegate_->SetInputEnabled(false);
  delegate_->OnAuthenticate();

  // Special case: supervised users. Use special authenticator.
  if (const user_manager::User* user =
          FindUnlockUser(user_context.GetUserID())) {
    if (user->GetType() == user_manager::USER_TYPE_SUPERVISED) {
      UserContext updated_context = UserManager::Get()
                                        ->GetSupervisedUserManager()
                                        ->GetAuthentication()
                                        ->TransformKey(user_context);
      // TODO(antrim) : replace empty closure with explicit method.
      // http://crbug.com/351268
      BrowserThread::PostTask(
          BrowserThread::UI,
          FROM_HERE,
          base::Bind(&ExtendedAuthenticator::AuthenticateToCheck,
                     extended_authenticator_.get(),
                     updated_context,
                     base::Closure()));
      return;
    }
  }

  // TODO(antrim) : migrate to new authenticator for all types of users.
  // http://crbug.com/351268
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&Authenticator::AuthenticateToUnlock,
                 authenticator_.get(),
                 user_context));
}

const user_manager::User* ScreenLocker::FindUnlockUser(
    const std::string& user_id) {
  const user_manager::User* unlock_user = NULL;
  for (user_manager::UserList::const_iterator it = users_.begin();
       it != users_.end();
       ++it) {
    if ((*it)->email() == user_id) {
      unlock_user = *it;
      break;
    }
  }
  return unlock_user;
}

void ScreenLocker::ClearErrors() {
  delegate_->ClearErrors();
}

void ScreenLocker::Signout() {
  delegate_->ClearErrors();
  content::RecordAction(UserMetricsAction("ScreenLocker_Signout"));
  // We expect that this call will not wait for any user input.
  // If it changes at some point, we will need to force exit.
  chrome::AttemptUserExit();

  // Don't hide yet the locker because the chrome screen may become visible
  // briefly.
}

void ScreenLocker::EnableInput() {
  delegate_->SetInputEnabled(true);
}

void ScreenLocker::ShowErrorMessage(int error_msg_id,
                                    HelpAppLauncher::HelpTopic help_topic_id,
                                    bool sign_out_only) {
  delegate_->SetInputEnabled(!sign_out_only);
  delegate_->ShowErrorMessage(error_msg_id, help_topic_id);
}

void ScreenLocker::SetLoginStatusConsumer(
    chromeos::AuthStatusConsumer* consumer) {
  auth_status_consumer_ = consumer;
}

// static
void ScreenLocker::InitClass() {
  DCHECK(!g_screen_lock_observer);
  g_screen_lock_observer = new ScreenLockObserver;
}

// static
void ScreenLocker::ShutDownClass() {
  DCHECK(g_screen_lock_observer);
  delete g_screen_lock_observer;
  g_screen_lock_observer = NULL;
}

// static
void ScreenLocker::HandleLockScreenRequest() {
  VLOG(1) << "Received LockScreen request from session manager";
  DCHECK(g_screen_lock_observer);
  if (UserAddingScreen::Get()->IsRunning()) {
    VLOG(1) << "Waiting for user adding screen to stop";
    UserAddingScreen::Get()->AddObserver(g_screen_lock_observer);
    UserAddingScreen::Get()->Cancel();
    return;
  }
  if (g_screen_lock_observer->session_started() &&
      UserManager::Get()->CanCurrentUserLock()) {
    ScreenLocker::Show();
    ash::Shell::GetInstance()->lock_state_controller()->OnStartingLock();
  } else {
    // If the current user's session cannot be locked or the user has not
    // completed all sign-in steps yet, log out instead. The latter is done to
    // avoid complications with displaying the lock screen over the login
    // screen while remaining secure in the case the user walks away during
    // the sign-in steps. See crbug.com/112225 and crbug.com/110933.
    VLOG(1) << "Calling session manager's StopSession D-Bus method";
    DBusThreadManager::Get()->GetSessionManagerClient()->StopSession();
  }
}

// static
void ScreenLocker::Show() {
  content::RecordAction(UserMetricsAction("ScreenLocker_Show"));
  DCHECK(base::MessageLoopForUI::IsCurrent());

  // Check whether the currently logged in user is a guest account and if so,
  // refuse to lock the screen (crosbug.com/23764).
  // For a demo user, we should never show the lock screen (crosbug.com/27647).
  if (UserManager::Get()->IsLoggedInAsGuest() ||
      UserManager::Get()->IsLoggedInAsDemoUser()) {
    VLOG(1) << "Refusing to lock screen for guest/demo account";
    return;
  }

  // If the active window is fullscreen, exit fullscreen to avoid the web page
  // or app mimicking the lock screen. Do not exit fullscreen if the shelf is
  // visible while in fullscreen because the shelf makes it harder for a web
  // page or app to mimick the lock screen.
  ash::wm::WindowState* active_window_state = ash::wm::GetActiveWindowState();
  if (active_window_state &&
      active_window_state->IsFullscreen() &&
      active_window_state->hide_shelf_when_fullscreen()) {
    const ash::wm::WMEvent event(ash::wm::WM_EVENT_TOGGLE_FULLSCREEN);
    active_window_state->OnWMEvent(&event);
  }

  if (!screen_locker_) {
    ScreenLocker* locker =
        new ScreenLocker(UserManager::Get()->GetUnlockUsers());
    VLOG(1) << "Created ScreenLocker " << locker;
    locker->Init();
  } else {
    VLOG(1) << "ScreenLocker " << screen_locker_ << " already exists; "
            << " calling session manager's HandleLockScreenShown D-Bus method";
    DBusThreadManager::Get()->GetSessionManagerClient()->
        NotifyLockScreenShown();
  }
}

// static
void ScreenLocker::Hide() {
  DCHECK(base::MessageLoopForUI::IsCurrent());
  // For a guest/demo user, screen_locker_ would have never been initialized.
  if (UserManager::Get()->IsLoggedInAsGuest() ||
      UserManager::Get()->IsLoggedInAsDemoUser()) {
    VLOG(1) << "Refusing to hide lock screen for guest/demo account";
    return;
  }

  DCHECK(screen_locker_);
  base::Callback<void(void)> callback =
      base::Bind(&ScreenLocker::ScheduleDeletion);
  ash::Shell::GetInstance()->lock_state_controller()->
    OnLockScreenHide(callback);
}

void ScreenLocker::ScheduleDeletion() {
  // Avoid possible multiple calls.
  if (screen_locker_ == NULL)
    return;
  VLOG(1) << "Deleting ScreenLocker " << screen_locker_;

  ash::PlaySystemSoundIfSpokenFeedback(SOUND_UNLOCK);

  delete screen_locker_;
  screen_locker_ = NULL;
}

////////////////////////////////////////////////////////////////////////////////
// ScreenLocker, private:

ScreenLocker::~ScreenLocker() {
  VLOG(1) << "Destroying ScreenLocker " << this;
  DCHECK(base::MessageLoopForUI::IsCurrent());

  if (authenticator_.get())
    authenticator_->SetConsumer(NULL);
  ClearErrors();

  VLOG(1) << "Moving desktop background to unlocked container";
  ash::Shell::GetInstance()->
      desktop_background_controller()->MoveDesktopToUnlockedContainer();

  screen_locker_ = NULL;
  bool state = false;
  VLOG(1) << "Emitting SCREEN_LOCK_STATE_CHANGED with state=" << state;
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_SCREEN_LOCK_STATE_CHANGED,
      content::Source<ScreenLocker>(this),
      content::Details<bool>(&state));

  VLOG(1) << "Calling session manager's HandleLockScreenDismissed D-Bus method";
  DBusThreadManager::Get()->GetSessionManagerClient()->
      NotifyLockScreenDismissed();
}

void ScreenLocker::SetAuthenticator(Authenticator* authenticator) {
  authenticator_ = authenticator;
}

void ScreenLocker::ScreenLockReady() {
  locked_ = true;
  base::TimeDelta delta = base::Time::Now() - start_time_;
  VLOG(1) << "ScreenLocker " << this << " is ready after "
          << delta.InSecondsF() << " second(s)";
  UMA_HISTOGRAM_TIMES("ScreenLocker.ScreenLockTime", delta);

  VLOG(1) << "Moving desktop background to locked container";
  ash::Shell::GetInstance()->
      desktop_background_controller()->MoveDesktopToLockedContainer();

  bool state = true;
  VLOG(1) << "Emitting SCREEN_LOCK_STATE_CHANGED with state=" << state;
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_SCREEN_LOCK_STATE_CHANGED,
      content::Source<ScreenLocker>(this),
      content::Details<bool>(&state));
  VLOG(1) << "Calling session manager's HandleLockScreenShown D-Bus method";
  DBusThreadManager::Get()->GetSessionManagerClient()->NotifyLockScreenShown();
}

content::WebUI* ScreenLocker::GetAssociatedWebUI() {
  return delegate_->GetAssociatedWebUI();
}

bool ScreenLocker::IsUserLoggedIn(const std::string& username) {
  for (user_manager::UserList::const_iterator it = users_.begin();
       it != users_.end();
       ++it) {
    if ((*it)->email() == username)
      return true;
  }
  return false;
}

}  // namespace chromeos
