// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_SIGNIN_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_SIGNIN_SCREEN_HANDLER_H_

#include <map>
#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/containers/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/login/enrollment/auto_enrollment_controller.h"
#include "chrome/browser/chromeos/login/screens/error_screen_actor.h"
#include "chrome/browser/chromeos/login/signin_specifics.h"
#include "chrome/browser/chromeos/login/ui/login_display.h"
#include "chrome/browser/chromeos/login/users/user_manager.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/signin/screenlock_bridge.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/network_state_informer.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chromeos/ime/ime_keyboard.h"
#include "chromeos/network/portal_detector/network_portal_detector.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_ui.h"
#include "net/base/net_errors.h"
#include "ui/events/event_handler.h"

namespace base {
class DictionaryValue;
class ListValue;
}

namespace chromeos {

class AuthenticatedUserEmailRetriever;
class CaptivePortalWindowProxy;
class CoreOobeActor;
class GaiaScreenHandler;
class NativeWindowDelegate;
class SupervisedUserCreationScreenHandler;
class User;
class UserContext;

// Helper class to pass initial parameters to the login screen.
class LoginScreenContext {
 public:
  LoginScreenContext();
  explicit LoginScreenContext(const base::ListValue* args);

  void set_email(const std::string& email) { email_ = email; }
  const std::string& email() const { return email_; }

  void set_oobe_ui(bool oobe_ui) { oobe_ui_ = oobe_ui; }
  bool oobe_ui() const { return oobe_ui_; }

 private:
  void Init();

  std::string email_;
  bool oobe_ui_;
};

// An interface for WebUILoginDisplay to call SigninScreenHandler.
class LoginDisplayWebUIHandler {
 public:
  virtual void ClearAndEnablePassword() = 0;
  virtual void ClearUserPodPassword() = 0;
  virtual void OnUserRemoved(const std::string& username) = 0;
  virtual void OnUserImageChanged(const User& user) = 0;
  virtual void OnPreferencesChanged() = 0;
  virtual void ResetSigninScreenHandlerDelegate() = 0;
  virtual void ShowError(int login_attempts,
                         const std::string& error_text,
                         const std::string& help_link_text,
                         HelpAppLauncher::HelpTopic help_topic_id) = 0;
  virtual void ShowErrorScreen(LoginDisplay::SigninError error_id) = 0;
  virtual void ShowGaiaPasswordChanged(const std::string& username) = 0;
  virtual void ShowSigninUI(const std::string& email) = 0;
  virtual void ShowPasswordChangedDialog(bool show_password_error) = 0;
  // Show sign-in screen for the given credentials.
  virtual void ShowSigninScreenForCreds(const std::string& username,
                                        const std::string& password) = 0;
  virtual void LoadUsers(const base::ListValue& users_list,
                         bool show_guest) = 0;

 protected:
  virtual ~LoginDisplayWebUIHandler() {}
};

// An interface for SigninScreenHandler to call WebUILoginDisplay.
class SigninScreenHandlerDelegate {
 public:
  // --------------- Password change flow methods.
  // Cancels current password changed flow.
  virtual void CancelPasswordChangedFlow() = 0;

  // Decrypt cryptohome using user provided |old_password|
  // and migrate to new password.
  virtual void MigrateUserData(const std::string& old_password) = 0;

  // Ignore password change, remove existing cryptohome and
  // force full sync of user data.
  virtual void ResyncUserData() = 0;

  // --------------- Sign in/out methods.
  // Sign in using username and password specified as a part of |user_context|.
  // Used for both known and new users.
  virtual void Login(const UserContext& user_context,
                     const SigninSpecifics& specifics) = 0;

  // Sign in as guest to create a new Google account.
  virtual void CreateAccount() = 0;

  // Returns true if sign in is in progress.
  virtual bool IsSigninInProgress() const = 0;

  // Signs out if the screen is currently locked.
  virtual void Signout() = 0;

  // --------------- Account creation methods.
  // Confirms sign up by provided credentials in |user_context|.
  // Used for new user login via GAIA extension.
  virtual void CompleteLogin(const UserContext& user_context) = 0;

  // --------------- Shared with login display methods.
  // Notify the delegate when the sign-in UI is finished loading.
  virtual void OnSigninScreenReady() = 0;

  // Shows Enterprise Enrollment screen.
  virtual void ShowEnterpriseEnrollmentScreen() = 0;

  // Shows Kiosk Enable screen.
  virtual void ShowKioskEnableScreen() = 0;

  // Shows Reset screen.
  virtual void ShowKioskAutolaunchScreen() = 0;

  // Show wrong hwid screen.
  virtual void ShowWrongHWIDScreen() = 0;

  // Sets the displayed email for the next login attempt. If it succeeds,
  // user's displayed email value will be updated to |email|.
  virtual void SetDisplayEmail(const std::string& email) = 0;

  // --------------- Rest of the methods.
  // Cancels user adding.
  virtual void CancelUserAdding() = 0;

  // Load wallpaper for given |username|.
  virtual void LoadWallpaper(const std::string& username) = 0;

  // Loads the default sign-in wallpaper.
  virtual void LoadSigninWallpaper() = 0;

  // Attempts to remove given user.
  virtual void RemoveUser(const std::string& username) = 0;

  // Let the delegate know about the handler it is supposed to be using.
  virtual void SetWebUIHandler(LoginDisplayWebUIHandler* webui_handler) = 0;

  // Returns users list to be shown.
  virtual const UserList& GetUsers() const = 0;

  // Whether login as guest is available.
  virtual bool IsShowGuest() const = 0;

  // Weather to show the user pods or only GAIA sign in.
  // Public sessions are always shown.
  virtual bool IsShowUsers() const = 0;

  // Whether user sign in has completed.
  virtual bool IsUserSigninCompleted() const = 0;

  // Request to (re)load user list.
  virtual void HandleGetUsers() = 0;

  // Set authentication type (for easier unlocking).
  virtual void SetAuthType(
      const std::string& username,
      ScreenlockBridge::LockHandler::AuthType auth_type) = 0;

  // Get authentication type (for easier unlocking).
  virtual ScreenlockBridge::LockHandler::AuthType GetAuthType(
      const std::string& username) const = 0;

 protected:
  virtual ~SigninScreenHandlerDelegate() {}
};

// A class that handles the WebUI hooks in sign-in screen in OobeDisplay
// and LoginDisplay.
class SigninScreenHandler
    : public BaseScreenHandler,
      public LoginDisplayWebUIHandler,
      public content::NotificationObserver,
      public ScreenlockBridge::LockHandler,
      public NetworkStateInformer::NetworkStateInformerObserver,
      public input_method::ImeKeyboard::Observer {
 public:
  SigninScreenHandler(
      const scoped_refptr<NetworkStateInformer>& network_state_informer,
      ErrorScreenActor* error_screen_actor,
      CoreOobeActor* core_oobe_actor,
      GaiaScreenHandler* gaia_screen_handler);
  virtual ~SigninScreenHandler();

  // Shows the sign in screen.
  void Show(const LoginScreenContext& context);

  // Shows the login spinner UI for retail mode logins.
  void ShowRetailModeLoginSpinner();

  // Sets delegate to be used by the handler. It is guaranteed that valid
  // delegate is set before Show() method will be called.
  void SetDelegate(SigninScreenHandlerDelegate* delegate);

  void SetNativeWindowDelegate(NativeWindowDelegate* native_window_delegate);

  // NetworkStateInformer::NetworkStateInformerObserver implementation:
  virtual void OnNetworkReady() OVERRIDE;
  virtual void UpdateState(ErrorScreenActor::ErrorReason reason) OVERRIDE;

  // Required Local State preferences.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  void set_kiosk_enable_flow_aborted_callback_for_test(
      const base::Closure& callback) {
    kiosk_enable_flow_aborted_callback_for_test_ = callback;
  }

 private:
  enum UIState {
    UI_STATE_UNKNOWN = 0,
    UI_STATE_GAIA_SIGNIN,
    UI_STATE_ACCOUNT_PICKER,
  };

  friend class GaiaScreenHandler;
  friend class ReportDnsCacheClearedOnUIThread;
  friend class SupervisedUserCreationScreenHandler;

  void ShowImpl();

  // Updates current UI of the signin screen according to |ui_state|
  // argument.  Optionally it can pass screen initialization data via
  // |params| argument.
  void UpdateUIState(UIState ui_state, base::DictionaryValue* params);

  void UpdateStateInternal(ErrorScreenActor::ErrorReason reason,
                           bool force_update);
  void SetupAndShowOfflineMessage(NetworkStateInformer::State state,
                                  ErrorScreenActor::ErrorReason reason);
  void HideOfflineMessage(NetworkStateInformer::State state,
                          ErrorScreenActor::ErrorReason reason);
  void ReloadGaiaScreen();

  // BaseScreenHandler implementation:
  virtual void DeclareLocalizedValues(LocalizedValuesBuilder* builder) OVERRIDE;
  virtual void Initialize() OVERRIDE;
  virtual gfx::NativeWindow GetNativeWindow() OVERRIDE;

  // WebUIMessageHandler implementation:
  virtual void RegisterMessages() OVERRIDE;

  // LoginDisplayWebUIHandler implementation:
  virtual void ClearAndEnablePassword() OVERRIDE;
  virtual void ClearUserPodPassword() OVERRIDE;
  virtual void OnUserRemoved(const std::string& username) OVERRIDE;
  virtual void OnUserImageChanged(const User& user) OVERRIDE;
  virtual void OnPreferencesChanged() OVERRIDE;
  virtual void ResetSigninScreenHandlerDelegate() OVERRIDE;
  virtual void ShowError(int login_attempts,
                         const std::string& error_text,
                         const std::string& help_link_text,
                         HelpAppLauncher::HelpTopic help_topic_id) OVERRIDE;
  virtual void ShowGaiaPasswordChanged(const std::string& username) OVERRIDE;
  virtual void ShowSigninUI(const std::string& email) OVERRIDE;
  virtual void ShowPasswordChangedDialog(bool show_password_error) OVERRIDE;
  virtual void ShowErrorScreen(LoginDisplay::SigninError error_id) OVERRIDE;
  virtual void ShowSigninScreenForCreds(const std::string& username,
                                        const std::string& password) OVERRIDE;
  virtual void LoadUsers(const base::ListValue& users_list,
                         bool show_guest) OVERRIDE;

  // content::NotificationObserver implementation:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // ScreenlockBridge::LockHandler implementation:
  virtual void ShowBannerMessage(const std::string& message) OVERRIDE;
  virtual void ShowUserPodCustomIcon(const std::string& username,
                                     const gfx::Image& icon) OVERRIDE;
  virtual void HideUserPodCustomIcon(const std::string& username) OVERRIDE;
  virtual void EnableInput() OVERRIDE;
  virtual void SetAuthType(const std::string& username,
                           ScreenlockBridge::LockHandler::AuthType auth_type,
                           const std::string& initial_value) OVERRIDE;
  virtual ScreenlockBridge::LockHandler::AuthType GetAuthType(
      const std::string& username) const OVERRIDE;
  virtual void Unlock(const std::string& user_email) OVERRIDE;

  // Updates authentication extension. Called when device settings that affect
  // sign-in (allow BWSI and allow whitelist) are changed.
  void UserSettingsChanged();
  void UpdateAddButtonStatus();

  // Restore input focus to current user pod.
  void RefocusCurrentPod();

  // WebUI message handlers.
  void HandleGetUsers();
  void HandleAuthenticateUser(const std::string& username,
                              const std::string& password);
  void HandleAttemptUnlock(const std::string& username);
  void HandleLaunchDemoUser();
  void HandleLaunchIncognito();
  void HandleLaunchPublicAccount(const std::string& username);
  void HandleOfflineLogin(const base::ListValue* args);
  void HandleShutdownSystem();
  void HandleLoadWallpaper(const std::string& email);
  void HandleRebootSystem();
  void HandleRemoveUser(const std::string& email);
  void HandleShowAddUser(const base::ListValue* args);
  void HandleToggleEnrollmentScreen();
  void HandleToggleKioskEnableScreen();
  void HandleToggleResetScreen();
  void HandleToggleKioskAutolaunchScreen();
  void HandleCreateAccount();
  void HandleAccountPickerReady();
  void HandleWallpaperReady();
  void HandleSignOutUser();
  void HandleOpenProxySettings();
  void HandleLoginVisible(const std::string& source);
  void HandleCancelPasswordChangedFlow();
  void HandleCancelUserAdding();
  void HandleMigrateUserData(const std::string& password);
  void HandleResyncUserData();
  void HandleLoginUIStateChanged(const std::string& source, bool new_value);
  void HandleUnlockOnLoginSuccess();
  void HandleLoginScreenUpdate();
  void HandleShowLoadingTimeoutError();
  void HandleUpdateOfflineLogin(bool offline_login_active);
  void HandleShowSupervisedUserCreationScreen();
  void HandleFocusPod(const std::string& user_id);
  void HandleLaunchKioskApp(const std::string& app_id, bool diagnostic_mode);
  void HandleRetrieveAuthenticatedUserEmail(double attempt_token);


  // Returns true iff
  // (i)   log in is restricted to some user list,
  // (ii)  all users in the restricted list are present.
  bool AllWhitelistedUsersPresent();

  // Cancels password changed flow - switches back to login screen.
  // Called as a callback after cookies are cleared.
  void CancelPasswordChangedFlowInternal();

  // Returns current visible screen.
  OobeUI::Screen GetCurrentScreen() const;

  // Returns true if current visible screen is the Gaia sign-in page.
  bool IsGaiaVisible() const;

  // Returns true if current visible screen is the error screen over
  // Gaia sign-in page.
  bool IsGaiaHiddenByError() const;

  // Returns true if current screen is the error screen over signin
  // screen.
  bool IsSigninScreenHiddenByError() const;

  // Returns true if guest signin is allowed.
  bool IsGuestSigninAllowed() const;

  // Returns true if offline login is allowed.
  bool IsOfflineLoginAllowed() const;

  bool ShouldLoadGaia() const;

  // Update current input method (namely keyboard layout) to LRU by this user.
  void SetUserInputMethod(const std::string& username);

  // Invoked when auto enrollment check progresses to decide whether to
  // continue kiosk enable flow. Kiosk enable flow is resumed when
  // |state| indicates that enrollment is not applicable.
  void ContinueKioskEnableFlow(policy::AutoEnrollmentState state);

  // Shows signin.
  void OnShowAddUser();

  GaiaScreenHandler::FrameState FrameState() const;
  net::Error FrameError() const;

  // input_method::ImeKeyboard::Observer implementation:
  virtual void OnCapsLockChanged(bool enabled) OVERRIDE;

  // Current UI state of the signin screen.
  UIState ui_state_;

  // A delegate that glues this handler with backend LoginDisplay.
  SigninScreenHandlerDelegate* delegate_;

  // A delegate used to get gfx::NativeWindow.
  NativeWindowDelegate* native_window_delegate_;

  // Whether screen should be shown right after initialization.
  bool show_on_init_;

  // Keeps whether screen should be shown for OOBE.
  bool oobe_ui_;

  // Is account picker being shown for the first time.
  bool is_account_picker_showing_first_time_;

  // Network state informer used to keep signin screen up.
  scoped_refptr<NetworkStateInformer> network_state_informer_;

  // Set to true once |LOGIN_WEBUI_VISIBLE| notification is observed.
  bool webui_visible_;
  bool preferences_changed_delayed_;

  ErrorScreenActor* error_screen_actor_;
  CoreOobeActor* core_oobe_actor_;

  bool is_first_update_state_call_;
  bool offline_login_active_;
  NetworkStateInformer::State last_network_state_;

  base::CancelableClosure update_state_closure_;
  base::CancelableClosure connecting_closure_;

  content::NotificationRegistrar registrar_;

  // Whether there is an auth UI pending. This flag is set on receiving
  // NOTIFICATION_AUTH_NEEDED and reset on either NOTIFICATION_AUTH_SUPPLIED or
  // NOTIFICATION_AUTH_CANCELLED.
  bool has_pending_auth_ui_;

  scoped_ptr<AutoEnrollmentController::ProgressCallbackList::Subscription>
      auto_enrollment_progress_subscription_;

  bool caps_lock_enabled_;

  base::Closure kiosk_enable_flow_aborted_callback_for_test_;

  // Non-owning ptr.
  // TODO (ygorshenin@): remove this dependency.
  GaiaScreenHandler* gaia_screen_handler_;

  // Helper that retrieves the authenticated user's e-mail address.
  scoped_ptr<AuthenticatedUserEmailRetriever> email_retriever_;

  base::WeakPtrFactory<SigninScreenHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SigninScreenHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_SIGNIN_SCREEN_HANDLER_H_
