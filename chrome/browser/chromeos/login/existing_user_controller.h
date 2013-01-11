// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_EXISTING_USER_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_EXISTING_USER_CONTROLLER_H_

#include <string>

#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/string16.h"
#include "base/time.h"
#include "base/timer.h"
#include "chrome/browser/chromeos/login/login_display.h"
#include "chrome/browser/chromeos/login/login_performer.h"
#include "chrome/browser/chromeos/login/login_utils.h"
#include "chrome/browser/chromeos/login/password_changed_view.h"
#include "chrome/browser/chromeos/login/user.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "googleurl/src/gurl.h"
#include "ui/gfx/rect.h"

namespace chromeos {

class CrosSettings;
class LoginDisplayHost;

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
                               public LoginUtils::Delegate,
                               public PasswordChangedView::Delegate {
 public:
  // All UI initialization is deferred till Init() call.
  explicit ExistingUserController(LoginDisplayHost* host);
  virtual ~ExistingUserController();

  // Returns the current existing user controller if it has been created.
  static ExistingUserController* current_controller() {
    return current_controller_;
  }

  // Creates and shows login UI for known users.
  void Init(const UserList& users);

  // Tells the controller to enter the Enterprise Enrollment screen when
  // appropriate.
  void DoAutoEnrollment();

  // Tells the controller to resume a pending login.
  void ResumeLogin();

  // LoginDisplay::Delegate: implementation
  virtual void CancelPasswordChangedFlow() OVERRIDE;
  virtual void CreateAccount() OVERRIDE;
  virtual void CompleteLogin(const std::string& username,
                             const std::string& password) OVERRIDE;
  virtual string16 GetConnectedNetworkName() OVERRIDE;
  virtual void Login(const std::string& username,
                     const std::string& password) OVERRIDE;
  virtual void MigrateUserData(const std::string& old_password) OVERRIDE;
  virtual void LoginAsRetailModeUser() OVERRIDE;
  virtual void LoginAsGuest() OVERRIDE;
  virtual void LoginAsPublicAccount(const std::string& username) OVERRIDE;
  virtual void OnUserSelected(const std::string& username) OVERRIDE;
  virtual void OnStartEnterpriseEnrollment() OVERRIDE;
  virtual void OnStartDeviceReset() OVERRIDE;
  virtual void ResyncUserData() OVERRIDE;
  virtual void SetDisplayEmail(const std::string& email) OVERRIDE;
  virtual void Signout() OVERRIDE;

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Set a delegate that we will pass LoginStatusConsumer events to.
  // Used for testing.
  void set_login_status_consumer(LoginStatusConsumer* consumer) {
    login_status_consumer_ = consumer;
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

 private:
  friend class ExistingUserControllerTest;
  friend class MockLoginPerformerDelegate;

  // LoginPerformer::Delegate implementation:
  virtual void OnLoginFailure(const LoginFailure& error) OVERRIDE;
  virtual void OnLoginSuccess(
      const std::string& username,
      const std::string& password,
      bool pending_requests,
      bool using_oauth) OVERRIDE;
  virtual void OnOffTheRecordLoginSuccess() OVERRIDE;
  virtual void OnPasswordChangeDetected() OVERRIDE;
  virtual void WhiteListCheckFailed(const std::string& email) OVERRIDE;
  virtual void PolicyLoadFailed() OVERRIDE;
  virtual void OnOnlineChecked(
      const std::string& username, bool success) OVERRIDE;

  // LoginUtils::Delegate implementation:
  virtual void OnProfilePrepared(Profile* profile) OVERRIDE;

  // PasswordChangedView::Delegate:
  virtual void RecoverEncryptedData(const std::string& old_password) OVERRIDE;
  virtual void ResyncEncryptedData() OVERRIDE;

  // Starts WizardController with the specified screen.
  void ActivateWizard(const std::string& screen_name);

  // Returns corresponding native window.
  gfx::NativeWindow GetNativeWindow() const;

  // Adds first-time login URLs.
  void InitializeStartUrls() const;

  // Shows "Release Notes"/"What's new"/Getting started guide on update.
  void OptionallyShowReleaseNotes(Profile* profile) const;

  // Show error message. |error_id| error message ID in resources.
  // If |details| string is not empty, it specify additional error text
  // provided by authenticator, it is not localized.
  void ShowError(int error_id, const std::string& details);

  // Shows Gaia page because password change was detected.
  void ShowGaiaPasswordChanged(const std::string& username);

  // Handles result of ownership check and starts enterprise enrollment if
  // applicable.
  void OnEnrollmentOwnershipCheckCompleted(
      DeviceSettingsService::OwnershipStatus status,
      bool current_user_is_owner);

  // Enters the enterprise enrollment screen. |forced| is true if this is the
  // result of an auto-enrollment check, and the user shouldn't be able to
  // easily cancel the enrollment. In that case, |user| is the user name that
  // first logged in.
  void ShowEnrollmentScreen(bool forced, const std::string& user);

  // Shows "reset device" screen.
  void ShowResetScreen();

  // Shows "critical TPM error" screen and starts reboot timer.
  void ShowTPMErrorAndScheduleReboot();

  // Reboot timer handler.
  void OnRebootTimeElapsed();

  // Invoked to complete login. Login might be suspended if auto-enrollment
  // has to be performed, and will resume once auto-enrollment completes.
  void CompleteLoginInternal(
      const std::string& username,
      const std::string& password,
      DeviceSettingsService::OwnershipStatus ownership_status,
      bool is_owner);

  // Creates |login_performer_| if necessary and calls login() on it.
  // The string arguments aren't passed by const reference because this is
  // posted as |resume_login_callback_| and resets it.
  void PerformLogin(std::string username,
                    std::string password,
                    LoginPerformer::AuthorizationMode auth_mode);

  void set_login_performer_delegate(LoginPerformer::Delegate* d) {
    login_performer_delegate_.reset(d);
  }

  // Updates the |login_display_| attached to this controller.
  void UpdateLoginDisplay(const UserList& users);

  // Used to execute login operations.
  scoped_ptr<LoginPerformer> login_performer_;

  // Delegate for login performer to be overridden by tests.
  // |this| is used if |login_performer_delegate_| is NULL.
  scoped_ptr<LoginPerformer::Delegate> login_performer_delegate_;

  // Delegate to forward all login status events to.
  // Tests can use this to receive login status events.
  LoginStatusConsumer* login_status_consumer_;

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

  // Whether everything is ready to launch the browser.
  bool ready_for_browser_launch_;

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

  // True if auto-enrollment should be performed before starting the user's
  // session.
  bool do_auto_enrollment_;

  // The username used for auto-enrollment, if it was triggered.
  std::string auto_enrollment_username_;

  // Callback to invoke to resume login, after auto-enrollment has completed.
  base::Closure resume_login_callback_;

  // Time when the signin screen was first displayed. Used to measure the time
  // from showing the screen until a successful login is performed.
  base::Time time_init_;

  // Timer for the interval to wait for the reboot after TPM error UI was shown.
  base::OneShotTimer<ExistingUserController> reboot_timer_;

  FRIEND_TEST_ALL_PREFIXES(ExistingUserControllerTest, ExistingUserLogin);

  DISALLOW_COPY_AND_ASSIGN(ExistingUserController);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_EXISTING_USER_CONTROLLER_H_
