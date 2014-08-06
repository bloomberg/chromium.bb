// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_EXISTING_USER_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_EXISTING_USER_CONTROLLER_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"
#include "chrome/browser/chromeos/login/auth/login_performer.h"
#include "chrome/browser/chromeos/login/login_utils.h"
#include "chrome/browser/chromeos/login/ui/login_display.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "components/user_manager/user.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/gfx/rect.h"
#include "url/gurl.h"

namespace base {
class ListValue;
}

namespace chromeos {

class CrosSettings;
class LoginDisplayHost;
class UserContext;

namespace login {
class NetworkStateHelper;
}

// ExistingUserController is used to handle login when someone has
// already logged into the machine.
// To use ExistingUserController create an instance of it and invoke Init.
// When Init is called it creates LoginDisplay instance which encapsulates
// all login UI implementation.
// ExistingUserController maintains it's own life cycle and deletes itself when
// the user logs in (or chooses to see other settings).
class ExistingUserController : public LoginDisplay::Delegate,
                               public content::NotificationObserver,
                               public LoginPerformer::Delegate,
                               public LoginUtils::Delegate {
 public:
  // All UI initialization is deferred till Init() call.
  explicit ExistingUserController(LoginDisplayHost* host);
  virtual ~ExistingUserController();

  // Returns the current existing user controller if it has been created.
  static ExistingUserController* current_controller() {
    return current_controller_;
  }

  // Creates and shows login UI for known users.
  void Init(const user_manager::UserList& users);

  // Tells the controller to enter the Enterprise Enrollment screen when
  // appropriate.
  void DoAutoEnrollment();

  // Tells the controller to resume a pending login.
  void ResumeLogin();

  // Start the public session auto-login timer.
  void StartPublicSessionAutoLoginTimer();

  // Stop the public session auto-login timer when a login attempt begins.
  void StopPublicSessionAutoLoginTimer();

  // LoginDisplay::Delegate: implementation
  virtual void CancelPasswordChangedFlow() OVERRIDE;
  virtual void CreateAccount() OVERRIDE;
  virtual void CompleteLogin(const UserContext& user_context) OVERRIDE;
  virtual base::string16 GetConnectedNetworkName() OVERRIDE;
  virtual bool IsSigninInProgress() const OVERRIDE;
  virtual void Login(const UserContext& user_context,
                     const SigninSpecifics& specifics) OVERRIDE;
  virtual void MigrateUserData(const std::string& old_password) OVERRIDE;
  virtual void OnSigninScreenReady() OVERRIDE;
  virtual void OnStartEnterpriseEnrollment() OVERRIDE;
  virtual void OnStartKioskEnableScreen() OVERRIDE;
  virtual void OnStartKioskAutolaunchScreen() OVERRIDE;
  virtual void ResetPublicSessionAutoLoginTimer() OVERRIDE;
  virtual void ResyncUserData() OVERRIDE;
  virtual void SetDisplayEmail(const std::string& email) OVERRIDE;
  virtual void ShowWrongHWIDScreen() OVERRIDE;
  virtual void Signout() OVERRIDE;

  void LoginAsRetailModeUser();
  void LoginAsGuest();
  void LoginAsPublicSession(const UserContext& user_context);
  void LoginAsKioskApp(const std::string& app_id, bool diagnostic_mode);

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Set a delegate that we will pass AuthStatusConsumer events to.
  // Used for testing.
  void set_login_status_consumer(AuthStatusConsumer* consumer) {
    auth_status_consumer_ = consumer;
  }

  // Returns the LoginDisplay created and owned by this controller.
  // Used for testing.
  LoginDisplay* login_display() {
    return login_display_.get();
  }

  // Returns the LoginDisplayHost for this controller.
  LoginDisplayHost* login_display_host() {
    return host_;
  }

  // Returns value of LoginPerformer::auth_mode() (cached if performer is
  // destroyed).
  LoginPerformer::AuthorizationMode auth_mode() const;

  // Returns value of LoginPerformer::password_changed() (cached if performer is
  // destroyed).
  bool password_changed() const;

 private:
  friend class ExistingUserControllerTest;
  friend class ExistingUserControllerAutoLoginTest;
  friend class ExistingUserControllerPublicSessionTest;
  friend class MockLoginPerformerDelegate;

  // Retrieve public session auto-login policy and update the timer.
  void ConfigurePublicSessionAutoLogin();

  // Trigger public session auto-login.
  void OnPublicSessionAutoLoginTimerFire();

  // LoginPerformer::Delegate implementation:
  virtual void OnAuthFailure(const AuthFailure& error) OVERRIDE;
  virtual void OnAuthSuccess(const UserContext& user_context) OVERRIDE;
  virtual void OnOffTheRecordAuthSuccess() OVERRIDE;
  virtual void OnPasswordChangeDetected() OVERRIDE;
  virtual void WhiteListCheckFailed(const std::string& email) OVERRIDE;
  virtual void PolicyLoadFailed() OVERRIDE;
  virtual void OnOnlineChecked(
      const std::string& username, bool success) OVERRIDE;

  // LoginUtils::Delegate implementation:
  virtual void OnProfilePrepared(Profile* profile) OVERRIDE;

  // Called when device settings change.
  void DeviceSettingsChanged();

  // Starts WizardController with the specified screen.
  void ActivateWizard(const std::string& screen_name);

  // Returns corresponding native window.
  gfx::NativeWindow GetNativeWindow() const;

  // Adds first-time login URLs.
  void InitializeStartUrls() const;

  // Show error message. |error_id| error message ID in resources.
  // If |details| string is not empty, it specify additional error text
  // provided by authenticator, it is not localized.
  void ShowError(int error_id, const std::string& details);

  // Shows Gaia page because password change was detected.
  void ShowGaiaPasswordChanged(const std::string& username);

  // Handles result of ownership check and starts enterprise or kiosk enrollment
  // if applicable.
  void OnEnrollmentOwnershipCheckCompleted(
      DeviceSettingsService::OwnershipStatus status);

  // Handles result of consumer kiosk configurability check and starts
  // enable kiosk screen if applicable.
  void OnConsumerKioskAutoLaunchCheckCompleted(
      KioskAppManager::ConsumerKioskAutoLaunchStatus status);

  // Enters the enterprise enrollment screen. |forced| is true if this is the
  // result of an auto-enrollment check, and the user shouldn't be able to
  // easily cancel the enrollment. In that case, |user| is the user name that
  // first logged in.
  void ShowEnrollmentScreen(bool forced, const std::string& user);

  // Shows "reset device" screen.
  void ShowResetScreen();

  // Shows kiosk feature enable screen.
  void ShowKioskEnableScreen();

  // Shows "kiosk auto-launch permission" screen.
  void ShowKioskAutolaunchScreen();

  // Shows "critical TPM error" screen.
  void ShowTPMError();

  // Invoked to complete login. Login might be suspended if auto-enrollment
  // has to be performed, and will resume once auto-enrollment completes.
  void CompleteLoginInternal(
      const UserContext& user_context,
      DeviceSettingsService::OwnershipStatus ownership_status);

  // Creates |login_performer_| if necessary and calls login() on it.
  // The string arguments aren't passed by const reference because this is
  // posted as |resume_login_callback_| and resets it.
  void PerformLogin(const UserContext& user_context,
                    LoginPerformer::AuthorizationMode auth_mode);

  // Updates the |login_display_| attached to this controller.
  void UpdateLoginDisplay(const user_manager::UserList& users);

  // Sends an accessibility alert event to extension listeners.
  void SendAccessibilityAlert(const std::string& alert_text);

  // Callback invoked when the keyboard layouts available for a public session
  // have been retrieved. Selects the first layout from the list and continues
  // login.
  void SetPublicSessionKeyboardLayoutAndLogin(
      const UserContext& user_context,
      scoped_ptr<base::ListValue> keyboard_layouts);

  // Starts the actual login process for a public session. Invoked when all
  // preconditions have been verified.
  void LoginAsPublicSessionInternal(const UserContext& user_context);

  // Public session auto-login timer.
  scoped_ptr<base::OneShotTimer<ExistingUserController> > auto_login_timer_;

  // Public session auto-login timeout, in milliseconds.
  int public_session_auto_login_delay_;

  // Username for public session auto-login.
  std::string public_session_auto_login_username_;

  // Used to execute login operations.
  scoped_ptr<LoginPerformer> login_performer_;

  // Delegate to forward all authentication status events to.
  // Tests can use this to receive authentication status events.
  AuthStatusConsumer* auth_status_consumer_;

  // Username of the last login attempt.
  std::string last_login_attempt_username_;

  // OOBE/login display host.
  LoginDisplayHost* host_;

  // Login UI implementation instance.
  scoped_ptr<LoginDisplay> login_display_;

  // Number of login attempts. Used to show help link when > 1 unsuccessful
  // logins for the same user.
  size_t num_login_attempts_;

  // Pointer to the current instance of the controller to be used by
  // automation tests.
  static ExistingUserController* current_controller_;

  // Interface to the signed settings store.
  CrosSettings* cros_settings_;

  // URL to append to start Guest mode with.
  GURL guest_mode_url_;

  // Used for notifications during the login process.
  content::NotificationRegistrar registrar_;

  // Factory of callbacks.
  base::WeakPtrFactory<ExistingUserController> weak_factory_;

  // The displayed email for the next login attempt set by |SetDisplayEmail|.
  std::string display_email_;

  // Whether offline login attempt failed.
  bool offline_failed_;

  // Whether login attempt is running.
  bool is_login_in_progress_;

  // Whether online login attempt succeeded.
  std::string online_succeeded_for_;

  // True if password has been changed for user who is completing sign in.
  // Set in OnLoginSuccess. Before that use LoginPerformer::password_changed().
  bool password_changed_;

  // Set in OnLoginSuccess. Before that use LoginPerformer::auth_mode().
  // Initialized with AUTH_MODE_EXTENSION as more restricted mode.
  LoginPerformer::AuthorizationMode auth_mode_;

  // True if auto-enrollment should be performed before starting the user's
  // session.
  bool do_auto_enrollment_;

  // Whether the sign-in UI is finished loading.
  bool signin_screen_ready_;

  // The username used for auto-enrollment, if it was triggered.
  std::string auto_enrollment_username_;

  // Callback to invoke to resume login, after auto-enrollment has completed.
  base::Closure resume_login_callback_;

  // Time when the signin screen was first displayed. Used to measure the time
  // from showing the screen until a successful login is performed.
  base::Time time_init_;

  // Timer for the interval to wait for the reboot after TPM error UI was shown.
  base::OneShotTimer<ExistingUserController> reboot_timer_;

  scoped_ptr<login::NetworkStateHelper> network_state_helper_;

  scoped_ptr<CrosSettings::ObserverSubscription> show_user_names_subscription_;
  scoped_ptr<CrosSettings::ObserverSubscription> allow_new_user_subscription_;
  scoped_ptr<CrosSettings::ObserverSubscription>
      allow_supervised_user_subscription_;
  scoped_ptr<CrosSettings::ObserverSubscription> allow_guest_subscription_;
  scoped_ptr<CrosSettings::ObserverSubscription> users_subscription_;
  scoped_ptr<CrosSettings::ObserverSubscription>
      local_account_auto_login_id_subscription_;
  scoped_ptr<CrosSettings::ObserverSubscription>
      local_account_auto_login_delay_subscription_;

  FRIEND_TEST_ALL_PREFIXES(ExistingUserControllerTest, ExistingUserLogin);

  DISALLOW_COPY_AND_ASSIGN(ExistingUserController);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_EXISTING_USER_CONTROLLER_H_
