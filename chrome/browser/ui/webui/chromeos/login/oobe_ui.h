// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_OOBE_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_OOBE_UI_H_

#include <map>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/chromeos/login/ui/oobe_display.h"
#include "chrome/browser/chromeos/settings/shutdown_policy_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/core_oobe_handler.h"
#include "content/public/browser/web_ui_controller.h"

namespace base {
class DictionaryValue;
}  // namespace base

namespace chromeos {
class AppLaunchSplashScreenActor;
class AutoEnrollmentCheckScreenActor;
class BaseScreenHandler;
class ControllerPairingScreenActor;
class DeviceDisabledScreenActor;
class ErrorScreen;
class ErrorScreenHandler;
class GaiaScreenHandler;
class HostPairingScreenActor;
class KioskAppMenuHandler;
class KioskEnableScreenActor;
class LoginScreenContext;
class NativeWindowDelegate;
class NetworkDropdownHandler;
class NetworkStateInformer;
class SigninScreenHandler;
class SigninScreenHandlerDelegate;
class UserBoardScreenHandler;

// A custom WebUI that defines datasource for out-of-box-experience (OOBE) UI:
// - welcome screen (setup language/keyboard/network).
// - eula screen (CrOS (+ OEM) EULA content/TPM password/crash reporting).
// - update screen.
class OobeUI : public OobeDisplay,
               public content::WebUIController,
               public CoreOobeHandler::Delegate,
               public ShutdownPolicyHandler::Delegate {
 public:
  class Observer {
   public:
    virtual ~Observer() {}
    virtual void OnCurrentScreenChanged(
        Screen current_screen, Screen new_screen) = 0;
  };

  // List of known types of OobeUI. Type added as path in chrome://oobe url, for
  // example chrome://oobe/user-adding.
  static const char kOobeDisplay[];
  static const char kLoginDisplay[];
  static const char kLockDisplay[];
  static const char kUserAddingDisplay[];
  static const char kAppLaunchSplashDisplay[];

  // JS oobe/login screens names.
  static const char kScreenOobeHIDDetection[];
  static const char kScreenOobeNetwork[];
  static const char kScreenOobeEnableDebugging[];
  static const char kScreenOobeEula[];
  static const char kScreenOobeUpdate[];
  static const char kScreenOobeEnrollment[];
  static const char kScreenOobeReset[];
  static const char kScreenGaiaSignin[];
  static const char kScreenAccountPicker[];
  static const char kScreenKioskAutolaunch[];
  static const char kScreenKioskEnable[];
  static const char kScreenErrorMessage[];
  static const char kScreenUserImagePicker[];
  static const char kScreenTpmError[];
  static const char kScreenPasswordChanged[];
  static const char kScreenSupervisedUserCreationFlow[];
  static const char kScreenTermsOfService[];
  static const char kScreenWrongHWID[];
  static const char kScreenAutoEnrollmentCheck[];
  static const char kScreenAppLaunchSplash[];
  static const char kScreenConfirmPassword[];
  static const char kScreenFatalError[];
  static const char kScreenHIDDetection[];
  static const char kScreenControllerPairing[];
  static const char kScreenHostPairing[];
  static const char kScreenDeviceDisabled[];

  OobeUI(content::WebUI* web_ui, const GURL& url);
  ~OobeUI() override;

  // OobeDisplay implementation:
  CoreOobeActor* GetCoreOobeActor() override;
  NetworkView* GetNetworkView() override;
  EulaView* GetEulaView() override;
  UpdateView* GetUpdateView() override;
  EnableDebuggingScreenActor* GetEnableDebuggingScreenActor() override;
  EnrollmentScreenActor* GetEnrollmentScreenActor() override;
  ResetView* GetResetView() override;
  KioskAutolaunchScreenActor* GetKioskAutolaunchScreenActor() override;
  KioskEnableScreenActor* GetKioskEnableScreenActor() override;
  TermsOfServiceScreenActor* GetTermsOfServiceScreenActor() override;
  UserImageView* GetUserImageView() override;
  ErrorScreen* GetErrorScreen() override;
  WrongHWIDScreenActor* GetWrongHWIDScreenActor() override;
  AutoEnrollmentCheckScreenActor* GetAutoEnrollmentCheckScreenActor() override;
  SupervisedUserCreationScreenHandler* GetSupervisedUserCreationScreenActor()
      override;
  AppLaunchSplashScreenActor* GetAppLaunchSplashScreenActor() override;
  bool IsJSReady(const base::Closure& display_is_ready_callback) override;
  HIDDetectionView* GetHIDDetectionView() override;
  ControllerPairingScreenActor* GetControllerPairingScreenActor() override;
  HostPairingScreenActor* GetHostPairingScreenActor() override;
  DeviceDisabledScreenActor* GetDeviceDisabledScreenActor() override;
  GaiaScreenHandler* GetGaiaScreenActor() override;
  UserBoardView* GetUserBoardScreenActor() override;

  // ShutdownPolicyObserver::Delegate
  void OnShutdownPolicyChanged(bool reboot_on_shutdown) override;

  // Collects localized strings from the owned handlers.
  void GetLocalizedStrings(base::DictionaryValue* localized_strings);

  // Initializes the handlers.
  void InitializeHandlers();

  // Invoked after the async assets load. The screen handler that has the same
  // async assets load id will be initialized.
  void OnScreenAssetsLoaded(const std::string& async_assets_load_id);

  // Shows or hides OOBE UI elements.
  void ShowOobeUI(bool show);

  // Shows the signin screen.
  void ShowSigninScreen(const LoginScreenContext& context,
                        SigninScreenHandlerDelegate* delegate,
                        NativeWindowDelegate* native_window_delegate);

  // Resets the delegate set in ShowSigninScreen.
  void ResetSigninScreenHandlerDelegate();

  // Add and remove observers for screen change events.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  Screen current_screen() const { return current_screen_; }

  Screen previous_screen() const { return previous_screen_; }

  const std::string& display_type() const { return display_type_; }

  const std::string& GetScreenName(Screen screen) const;

  SigninScreenHandler* signin_screen_handler_for_test() {
    return signin_screen_handler_;
  }

  NetworkStateInformer* network_state_informer_for_test() const {
    return network_state_informer_.get();
  }

 private:
  // Initializes |screen_ids_| and |screen_names_| structures.
  void InitializeScreenMaps();

  void AddScreenHandler(BaseScreenHandler* handler);

  // CoreOobeHandler::Delegate implementation:
  void OnCurrentScreenChanged(const std::string& screen) override;

  // Type of UI.
  std::string display_type_;

  // Reference to NetworkStateInformer that handles changes in network
  // state.
  scoped_refptr<NetworkStateInformer> network_state_informer_;

  // Reference to CoreOobeHandler that handles common requests of Oobe page.
  CoreOobeHandler* core_handler_;

  // Reference to NetworkDropdownHandler that handles interaction with
  // network dropdown.
  NetworkDropdownHandler* network_dropdown_handler_;

  // Screens actors. Note, OobeUI owns them via |handlers_|, not directly here.
  UpdateView* update_view_;
  NetworkView* network_view_;
  EnableDebuggingScreenActor* debugging_screen_actor_;
  EulaView* eula_view_;
  EnrollmentScreenActor* enrollment_screen_actor_;
  ResetView* reset_view_;
  HIDDetectionView* hid_detection_view_;
  KioskAutolaunchScreenActor* autolaunch_screen_actor_;
  KioskEnableScreenActor* kiosk_enable_screen_actor_;
  WrongHWIDScreenActor* wrong_hwid_screen_actor_;
  AutoEnrollmentCheckScreenActor* auto_enrollment_check_screen_actor_;
  SupervisedUserCreationScreenHandler*
      supervised_user_creation_screen_actor_;
  AppLaunchSplashScreenActor* app_launch_splash_screen_actor_;
  ControllerPairingScreenActor* controller_pairing_screen_actor_;
  HostPairingScreenActor* host_pairing_screen_actor_;
  DeviceDisabledScreenActor* device_disabled_screen_actor_;

  // Reference to ErrorScreenHandler that handles error screen
  // requests and forward calls from native code to JS side.
  ErrorScreenHandler* error_screen_handler_;

  // Reference to GaiaScreenHandler that handles gaia screen requests and
  // forwards calls from native code to JS side.
  GaiaScreenHandler* gaia_screen_handler_;

  // Reference to UserBoardScreenHandler, that allows to pick user on device
  // and attempt authentication.
  UserBoardScreenHandler* user_board_screen_handler_;

  // Reference to SigninScreenHandler that handles sign-in screen requests and
  // forwards calls from native code to JS side.
  SigninScreenHandler* signin_screen_handler_;

  TermsOfServiceScreenActor* terms_of_service_screen_actor_;
  UserImageView* user_image_view_;

  std::vector<BaseScreenHandler*> handlers_;  // Non-owning pointers.

  KioskAppMenuHandler* kiosk_app_menu_handler_;  // Non-owning pointers.

  scoped_ptr<ErrorScreen> error_screen_;

  // Id of the current oobe/login screen.
  Screen current_screen_;

  // Id of the previous oobe/login screen.
  Screen previous_screen_;

  // Maps JS screen names to screen ids.
  std::map<std::string, Screen> screen_ids_;

  // Maps screen ids to JS screen names.
  std::vector<std::string> screen_names_;

  std::vector<Screen> dim_overlay_screen_ids_;

  // Flag that indicates whether JS part is fully loaded and ready to accept
  // calls.
  bool ready_;

  // Callbacks to notify when JS part is fully loaded and ready to accept calls.
  std::vector<base::Closure> ready_callbacks_;

  // List of registered observers.
  base::ObserverList<Observer> observer_list_;

  // Observer of CrosSettings watching the kRebootOnShutdown policy.
  scoped_ptr<ShutdownPolicyHandler> shutdown_policy_handler_;

  DISALLOW_COPY_AND_ASSIGN(OobeUI);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_OOBE_UI_H_
