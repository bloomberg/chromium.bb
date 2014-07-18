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
#include "base/observer_list.h"
#include "chrome/browser/chromeos/login/ui/oobe_display.h"
#include "chrome/browser/ui/webui/chromeos/login/core_oobe_handler.h"
#include "content/public/browser/web_ui_controller.h"

namespace base {
class DictionaryValue;
}  // namespace base

namespace chromeos {
class AppLaunchSplashScreenActor;
class BaseScreenHandler;
class ControllerPairingScreenActor;
class ErrorScreenHandler;
class HostPairingScreenActor;
class KioskAppMenuHandler;
class KioskEnableScreenActor;
class LoginScreenContext;
class NativeWindowDelegate;
class NetworkDropdownHandler;
class NetworkStateInformer;
class GaiaScreenHandler;
class SigninScreenHandler;
class SigninScreenHandlerDelegate;
class UpdateScreenHandler;

// A custom WebUI that defines datasource for out-of-box-experience (OOBE) UI:
// - welcome screen (setup language/keyboard/network).
// - eula screen (CrOS (+ OEM) EULA content/TPM password/crash reporting).
// - update screen.
class OobeUI : public OobeDisplay,
               public content::WebUIController,
               public CoreOobeHandler::Delegate {
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

  OobeUI(content::WebUI* web_ui, const GURL& url);
  virtual ~OobeUI();

  // OobeDisplay implementation:
  virtual CoreOobeActor* GetCoreOobeActor() OVERRIDE;
  virtual UpdateScreenActor* GetUpdateScreenActor() OVERRIDE;
  virtual NetworkScreenActor* GetNetworkScreenActor() OVERRIDE;
  virtual EulaScreenActor* GetEulaScreenActor() OVERRIDE;
  virtual EnrollmentScreenActor* GetEnrollmentScreenActor() OVERRIDE;
  virtual ResetScreenActor* GetResetScreenActor() OVERRIDE;
  virtual KioskAutolaunchScreenActor* GetKioskAutolaunchScreenActor() OVERRIDE;
  virtual KioskEnableScreenActor* GetKioskEnableScreenActor() OVERRIDE;
  virtual TermsOfServiceScreenActor*
      GetTermsOfServiceScreenActor() OVERRIDE;
  virtual UserImageScreenActor* GetUserImageScreenActor() OVERRIDE;
  virtual ErrorScreenActor* GetErrorScreenActor() OVERRIDE;
  virtual WrongHWIDScreenActor* GetWrongHWIDScreenActor() OVERRIDE;
  virtual AutoEnrollmentCheckScreenActor*
      GetAutoEnrollmentCheckScreenActor() OVERRIDE;
  virtual SupervisedUserCreationScreenHandler*
      GetSupervisedUserCreationScreenActor() OVERRIDE;
  virtual AppLaunchSplashScreenActor*
      GetAppLaunchSplashScreenActor() OVERRIDE;
  virtual bool IsJSReady(const base::Closure& display_is_ready_callback)
      OVERRIDE;
  virtual HIDDetectionScreenActor* GetHIDDetectionScreenActor() OVERRIDE;
  virtual ControllerPairingScreenActor* GetControllerPairingScreenActor()
      OVERRIDE;
  virtual HostPairingScreenActor* GetHostPairingScreenActor() OVERRIDE;

  // Collects localized strings from the owned handlers.
  void GetLocalizedStrings(base::DictionaryValue* localized_strings);

  // Initializes the handlers.
  void InitializeHandlers();

  // Invoked after the async assets load. The screen handler that has the same
  // async assets load id will be initialized.
  void OnScreenAssetsLoaded(const std::string& async_assets_load_id);

  // Shows or hides OOBE UI elements.
  void ShowOobeUI(bool show);

  // TODO(rkc): Create a separate retail mode login UI and move this method
  // there - see crbug.com/157671.
  // Shows a login spinner for retail mode logins.
  void ShowRetailModeLoginSpinner();

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

 private:
  // Initializes |screen_ids_| and |screen_names_| structures.
  void InitializeScreenMaps();

  void AddScreenHandler(BaseScreenHandler* handler);

  // CoreOobeHandler::Delegate implementation:
  virtual void OnCurrentScreenChanged(const std::string& screen) OVERRIDE;

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
  UpdateScreenHandler* update_screen_handler_;
  NetworkScreenActor* network_screen_actor_;
  EulaScreenActor* eula_screen_actor_;
  EnrollmentScreenActor* enrollment_screen_actor_;
  HIDDetectionScreenActor* hid_detection_screen_actor_;
  ResetScreenActor* reset_screen_actor_;
  KioskAutolaunchScreenActor* autolaunch_screen_actor_;
  KioskEnableScreenActor* kiosk_enable_screen_actor_;
  WrongHWIDScreenActor* wrong_hwid_screen_actor_;
  AutoEnrollmentCheckScreenActor* auto_enrollment_check_screen_actor_;
  SupervisedUserCreationScreenHandler*
      supervised_user_creation_screen_actor_;
  AppLaunchSplashScreenActor* app_launch_splash_screen_actor_;
  ControllerPairingScreenActor* controller_pairing_screen_actor_;
  HostPairingScreenActor* host_pairing_screen_actor_;

  // Reference to ErrorScreenHandler that handles error screen
  // requests and forward calls from native code to JS side.
  ErrorScreenHandler* error_screen_handler_;

  // Reference to GaiaScreenHandler that handles gaia screen requests and
  // forwards calls from native code to JS side.
  GaiaScreenHandler* gaia_screen_handler_;

  // Reference to SigninScreenHandler that handles sign-in screen requests and
  // forwards calls from native code to JS side.
  SigninScreenHandler* signin_screen_handler_;

  TermsOfServiceScreenActor* terms_of_service_screen_actor_;
  UserImageScreenActor* user_image_screen_actor_;

  std::vector<BaseScreenHandler*> handlers_;  // Non-owning pointers.

  KioskAppMenuHandler* kiosk_app_menu_handler_;  // Non-owning pointers.

  // Id of the current oobe/login screen.
  Screen current_screen_;

  // Id of the previous oobe/login screen.
  Screen previous_screen_;

  // Maps JS screen names to screen ids.
  std::map<std::string, Screen> screen_ids_;

  // Maps screen ids to JS screen names.
  std::vector<std::string> screen_names_;

  // Flag that indicates whether JS part is fully loaded and ready to accept
  // calls.
  bool ready_;

  // Callbacks to notify when JS part is fully loaded and ready to accept calls.
  std::vector<base::Closure> ready_callbacks_;

  // List of registered observers.
  ObserverList<Observer> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(OobeUI);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_OOBE_UI_H_
