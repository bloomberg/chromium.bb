// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_SIGNIN_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_SIGNIN_SCREEN_HANDLER_H_

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
#include "chrome/browser/chromeos/login/login_display.h"
#include "chrome/browser/chromeos/login/screens/error_screen_actor.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/net/network_portal_detector.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/network_state_informer.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
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
class LocallyManagedUserCreationScreenHandler;
class NativeWindowDelegate;
class User;
struct UserContext;

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
  virtual void OnLoginSuccess(const std::string& username) = 0;
  virtual void OnUserRemoved(const std::string& username) = 0;
  virtual void OnUserImageChanged(const User& user) = 0;
  virtual void OnPreferencesChanged() = 0;
  virtual void ResetSigninScreenHandlerDelegate() = 0;
  virtual void ShowBannerMessage(const std::string& message) = 0;
  virtual void ShowUserPodButton(const std::string& username,
                                 const std::string& iconURL,
                                 const base::Closure& click_callback) = 0;
  virtual void HideUserPodButton(const std::string& username) = 0;
  virtual void SetAuthType(const std::string& username,
                           LoginDisplay::AuthType auth_type,
                           const std::string& initial_value) = 0;
  virtual LoginDisplay::AuthType GetAuthType(const std::string& username)
      const = 0;
  virtual void ShowError(int login_attempts,
                         const std::string& error_text,
                         const std::string& help_link_text,
                         HelpAppLauncher::HelpTopic help_topic_id) = 0;
  virtual void ShowErrorScreen(LoginDisplay::SigninError error_id) = 0;
  virtual void ShowGaiaPasswordChanged(const std::string& username) = 0;
  virtual void ShowSigninUI(const std::string& email) = 0;
  virtual void ShowControlBar(bool show) = 0;
  virtual void ShowPasswordChangedDialog(bool show_password_error) = 0;
  // Show sign-in screen for the given credentials.
  virtual void ShowSigninScreenForCreds(const std::string& username,
                                        const std::string& password) = 0;
 protected:
  virtual ~LoginDisplayWebUIHandler() {}
};

// An interface for SigninScreenHandler to call WebUILoginDisplay.
class SigninScreenHandlerDelegate {
 public:
  // Cancels current password changed flow.
  virtual void CancelPasswordChangedFlow() = 0;

  // Cancels user adding.
  virtual void CancelUserAdding() = 0;

  // Create a new Google account.
  virtual void CreateAccount() = 0;

  // Confirms sign up by provided credentials in |user_context|.
  // Used for new user login via GAIA extension.
  virtual void CompleteLogin(const UserContext& user_context) = 0;

  // Sign in using username and password specified as a part of |user_context|.
  // Used for both known and new users.
  virtual void Login(const UserContext& user_context) = 0;

  // Sign in into a retail mode session.
  virtual void LoginAsRetailModeUser() = 0;

  // Sign in into guest session.
  virtual void LoginAsGuest() = 0;

  // Sign in into the public account identified by |username|.
  virtual void LoginAsPublicAccount(const std::string& username) = 0;

  // Decrypt cryptohome using user provided |old_password|
  // and migrate to new password.
  virtual void MigrateUserData(const std::string& old_password) = 0;

  // Load wallpaper for given |username|.
  virtual void LoadWallpaper(const std::string& username) = 0;

  // Loads the default sign-in wallpaper.
  virtual void LoadSigninWallpaper() = 0;

  // Notify the delegate when the sign-in UI is finished loading.
  virtual void OnSigninScreenReady() = 0;

  // Attempts to remove given user.
  virtual void RemoveUser(const std::string& username) = 0;

  // Ignore password change, remove existing cryptohome and
  // force full sync of user data.
  virtual void ResyncUserData() = 0;

  // Shows Enterprise Enrollment screen.
  virtual void ShowEnterpriseEnrollmentScreen() = 0;

  // Shows Kiosk Enable screen.
  virtual void ShowKioskEnableScreen() = 0;

  // Shows Reset screen.
  virtual void ShowResetScreen() = 0;

  // Shows Reset screen.
  virtual void ShowKioskAutolaunchScreen() = 0;

  // Show wrong hwid screen.
  virtual void ShowWrongHWIDScreen() = 0;

  // Let the delegate know about the handler it is supposed to be using.
  virtual void SetWebUIHandler(LoginDisplayWebUIHandler* webui_handler) = 0;

  // Returns users list to be shown.
  virtual const UserList& GetUsers() const = 0;

  // Whether login as guest is available.
  virtual bool IsShowGuest() const = 0;

  // Whether login as guest is available.
  virtual bool IsShowUsers() const = 0;

  // Whether new user pod is available.
  virtual bool IsShowNewUser() const = 0;

  // Returns true if sign in is in progress.
  virtual bool IsSigninInProgress() const = 0;

  // Whether user sign in has completed.
  virtual bool IsUserSigninCompleted() const = 0;

  // Sets the displayed email for the next login attempt. If it succeeds,
  // user's displayed email value will be updated to |email|.
  virtual void SetDisplayEmail(const std::string& email) = 0;

  // Signs out if the screen is currently locked.
  virtual void Signout() = 0;

  // Login to kiosk mode for app with |app_id|.
  virtual void LoginAsKioskApp(const std::string& app_id,
                               bool diagnostic_mode) = 0;

 protected:
  virtual ~SigninScreenHandlerDelegate() {}
};

// A class that handles the WebUI hooks in sign-in screen in OobeDisplay
// and LoginDisplay.
class SigninScreenHandler
    : public BaseScreenHandler,
      public LoginDisplayWebUIHandler,
      public content::NotificationObserver,
      public ui::EventHandler,
      public NetworkStateInformer::NetworkStateInformerObserver {
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

  friend class ReportDnsCacheClearedOnUIThread;
  friend class LocallyManagedUserCreationScreenHandler;

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
  virtual void OnLoginSuccess(const std::string& username) OVERRIDE;
  virtual void OnUserRemoved(const std::string& username) OVERRIDE;
  virtual void OnUserImageChanged(const User& user) OVERRIDE;
  virtual void OnPreferencesChanged() OVERRIDE;
  virtual void ResetSigninScreenHandlerDelegate() OVERRIDE;
  virtual void ShowBannerMessage(const std::string& message) OVERRIDE;
  virtual void ShowUserPodButton(const std::string& username,
                                 const std::string& iconURL,
                                 const base::Closure& click_callback) OVERRIDE;
  virtual void HideUserPodButton(const std::string& username) OVERRIDE;
  virtual void SetAuthType(const std::string& username,
                           LoginDisplay::AuthType auth_type,
                           const std::string& initial_value) OVERRIDE;
  virtual LoginDisplay::AuthType GetAuthType(const std::string& username)
      const OVERRIDE;
  virtual void ShowError(int login_attempts,
                         const std::string& error_text,
                         const std::string& help_link_text,
                         HelpAppLauncher::HelpTopic help_topic_id) OVERRIDE;
  virtual void ShowGaiaPasswordChanged(const std::string& username) OVERRIDE;
  virtual void ShowSigninUI(const std::string& email) OVERRIDE;
  virtual void ShowControlBar(bool show) OVERRIDE;
  virtual void ShowPasswordChangedDialog(bool show_password_error) OVERRIDE;
  virtual void ShowErrorScreen(LoginDisplay::SigninError error_id) OVERRIDE;
  virtual void ShowSigninScreenForCreds(const std::string& username,
                                        const std::string& password) OVERRIDE;

  // ui::EventHandler implementation:
  virtual void OnKeyEvent(ui::KeyEvent* key) OVERRIDE;

  // content::NotificationObserver implementation:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Shows signin screen after dns cache and cookie cleanup operations finish.
  void ShowSigninScreenIfReady();

  // Tells webui to load authentication extension. |force| is used to force the
  // extension reloading, if it has already been loaded. |silent_load| is true
  // for cases when extension should be loaded in the background and it
  // shouldn't grab the focus. |offline| is true when offline version of the
  // extension should be used.
  void LoadAuthExtension(bool force, bool silent_load, bool offline);

  // Updates authentication extension. Called when device settings that affect
  // sign-in (allow BWSI and allow whitelist) are changed.
  void UserSettingsChanged();
  void UpdateAddButtonStatus();

  // Restore input focus to current user pod.
  void RefocusCurrentPod();

  // WebUI message handlers.
  void HandleCompleteAuthentication(const std::string& email,
                                    const std::string& password,
                                    const std::string& auth_code);
  void HandleCompleteLogin(const std::string& typed_email,
                           const std::string& password,
                           bool using_saml);
  void HandleGetUsers();
  void HandleUsingSAMLAPI();
  void HandleScrapedPasswordCount(int password_count);
  void HandleScrapedPasswordVerificationFailed();
  void HandleAuthenticateUser(const std::string& username,
                              const std::string& password);
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
  void HandleLoginWebuiReady();
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
  void HandleShowLocallyManagedUserCreationScreen();
  void HandleFocusPod(const std::string& user_id);
  void HandleLaunchKioskApp(const std::string& app_id, bool diagnostic_mode);
  void HandleCustomButtonClicked(const std::string& username);
  void HandleRetrieveAuthenticatedUserEmail(double attempt_token);

  // Fills |user_dict| with information about |user|.
  static void FillUserDictionary(User* user,
                                 bool is_owner,
                                 bool is_signin_to_add,
                                 LoginDisplay::AuthType auth_type,
                                 base::DictionaryValue* user_dict);

  // Sends user list to account picker.
  void SendUserList(bool animated);

  // Kick off cookie / local storage cleanup.
  void StartClearingCookies(const base::Closure& on_clear_callback);
  void OnCookiesCleared(base::Closure on_clear_callback);

  // Kick off DNS cache flushing.
  void StartClearingDnsCache();
  void OnDnsCleared();

  // Decides whether an auth extension should be pre-loaded. If it should,
  // pre-loads it.
  void MaybePreloadAuthExtension();

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

  // Attempts login for test.
  void SubmitLoginFormForTest();

  // Update current input method (namely keyboard layout) to LRU by this user.
  void SetUserInputMethod(const std::string& username);

  // Invoked when auto enrollment check progresses to decide whether to
  // continue kiosk enable flow. Kiosk enable flow is resumed when
  // |state| indicates that enrollment is not applicable.
  void ContinueKioskEnableFlow(policy::AutoEnrollmentState state);

  // Shows signin screen for |email|.
  void OnShowAddUser(const std::string& email);

  // Updates the member variable and UMA histogram indicating whether the
  // principals API was used during SAML login.
  void SetSAMLPrincipalsAPIUsed(bool api_used);

  GaiaScreenHandler::FrameState FrameState() const;
  net::Error FrameError() const;

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

  // Is focus still stolen from Gaia page?
  bool focus_stolen_;

  // Has Gaia page silent load been started for the current sign-in attempt?
  bool gaia_silent_load_;

  // The active network at the moment when Gaia page was preloaded.
  std::string gaia_silent_load_network_;

  // Is account picker being shown for the first time.
  bool is_account_picker_showing_first_time_;

  // True if dns cache cleanup is done.
  bool dns_cleared_;

  // True if DNS cache task is already running.
  bool dns_clear_task_running_;

  // True if cookie jar cleanup is done.
  bool cookies_cleared_;

  // Network state informer used to keep signin screen up.
  scoped_refptr<NetworkStateInformer> network_state_informer_;

  // Email to pre-populate with.
  std::string email_;
  // Emails of the users, whose passwords have recently been changed.
  std::set<std::string> password_changed_for_;

  // If the user authenticated via SAML, this indicates whether the principals
  // API was used.
  bool using_saml_api_;

  // Test credentials.
  std::string test_user_;
  std::string test_pass_;
  bool test_expects_complete_login_;

  base::WeakPtrFactory<SigninScreenHandler> weak_factory_;

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

  scoped_ptr<CrosSettings::ObserverSubscription> allow_new_user_subscription_;
  scoped_ptr<CrosSettings::ObserverSubscription> allow_guest_subscription_;
  scoped_ptr<AutoEnrollmentController::ProgressCallbackList::Subscription>
      auto_enrollment_progress_subscription_;

  bool caps_lock_enabled_;

  base::Closure kiosk_enable_flow_aborted_callback_for_test_;

  // Map of callbacks run when the custom button on a user pod is clicked.
  std::map<std::string, base::Closure> user_pod_button_callback_map_;

  // Map of usernames to their current authentication type. If a user is not
  // contained in the map, it is using the default authentication type.
  std::map<std::string, LoginDisplay::AuthType> user_auth_type_map_;

  // Non-owning ptr.
  // TODO (ygorshenin@): remove this dependency.
  GaiaScreenHandler* gaia_screen_handler_;

  // Helper that retrieves the authenticated user's e-mail address.
  scoped_ptr<AuthenticatedUserEmailRetriever> email_retriever_;

  DISALLOW_COPY_AND_ASSIGN(SigninScreenHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_SIGNIN_SCREEN_HANDLER_H_
