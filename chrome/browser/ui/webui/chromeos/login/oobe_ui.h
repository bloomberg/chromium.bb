// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_OOBE_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_OOBE_UI_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "chrome/browser/chromeos/login/oobe_screen.h"
#include "chrome/browser/chromeos/settings/shutdown_policy_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/core_oobe_handler.h"
#include "content/public/browser/web_ui_controller.h"

namespace ash {
class ScreenDimmer;
}

namespace base {
class DictionaryValue;
}  // namespace base

namespace chromeos {
class AppLaunchSplashScreenView;
class ArcKioskSplashScreenView;
class ArcTermsOfServiceScreenView;
class AutoEnrollmentCheckScreenView;
class BaseScreenHandler;
class ControllerPairingScreenView;
class CoreOobeView;
class DeviceDisabledScreenView;
class EnableDebuggingScreenView;
class EnrollmentScreenView;
class EulaView;
class ErrorScreen;
class ErrorScreenHandler;
class GaiaView;
class GaiaScreenHandler;
class HIDDetectionView;
class HostPairingScreenView;
class KioskAppMenuHandler;
class KioskAutolaunchScreenView;
class KioskEnableScreenView;
class LoginScreenContext;
class NativeWindowDelegate;
class NetworkDropdownHandler;
class NetworkStateInformer;
class NetworkView;
class SigninScreenHandler;
class SigninScreenHandlerDelegate;
class SupervisedUserCreationScreenHandler;
class ResetView;
class TermsOfServiceScreenView;
class UserBoardScreenHandler;
class UserBoardView;
class UserImageView;
class UpdateView;
class WrongHWIDScreenView;

// A custom WebUI that defines datasource for out-of-box-experience (OOBE) UI:
// - welcome screen (setup language/keyboard/network).
// - eula screen (CrOS (+ OEM) EULA content/TPM password/crash reporting).
// - update screen.
class OobeUI : public content::WebUIController,
               public CoreOobeHandler::Delegate,
               public ShutdownPolicyHandler::Delegate {
 public:
  // List of known types of OobeUI. Type added as path in chrome://oobe url, for
  // example chrome://oobe/user-adding.
  static const char kOobeDisplay[];
  static const char kLoginDisplay[];
  static const char kLockDisplay[];
  static const char kUserAddingDisplay[];
  static const char kAppLaunchSplashDisplay[];
  static const char kArcKioskSplashDisplay[];

  class Observer {
   public:
    Observer() {}
    virtual void OnCurrentScreenChanged(OobeScreen current_screen,
                                        OobeScreen new_screen) = 0;

   protected:
    virtual ~Observer() {}
    DISALLOW_COPY(Observer);
  };

  OobeUI(content::WebUI* web_ui, const GURL& url);
  ~OobeUI() override;

  CoreOobeView* GetCoreOobeView();
  NetworkView* GetNetworkView();
  EulaView* GetEulaView();
  UpdateView* GetUpdateView();
  EnableDebuggingScreenView* GetEnableDebuggingScreenView();
  EnrollmentScreenView* GetEnrollmentScreenView();
  ResetView* GetResetView();
  KioskAutolaunchScreenView* GetKioskAutolaunchScreenView();
  KioskEnableScreenView* GetKioskEnableScreenView();
  TermsOfServiceScreenView* GetTermsOfServiceScreenView();
  ArcTermsOfServiceScreenView* GetArcTermsOfServiceScreenView();
  UserImageView* GetUserImageView();
  ErrorScreen* GetErrorScreen();
  WrongHWIDScreenView* GetWrongHWIDScreenView();
  AutoEnrollmentCheckScreenView* GetAutoEnrollmentCheckScreenView();
  SupervisedUserCreationScreenHandler* GetSupervisedUserCreationScreenView();
  AppLaunchSplashScreenView* GetAppLaunchSplashScreenView();
  ArcKioskSplashScreenView* GetArcKioskSplashScreenView();
  HIDDetectionView* GetHIDDetectionView();
  ControllerPairingScreenView* GetControllerPairingScreenView();
  HostPairingScreenView* GetHostPairingScreenView();
  DeviceDisabledScreenView* GetDeviceDisabledScreenView();
  GaiaView* GetGaiaScreenView();
  UserBoardView* GetUserBoardView();

  // ShutdownPolicyHandler::Delegate
  void OnShutdownPolicyChanged(bool reboot_on_shutdown) override;

  // Collects localized strings from the owned handlers.
  void GetLocalizedStrings(base::DictionaryValue* localized_strings);

  // Initializes the handlers.
  void InitializeHandlers();

  // Invoked after the async assets load. The screen handler that has the same
  // async assets load id will be initialized.
  void OnScreenAssetsLoaded(const std::string& async_assets_load_id);

  bool IsJSReady(const base::Closure& display_is_ready_callback);

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

  OobeScreen current_screen() const { return current_screen_; }

  OobeScreen previous_screen() const { return previous_screen_; }

  const std::string& display_type() const { return display_type_; }

  SigninScreenHandler* signin_screen_handler() {
    return signin_screen_handler_;
  }

  NetworkStateInformer* network_state_informer_for_test() const {
    return network_state_informer_.get();
  }

  // Does ReloadContent() if needed (for example, if material design mode has
  // changed).
  void UpdateLocalizedStringsIfNeeded();

 private:
  void AddScreenHandler(std::unique_ptr<BaseScreenHandler> handler);

  // CoreOobeHandler::Delegate implementation:
  void OnCurrentScreenChanged(OobeScreen screen) override;

  // Type of UI.
  std::string display_type_;

  // Reference to NetworkStateInformer that handles changes in network
  // state.
  scoped_refptr<NetworkStateInformer> network_state_informer_;

  // Reference to CoreOobeHandler that handles common requests of Oobe page.
  CoreOobeHandler* core_handler_ = nullptr;

  // Reference to NetworkDropdownHandler that handles interaction with
  // network dropdown.
  NetworkDropdownHandler* network_dropdown_handler_ = nullptr;

  // Screens views. Note, OobeUI owns them via |handlers_|, not directly here.
  UpdateView* update_view_ = nullptr;
  NetworkView* network_view_ = nullptr;
  EnableDebuggingScreenView* debugging_screen_view_ = nullptr;
  EulaView* eula_view_ = nullptr;
  EnrollmentScreenView* enrollment_screen_view_ = nullptr;
  ResetView* reset_view_ = nullptr;
  HIDDetectionView* hid_detection_view_ = nullptr;
  KioskAutolaunchScreenView* autolaunch_screen_view_ = nullptr;
  KioskEnableScreenView* kiosk_enable_screen_view_ = nullptr;
  WrongHWIDScreenView* wrong_hwid_screen_view_ = nullptr;
  AutoEnrollmentCheckScreenView* auto_enrollment_check_screen_view_ = nullptr;
  SupervisedUserCreationScreenHandler* supervised_user_creation_screen_view_ =
      nullptr;
  AppLaunchSplashScreenView* app_launch_splash_screen_view_ = nullptr;
  ArcKioskSplashScreenView* arc_kiosk_splash_screen_view_ = nullptr;
  ControllerPairingScreenView* controller_pairing_screen_view_ = nullptr;
  HostPairingScreenView* host_pairing_screen_view_ = nullptr;
  DeviceDisabledScreenView* device_disabled_screen_view_ = nullptr;

  // Reference to ErrorScreenHandler that handles error screen
  // requests and forward calls from native code to JS side.
  ErrorScreenHandler* error_screen_handler_ = nullptr;

  // Reference to GaiaScreenHandler that handles gaia screen requests and
  // forwards calls from native code to JS side.
  GaiaScreenHandler* gaia_screen_handler_ = nullptr;

  // Reference to UserBoardScreenHandler, that allows to pick user on device
  // and attempt authentication.
  UserBoardScreenHandler* user_board_screen_handler_ = nullptr;

  // Reference to SigninScreenHandler that handles sign-in screen requests and
  // forwards calls from native code to JS side.
  SigninScreenHandler* signin_screen_handler_ = nullptr;

  TermsOfServiceScreenView* terms_of_service_screen_view_ = nullptr;
  ArcTermsOfServiceScreenView* arc_terms_of_service_screen_view_ = nullptr;
  UserImageView* user_image_view_ = nullptr;

  std::vector<BaseScreenHandler*> handlers_;  // Non-owning pointers.

  KioskAppMenuHandler* kiosk_app_menu_handler_ =
      nullptr;  // Non-owning pointers.

  std::unique_ptr<ErrorScreen> error_screen_;

  // Id of the current oobe/login screen.
  OobeScreen current_screen_ = OobeScreen::SCREEN_UNKNOWN;

  // Id of the previous oobe/login screen.
  OobeScreen previous_screen_ = OobeScreen::SCREEN_UNKNOWN;

  // Flag that indicates whether JS part is fully loaded and ready to accept
  // calls.
  bool ready_ = false;

  // This flag stores material-design mode (on/off) of currently displayed UI.
  // If different version of UI is required, UI is updated.
  bool oobe_ui_md_mode_ = false;

  // Callbacks to notify when JS part is fully loaded and ready to accept calls.
  std::vector<base::Closure> ready_callbacks_;

  // List of registered observers.
  base::ObserverList<Observer> observer_list_;

  // Observer of CrosSettings watching the kRebootOnShutdown policy.
  std::unique_ptr<ShutdownPolicyHandler> shutdown_policy_handler_;

  std::unique_ptr<ash::ScreenDimmer> screen_dimmer_;

  // Store the deferred JS calls before the screen handler instance is
  // initialized.
  std::unique_ptr<JSCallsContainer> js_calls_container;

  DISALLOW_COPY_AND_ASSIGN(OobeUI);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_OOBE_UI_H_
