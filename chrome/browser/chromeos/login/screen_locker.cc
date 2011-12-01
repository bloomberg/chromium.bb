// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screen_locker.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/string_util.h"
#include "base/timer.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/dbus/dbus_thread_manager.h"
#include "chrome/browser/chromeos/dbus/power_manager_client.h"
#include "chrome/browser/chromeos/dbus/session_manager_client.h"
#include "chrome/browser/chromeos/input_method/input_method_manager.h"
#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "chrome/browser/chromeos/input_method/xkeyboard.h"
#include "chrome/browser/chromeos/language_preferences.h"
#include "chrome/browser/chromeos/login/authenticator.h"
#include "chrome/browser/chromeos/login/login_performer.h"
#include "chrome/browser/chromeos/login/login_utils.h"
#include "chrome/browser/chromeos/login/screen_locker_views.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/login/webui_screen_locker.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "content/browser/user_metrics.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "googleurl/src/gurl.h"
#include "grit/generated_resources.h"
#include "third_party/cros_system_api/window_manager/chromeos_wm_ipc_enums.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(TOOLKIT_USES_GTK)
#include "chrome/browser/chromeos/legacy_window_manager/wm_ipc.h"
#endif

using content::BrowserThread;

namespace {

// Observer to start ScreenLocker when the screen lock
class ScreenLockObserver : public chromeos::PowerManagerClient::Observer,
                           public content::NotificationObserver {
 public:
  ScreenLockObserver() {
    registrar_.Add(this, chrome::NOTIFICATION_LOGIN_USER_CHANGED,
                   content::NotificationService::AllSources());
  }

  // NotificationObserver overrides:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE {
    if (type == chrome::NOTIFICATION_LOGIN_USER_CHANGED) {
      // Register Screen Lock after login screen to make sure
      // we don't show the screen lock on top of the login screen by accident.
      chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->
          AddObserver(this);
    }
  }

  virtual void LockScreen() OVERRIDE {
    VLOG(1) << "In: ScreenLockObserver::LockScreen";
    SetupInputMethodsForScreenLocker();
    chromeos::ScreenLocker::Show();
  }

  virtual void UnlockScreen() OVERRIDE {
    RestoreInputMethods();
    chromeos::ScreenLocker::Hide();
  }

  virtual void UnlockScreenFailed() OVERRIDE {
    chromeos::ScreenLocker::UnlockScreenFailed();
  }

 private:
  // Temporarily deactivates all input methods (e.g. Chinese, Japanese, Arabic)
  // since they are not necessary to input a login password. Users are still
  // able to use/switch active keyboard layouts (e.g. US qwerty, US dvorak,
  // French).
  void SetupInputMethodsForScreenLocker() {
    if (// The LockScreen function is also called when the OS is suspended, and
        // in that case |saved_active_input_method_list_| might be non-empty.
        saved_active_input_method_list_.empty()) {
      chromeos::input_method::InputMethodManager* manager =
          chromeos::input_method::InputMethodManager::GetInstance();

      saved_previous_input_method_id_ = manager->previous_input_method().id();
      saved_current_input_method_id_ = manager->current_input_method().id();
      scoped_ptr<chromeos::input_method::InputMethodDescriptors>
          active_input_method_list(manager->GetActiveInputMethods());

      const std::string hardware_keyboard_id =
          manager->GetInputMethodUtil()->GetHardwareInputMethodId();
      // We'll add the hardware keyboard if it's not included in
      // |active_input_method_list| so that the user can always use the hardware
      // keyboard on the screen locker.
      bool should_add_hardware_keyboard = true;

      chromeos::input_method::ImeConfigValue value;
      value.type = chromeos::input_method::ImeConfigValue::kValueTypeStringList;
      for (size_t i = 0; i < active_input_method_list->size(); ++i) {
        const std::string& input_method_id =
            active_input_method_list->at(i).id();
        saved_active_input_method_list_.push_back(input_method_id);
        // Skip if it's not a keyboard layout.
        if (!chromeos::input_method::InputMethodUtil::IsKeyboardLayout(
                input_method_id))
          continue;
        value.string_list_value.push_back(input_method_id);
        if (input_method_id == hardware_keyboard_id) {
          should_add_hardware_keyboard = false;
        }
      }
      if (should_add_hardware_keyboard) {
        value.string_list_value.push_back(hardware_keyboard_id);
      }
      // We don't want to shut down the IME, even if the hardware layout is the
      // only IME left.
      manager->SetEnableAutoImeShutdown(false);
      manager->SetImeConfig(
          chromeos::language_prefs::kGeneralSectionName,
          chromeos::language_prefs::kPreloadEnginesConfigName,
          value);
    }
  }

  void RestoreInputMethods() {
    if (!saved_active_input_method_list_.empty()) {
      chromeos::input_method::InputMethodManager* manager =
          chromeos::input_method::InputMethodManager::GetInstance();

      chromeos::input_method::ImeConfigValue value;
      value.type = chromeos::input_method::ImeConfigValue::kValueTypeStringList;
      value.string_list_value = saved_active_input_method_list_;
      manager->SetEnableAutoImeShutdown(true);
      manager->SetImeConfig(
          chromeos::language_prefs::kGeneralSectionName,
          chromeos::language_prefs::kPreloadEnginesConfigName,
          value);
      // Send previous input method id first so Ctrl+space would work fine.
      if (!saved_previous_input_method_id_.empty())
        manager->ChangeInputMethod(saved_previous_input_method_id_);
      if (!saved_current_input_method_id_.empty())
        manager->ChangeInputMethod(saved_current_input_method_id_);

      saved_previous_input_method_id_.clear();
      saved_current_input_method_id_.clear();
      saved_active_input_method_list_.clear();
    }
  }

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

ScreenLocker::ScreenLocker(const User& user)
    : user_(user),
      // TODO(oshima): support auto login mode (this is not implemented yet)
      // http://crosbug.com/1881
      unlock_on_input_(user_.email().empty()),
      locked_(false),
      start_time_(base::Time::Now()),
      login_status_consumer_(NULL) {
  DCHECK(!screen_locker_);
  screen_locker_ = this;
}

void ScreenLocker::Init() {
  authenticator_ = LoginUtils::Get()->CreateAuthenticator(this);
  if (UseWebUILockScreen())
    delegate_.reset(new WebUIScreenLocker(this));
  else
    delegate_.reset(new ScreenLockerViews(this));
  delegate_->LockScreen(unlock_on_input_);
}

void ScreenLocker::OnLoginFailure(const LoginFailure& error) {
  DVLOG(1) << "OnLoginFailure";
  UserMetrics::RecordAction(UserMetricsAction("ScreenLocker_OnLoginFailure"));
  if (authentication_start_time_.is_null()) {
    LOG(ERROR) << "authentication_start_time_ is not set";
  } else {
    base::TimeDelta delta = base::Time::Now() - authentication_start_time_;
    VLOG(1) << "Authentication failure time: " << delta.InSecondsF();
    UMA_HISTOGRAM_TIMES("ScreenLocker.AuthenticationFailureTime", delta);
  }

  EnableInput();
  // Don't enable signout button here as we're showing
  // MessageBubble.

  string16 msg = l10n_util::GetStringUTF16(IDS_LOGIN_ERROR_AUTHENTICATING);
  const std::string error_text = error.GetErrorString();
  // TODO(ivankr): use a format string instead of concatenation.
  if (!error_text.empty())
    msg += ASCIIToUTF16("\n") + ASCIIToUTF16(error_text);

  // Display a warning if Caps Lock is on.
  if (input_method::XKeyboard::CapsLockIsEnabled()) {
    msg += ASCIIToUTF16("\n") +
        l10n_util::GetStringUTF16(IDS_LOGIN_ERROR_CAPS_LOCK_HINT);
  }

  input_method::InputMethodManager* input_method_manager =
      input_method::InputMethodManager::GetInstance();
  if (input_method_manager->GetNumActiveInputMethods() > 1)
    msg += ASCIIToUTF16("\n") +
        l10n_util::GetStringUTF16(IDS_LOGIN_ERROR_KEYBOARD_SWITCH_HINT);

  delegate_->ShowErrorMessage(msg, false);

  if (login_status_consumer_)
    login_status_consumer_->OnLoginFailure(error);
}

void ScreenLocker::OnLoginSuccess(
    const std::string& username,
    const std::string& password,
    const GaiaAuthConsumer::ClientLoginResult& unused,
    bool pending_requests,
    bool using_oauth) {
  VLOG(1) << "OnLoginSuccess: Sending Unlock request.";
  if (authentication_start_time_.is_null()) {
    if (!username.empty())
      LOG(WARNING) << "authentication_start_time_ is not set";
  } else {
    base::TimeDelta delta = base::Time::Now() - authentication_start_time_;
    VLOG(1) << "Authentication success time: " << delta.InSecondsF();
    UMA_HISTOGRAM_TIMES("ScreenLocker.AuthenticationSuccessTime", delta);
  }

  Profile* profile = ProfileManager::GetDefaultProfile();
  if (profile) {
    ProfileSyncService* service = profile->GetProfileSyncService(username);
    if (service && !service->HasSyncSetupCompleted()) {
      // If sync has failed somehow, try setting the sync passphrase here.
      service->SetPassphrase(password, false);
    }
  }
  DBusThreadManager::Get()->GetPowerManagerClient()->
      NotifyScreenUnlockRequested();

  if (login_status_consumer_)
    login_status_consumer_->OnLoginSuccess(username, password,
                                           unused, pending_requests,
                                           using_oauth);
}

void ScreenLocker::Authenticate(const string16& password) {
  authentication_start_time_ = base::Time::Now();
  delegate_->SetInputEnabled(false);
  delegate_->SetSignoutEnabled(false);
  delegate_->OnAuthenticate();

  // If LoginPerformer instance exists,
  // initial online login phase is still active.
  if (LoginPerformer::default_performer()) {
    DVLOG(1) << "Delegating authentication to LoginPerformer.";
    LoginPerformer::default_performer()->Login(user_.email(),
                                               UTF16ToUTF8(password));
  } else {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&Authenticator::AuthenticateToUnlock, authenticator_.get(),
                   user_.email(), UTF16ToUTF8(password)));
  }
}

void ScreenLocker::ShowCaptchaAndErrorMessage(const GURL& captcha_url,
                                              const string16& message) {
  delegate_->ShowCaptchaAndErrorMessage(captcha_url, message);
}

void ScreenLocker::ClearErrors() {
  delegate_->ClearErrors();
}

void ScreenLocker::EnableInput() {
  delegate_->SetInputEnabled(true);
}

void ScreenLocker::Signout() {
  // TODO(flackr): For proper functionality, check if (error_info) is NULL
  // (crbug.com/105267).
  delegate_->ClearErrors();
  UserMetrics::RecordAction(UserMetricsAction("ScreenLocker_Signout"));
#if defined(TOOLKIT_USES_GTK)
  WmIpc::instance()->NotifyAboutSignout();
#endif
  DBusThreadManager::Get()->GetSessionManagerClient()->StopSession();

  // Don't hide yet the locker because the chrome screen may become visible
  // briefly.
}

void ScreenLocker::ShowErrorMessage(const string16& message,
                                    bool sign_out_only) {
  delegate_->SetInputEnabled(!sign_out_only);
  delegate_->SetSignoutEnabled(sign_out_only);
  delegate_->ShowErrorMessage(message, sign_out_only);
}

void ScreenLocker::SetLoginStatusConsumer(
    chromeos::LoginStatusConsumer* consumer) {
  login_status_consumer_ = consumer;
}

// static
void ScreenLocker::Show() {
  VLOG(1) << "In ScreenLocker::Show";
  UserMetrics::RecordAction(UserMetricsAction("ScreenLocker_Show"));
  DCHECK(MessageLoop::current()->type() == MessageLoop::TYPE_UI);

  // Exit fullscreen.
  Browser* browser = BrowserList::GetLastActive();
  // browser can be NULL if we receive a lock request before the first browser
  // window is shown.
  if (browser && browser->window()->IsFullscreen()) {
    browser->ToggleFullscreenMode(false);
  }

  if (!screen_locker_) {
    VLOG(1) << "Show: Locking screen";
    ScreenLocker* locker =
        new ScreenLocker(UserManager::Get()->logged_in_user());
    locker->Init();
  } else {
    // PowerManager re-sends lock screen signal if it doesn't
    // receive the response within timeout. Just send complete
    // signal.
    VLOG(1) << "Show: locker already exists. Just sending completion event.";
    DBusThreadManager::Get()->GetPowerManagerClient()->
        NotifyScreenLockCompleted();
  }
}

// static
void ScreenLocker::Hide() {
  DCHECK(MessageLoop::current()->type() == MessageLoop::TYPE_UI);
  DCHECK(screen_locker_);
  VLOG(1) << "Hide: Deleting ScreenLocker: " << screen_locker_;
  MessageLoopForUI::current()->DeleteSoon(FROM_HERE, screen_locker_);
}

// static
void ScreenLocker::UnlockScreenFailed() {
  DCHECK(MessageLoop::current()->type() == MessageLoop::TYPE_UI);
  if (screen_locker_) {
    // Power manager decided no to unlock the screen even if a user
    // typed in password, for example, when a user closed the lid
    // immediately after typing in the password.
    VLOG(1) << "UnlockScreenFailed: re-enabling screen locker.";
    screen_locker_->EnableInput();
  } else {
    // This can happen when a user requested unlock, but PowerManager
    // rejected because the computer is closed, then PowerManager unlocked
    // because it's open again and the above failure message arrives.
    // This'd be extremely rare, but may still happen.
    VLOG(1) << "UnlockScreenFailed: screen is already unlocked.";
  }
}

// static
bool ScreenLocker::UseWebUILockScreen() {
  return !CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableWebUILockScreen);
}

// static
void ScreenLocker::InitClass() {
  g_screen_lock_observer.Get();
}

////////////////////////////////////////////////////////////////////////////////
// ScreenLocker, private:

ScreenLocker::~ScreenLocker() {
  DCHECK(MessageLoop::current()->type() == MessageLoop::TYPE_UI);
  ClearErrors();

  screen_locker_ = NULL;
  bool state = false;
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_SCREEN_LOCK_STATE_CHANGED,
      content::Source<ScreenLocker>(this),
      content::Details<bool>(&state));
  DBusThreadManager::Get()->GetPowerManagerClient()->
      NotifyScreenUnlockCompleted();
}

void ScreenLocker::SetAuthenticator(Authenticator* authenticator) {
  authenticator_ = authenticator;
}

void ScreenLocker::ScreenLockReady() {
  VLOG(1) << "ScreenLockReady: sending completed signal to power manager.";
  locked_ = true;
  base::TimeDelta delta = base::Time::Now() - start_time_;
  VLOG(1) << "Screen lock time: " << delta.InSecondsF();
  UMA_HISTOGRAM_TIMES("ScreenLocker.ScreenLockTime", delta);

  bool state = true;
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_SCREEN_LOCK_STATE_CHANGED,
      content::Source<ScreenLocker>(this),
      content::Details<bool>(&state));
  DBusThreadManager::Get()->GetPowerManagerClient()->
      NotifyScreenLockCompleted();
}

}  // namespace chromeos
