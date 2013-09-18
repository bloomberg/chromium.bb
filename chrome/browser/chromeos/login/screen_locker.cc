// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screen_locker.h"

#include <string>
#include <vector>

#include "ash/ash_switches.h"
#include "ash/desktop_background/desktop_background_controller.h"
#include "ash/shell.h"
#include "ash/wm/lock_state_controller.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_util.h"
#include "base/timer/timer.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/authenticator.h"
#include "chrome/browser/chromeos/login/login_performer.h"
#include "chrome/browser/chromeos/login/login_utils.h"
#include "chrome/browser/chromeos/login/user_adding_screen.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/login/webui_screen_locker.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager_client.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/user_metrics.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

using content::BrowserThread;
using content::UserMetricsAction;

namespace {

// Timeout for unlock animation guard - some animations may be required to run
// on successful authentication before unlocking, but we want to be sure that
// unlock happens even if animations are broken.
const int kUnlockGuardTimeoutMs = 400;

// Observer to start ScreenLocker when the screen lock
class ScreenLockObserver : public chromeos::SessionManagerClient::Observer,
                           public content::NotificationObserver,
                           public chromeos::UserAddingScreen::Observer {
 public:
  ScreenLockObserver() : session_started_(false) {
    registrar_.Add(this,
                   chrome::NOTIFICATION_LOGIN_USER_CHANGED,
                   content::NotificationService::AllSources());
    registrar_.Add(this,
                   chrome::NOTIFICATION_SESSION_STARTED,
                   content::NotificationService::AllSources());
  }

  // NotificationObserver overrides:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE {
    switch (type) {
      case chrome::NOTIFICATION_LOGIN_USER_CHANGED: {
        // Register Screen Lock only after a user has logged in.
        chromeos::SessionManagerClient* session_manager =
            chromeos::DBusThreadManager::Get()->GetSessionManagerClient();
        if (!session_manager->HasObserver(this))
          session_manager->AddObserver(this);
        break;
      }

      case chrome::NOTIFICATION_SESSION_STARTED: {
        session_started_ = true;
        break;
      }

      default:
        NOTREACHED();
    }
  }

  virtual void LockScreen() OVERRIDE {
    VLOG(1) << "Received LockScreen D-Bus signal from session manager";
    if (chromeos::UserAddingScreen::Get()->IsRunning()) {
      VLOG(1) << "Waiting for user adding screen to stop";
      chromeos::UserAddingScreen::Get()->AddObserver(this);
      chromeos::UserAddingScreen::Get()->Cancel();
      return;
    }
    if (session_started_ &&
        chromeos::UserManager::Get()->CanCurrentUserLock()) {
      chromeos::ScreenLocker::Show();
    } else {
      // If the current user's session cannot be locked or the user has not
      // completed all sign-in steps yet, log out instead. The latter is done to
      // avoid complications with displaying the lock screen over the login
      // screen while remaining secure in the case the user walks away during
      // the sign-in steps. See crbug.com/112225 and crbug.com/110933.
      VLOG(1) << "Calling session manager's StopSession D-Bus method";
      chromeos::DBusThreadManager::Get()->
          GetSessionManagerClient()->StopSession();
    }
  }

  virtual void OnUserAddingFinished() OVERRIDE {
    chromeos::UserAddingScreen::Get()->RemoveObserver(this);
    LockScreen();
  }

 private:
  bool session_started_;
  content::NotificationRegistrar registrar_;
  std::string saved_previous_input_method_id_;
  std::string saved_current_input_method_id_;
  std::vector<std::string> saved_active_input_method_list_;

  DISALLOW_COPY_AND_ASSIGN(ScreenLockObserver);
};

static base::LazyInstance<ScreenLockObserver> g_screen_lock_observer =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

namespace chromeos {

// static
ScreenLocker* ScreenLocker::screen_locker_ = NULL;

//////////////////////////////////////////////////////////////////////////////
// ScreenLocker, public:

ScreenLocker::ScreenLocker(const UserList& users)
    : users_(users),
      locked_(false),
      start_time_(base::Time::Now()),
      login_status_consumer_(NULL),
      incorrect_passwords_count_(0),
      weak_factory_(this) {
  DCHECK(!screen_locker_);
  screen_locker_ = this;
}

void ScreenLocker::Init() {
  authenticator_ = LoginUtils::Get()->CreateAuthenticator(this);
  delegate_.reset(new WebUIScreenLocker(this));
  delegate_->LockScreen();
}

void ScreenLocker::OnLoginFailure(const LoginFailure& error) {
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

  if (login_status_consumer_)
    login_status_consumer_->OnLoginFailure(error);
}

void ScreenLocker::OnLoginSuccess(
    const UserContext& user_context,
    bool pending_requests,
    bool using_oauth) {
  incorrect_passwords_count_ = 0;
  if (authentication_start_time_.is_null()) {
    if (!user_context.username.empty())
      LOG(ERROR) << "Start time is not set at authentication success";
  } else {
    base::TimeDelta delta = base::Time::Now() - authentication_start_time_;
    VLOG(1) << "Authentication success: " << delta.InSecondsF() << " second(s)";
    UMA_HISTOGRAM_TIMES("ScreenLocker.AuthenticationSuccessTime", delta);
  }

  if (!CommandLine::ForCurrentProcess()->HasSwitch(switches::kMultiProfiles)) {
    // TODO(dzhioev): It seems like this branch never executed and should be
    // removed before multi-profile enabling.
    Profile* profile = ProfileManager::GetDefaultProfile();
    if (profile && !user_context.password.empty()) {
      // We have a non-empty password, so notify listeners (such as the sync
      // engine).
      SigninManagerBase* signin = SigninManagerFactory::GetForProfile(profile);
      DCHECK(signin);
      GoogleServiceSigninSuccessDetails details(
          signin->GetAuthenticatedUsername(),
          user_context.password);
      content::NotificationService::current()->Notify(
          chrome::NOTIFICATION_GOOGLE_SIGNIN_SUCCESSFUL,
          content::Source<Profile>(profile),
          content::Details<const GoogleServiceSigninSuccessDetails>(&details));
    }
  }

  if (const User* user = UserManager::Get()->FindUser(user_context.username)) {
    if (!user->is_active())
      UserManager::Get()->SwitchActiveUser(user_context.username);
  } else {
    NOTREACHED() << "Logged in user not found.";
  }

  authentication_capture_.reset(new AuthenticationParametersCapture());
  authentication_capture_->username = user_context.username;
  authentication_capture_->pending_requests = pending_requests;
  authentication_capture_->using_oauth = using_oauth;

  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(ash::switches::kAshDisableNewLockAnimations)) {
    UnlockOnLoginSuccess();
  } else {
    // Add guard for case when something get broken in call chain to unlock
    // for sure.
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&ScreenLocker::UnlockOnLoginSuccess,
            weak_factory_.GetWeakPtr()),
        base::TimeDelta::FromMilliseconds(kUnlockGuardTimeoutMs));
    delegate_->AnimateAuthenticationSuccess();
  }
}

void ScreenLocker::UnlockOnLoginSuccess() {
  DCHECK(base::MessageLoop::current()->type() == base::MessageLoop::TYPE_UI);
  if (!authentication_capture_.get()) {
    LOG(WARNING) << "Call to UnlockOnLoginSuccess without previous " <<
      "authentication success.";
    return;
  }

  if (login_status_consumer_) {
    login_status_consumer_->OnLoginSuccess(
        UserContext(authentication_capture_->username,
                    std::string(),   // password
                    std::string()),  // auth_code
        authentication_capture_->pending_requests,
        authentication_capture_->using_oauth);
  }
  authentication_capture_.reset();
  weak_factory_.InvalidateWeakPtrs();

  VLOG(1) << "Hiding the lock screen.";
  chromeos::ScreenLocker::Hide();
}

void ScreenLocker::Authenticate(const UserContext& user_context) {
  LOG_ASSERT(IsUserLoggedIn(user_context.username))
      << "Invalid user trying to unlock.";
  authentication_start_time_ = base::Time::Now();
  delegate_->SetInputEnabled(false);
  delegate_->OnAuthenticate();

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&Authenticator::AuthenticateToUnlock,
                 authenticator_.get(),
                 user_context));
}

void ScreenLocker::AuthenticateByPassword(const std::string& password) {
  LOG_ASSERT(users_.size() == 1);
  Authenticate(UserContext(users_[0]->email(), password, ""));
}

void ScreenLocker::ClearErrors() {
  delegate_->ClearErrors();
}

void ScreenLocker::EnableInput() {
  delegate_->SetInputEnabled(true);
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

void ScreenLocker::ShowErrorMessage(int error_msg_id,
                                    HelpAppLauncher::HelpTopic help_topic_id,
                                    bool sign_out_only) {
  delegate_->SetInputEnabled(!sign_out_only);
  delegate_->ShowErrorMessage(error_msg_id, help_topic_id);
}

void ScreenLocker::SetLoginStatusConsumer(
    chromeos::LoginStatusConsumer* consumer) {
  login_status_consumer_ = consumer;
}

// static
void ScreenLocker::Show() {
  content::RecordAction(UserMetricsAction("ScreenLocker_Show"));
  DCHECK(base::MessageLoop::current()->type() == base::MessageLoop::TYPE_UI);

  // Check whether the currently logged in user is a guest account and if so,
  // refuse to lock the screen (crosbug.com/23764).
  // For a demo user, we should never show the lock screen (crosbug.com/27647).
  if (UserManager::Get()->IsLoggedInAsGuest() ||
      UserManager::Get()->IsLoggedInAsDemoUser()) {
    VLOG(1) << "Refusing to lock screen for guest/demo account";
    return;
  }

  // Exit fullscreen.
  Browser* browser = chrome::FindLastActiveWithHostDesktopType(
      chrome::HOST_DESKTOP_TYPE_ASH);
  // browser can be NULL if we receive a lock request before the first browser
  // window is shown.
  if (browser && browser->window()->IsFullscreen()) {
    chrome::ToggleFullscreenMode(browser);
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
  DCHECK(base::MessageLoop::current()->type() == base::MessageLoop::TYPE_UI);
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
  delete screen_locker_;
  screen_locker_ = NULL;
}

// static
void ScreenLocker::InitClass() {
  g_screen_lock_observer.Get();
}

////////////////////////////////////////////////////////////////////////////////
// ScreenLocker, private:

ScreenLocker::~ScreenLocker() {
  VLOG(1) << "Destroying ScreenLocker " << this;
  DCHECK(base::MessageLoop::current()->type() == base::MessageLoop::TYPE_UI);

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
  for (UserList::const_iterator it = users_.begin(); it != users_.end(); ++it) {
    if ((*it)->email() == username)
      return true;
  }
  return false;
}

}  // namespace chromeos
