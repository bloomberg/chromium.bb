// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_LOGIN_SCREEN_CONTROLLER_H_
#define ASH_LOGIN_LOGIN_SCREEN_CONTROLLER_H_

#include <vector>

#include "ash/ash_export.h"
#include "ash/login/ui/login_data_dispatcher.h"
#include "ash/public/cpp/kiosk_app_menu.h"
#include "ash/public/cpp/login_screen.h"
#include "ash/public/cpp/system_tray_focus_observer.h"
#include "ash/public/interfaces/login_screen.mojom.h"
#include "base/macros.h"
#include "base/optional.h"
#include "mojo/public/cpp/bindings/binding_set.h"

class PrefRegistrySimple;

namespace ash {

class ParentAccessWidget;
class SystemTrayNotifier;

// LoginScreenController implements mojom::LoginScreen and wraps the
// mojom::LoginScreenClient interface. This lets a consumer of ash provide a
// LoginScreenClient, which we will dispatch to if one has been provided to us.
// This could send requests to LoginScreenClient and also handle requests from
// LoginScreenClient through mojo.
class ASH_EXPORT LoginScreenController : public mojom::LoginScreen,
                                         public LoginScreen,
                                         public KioskAppMenu,
                                         public SystemTrayFocusObserver {
 public:
  // The current authentication stage. Used to get more verbose logging.
  enum class AuthenticationStage {
    kIdle,
    kDoAuthenticate,
    kUserCallback,
  };

  using OnShownCallback = base::OnceCallback<void(bool did_show)>;
  // Callback for authentication checks. |success| is nullopt if an
  // authentication check did not run, otherwise it is true/false if auth
  // succeeded/failed.
  using OnAuthenticateCallback =
      base::OnceCallback<void(base::Optional<bool> success)>;
  // Callback for parent access code validation. |success| is nullopt if
  // validation did not run, otherwise it contains validation result.
  using OnParentAccessValidation =
      base::OnceCallback<void(base::Optional<bool> success)>;

  explicit LoginScreenController(SystemTrayNotifier* system_tray_notifier);
  ~LoginScreenController() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry, bool for_test);

  // Binds the mojom::LoginScreen interface to this object.
  void BindRequest(mojom::LoginScreenRequest request);

  // Check to see if an authentication attempt is in-progress.
  bool IsAuthenticating() const;

  // Hash the password and send AuthenticateUser request to LoginScreenClient.
  // LoginScreenClient (in the chrome process) will do the authentication and
  // request to show error messages in the screen if auth fails, or request to
  // clear errors if auth succeeds.
  void AuthenticateUserWithPasswordOrPin(const AccountId& account_id,
                                         const std::string& password,
                                         bool authenticated_by_pin,
                                         OnAuthenticateCallback callback);
  void AuthenticateUserWithExternalBinary(const AccountId& account_id,
                                          OnAuthenticateCallback callback);
  void EnrollUserWithExternalBinary(OnAuthenticateCallback callback);
  void AuthenticateUserWithEasyUnlock(const AccountId& account_id);
  void ValidateParentAccessCode(const AccountId& account_id,
                                const std::string& code,
                                OnParentAccessValidation callback);
  void HardlockPod(const AccountId& account_id);
  void OnFocusPod(const AccountId& account_id);
  void OnNoPodFocused();
  void LoadWallpaper(const AccountId& account_id);
  void SignOutUser();
  void CancelAddUser();
  void LoginAsGuest();
  void OnMaxIncorrectPasswordAttempted(const AccountId& account_id);
  void FocusLockScreenApps(bool reverse);
  void ShowGaiaSignin(bool can_close,
                      const base::Optional<AccountId>& prefilled_account);
  void OnRemoveUserWarningShown();
  void RemoveUser(const AccountId& account_id);
  void LaunchPublicSession(const AccountId& account_id,
                           const std::string& locale,
                           const std::string& input_method);
  void RequestPublicSessionKeyboardLayouts(const AccountId& account_id,
                                           const std::string& locale);
  void ShowFeedback();
  void ShowResetScreen();
  void ShowAccountAccessHelpApp();
  void FocusOobeDialog();
  void NotifyUserActivity();

  // Enable or disable authentication for the debug overlay.
  enum class ForceFailAuth { kOff, kImmediate, kDelayed };
  void set_force_fail_auth_for_debug_overlay(ForceFailAuth force_fail) {
    force_fail_auth_for_debug_overlay_ = force_fail;
  }

  // LoginScreen:
  LoginScreenModel* GetModel() override;
  void ShowParentAccessWidget(
      const AccountId& child_account_id,
      base::RepeatingCallback<void(bool success)> callback) override;

  // mojom::LoginScreen:
  void SetClient(mojom::LoginScreenClientPtr client) override;
  void ShowLockScreen(ShowLockScreenCallback on_shown) override;
  void ShowLoginScreen(ShowLoginScreenCallback on_shown) override;
  void ShowErrorMessage(int32_t login_attempts,
                        const std::string& error_text,
                        const std::string& help_link_text,
                        int32_t help_topic_id) override;
  void ClearErrors() override;
  void IsReadyForPassword(IsReadyForPasswordCallback callback) override;
  void ShowKioskAppError(const std::string& message) override;
  void SetAddUserButtonEnabled(bool enable) override;
  void SetShutdownButtonEnabled(bool enable) override;
  void SetAllowLoginAsGuest(bool allow_guest) override;
  void SetShowGuestButtonInOobe(bool show) override;
  void SetShowParentAccessButton(bool show) override;
  void SetShowParentAccessDialog(bool show) override;
  void FocusLoginShelf(bool reverse) override;

  // KioskAppMenu:
  void SetKioskApps(
      const std::vector<KioskAppMenuEntry>& kiosk_apps,
      const base::RepeatingCallback<void(const KioskAppMenuEntry&)>& launch_app)
      override;

  // Flushes the mojo pipes - to be used in tests.
  void FlushForTesting();

  AuthenticationStage authentication_stage() const {
    return authentication_stage_;
  }

  LoginDataDispatcher* data_dispatcher() { return &login_data_dispatcher_; }

 private:
  void OnAuthenticateComplete(OnAuthenticateCallback callback, bool success);
  void OnParentAccessValidationComplete(OnParentAccessValidation callback,
                                        bool success);

  // Common code that is called when the login/lock screen is shown.
  void OnShow();

  // SystemTrayFocusObserver:
  void OnFocusLeavingSystemTray(bool reverse) override;

  LoginDataDispatcher login_data_dispatcher_;

  // Client interface in chrome browser. May be null in tests.
  mojom::LoginScreenClientPtr login_screen_client_;

  // Bindings for users of the LockScreen interface.
  mojo::BindingSet<mojom::LoginScreen> bindings_;

  AuthenticationStage authentication_stage_ = AuthenticationStage::kIdle;

  SystemTrayNotifier* system_tray_notifier_;

  // If set to false, all auth requests will forcibly fail.
  ForceFailAuth force_fail_auth_for_debug_overlay_ = ForceFailAuth::kOff;

  std::unique_ptr<ParentAccessWidget> parent_access_widget_;

  base::WeakPtrFactory<LoginScreenController> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(LoginScreenController);
};

}  // namespace ash

#endif  // ASH_LOGIN_LOGIN_SCREEN_CONTROLLER_H_
