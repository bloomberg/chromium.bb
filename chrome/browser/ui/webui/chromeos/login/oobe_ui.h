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
#include "chrome/browser/chromeos/login/oobe_display.h"
#include "chrome/browser/ui/webui/chromeos/login/core_oobe_handler.h"
#include "content/public/browser/web_ui_controller.h"

namespace base {
class DictionaryValue;
}

namespace chromeos {
class BaseScreenHandler;
class CoreOobeHandler;
class ErrorScreenHandler;
class NativeWindowDelegate;
class NetworkStateInformer;
class SigninScreenHandler;
class SigninScreenHandlerDelegate;
}

namespace chromeos {

// A custom WebUI that defines datasource for out-of-box-experience (OOBE) UI:
// - welcome screen (setup language/keyboard/network).
// - eula screen (CrOS (+ OEM) EULA content/TPM password/crash reporting).
// - update screen.
class OobeUI : public OobeDisplay,
               public content::WebUIController,
               public CoreOobeHandler::Delegate {
 public:
  enum Screen {
    SCREEN_OOBE_NETWORK = 0,
    SCREEN_OOBE_EULA,
    SCREEN_OOBE_UPDATE,
    SCREEN_OOBE_ENROLLMENT,
    SCREEN_GAIA_SIGNIN,
    SCREEN_ACCOUNT_PICKER,
    SCREEN_ERROR_MESSAGE,
    SCREEN_USER_IMAGE_PICKER,
    SCREEN_TPM_ERROR,
    SCREEN_PASSWORD_CHANGED,
    SCREEN_CREATE_MANAGED_USER,
    SCREEN_TERMS_OF_SERVICE,
    SCREEN_WRONG_HWID,
    SCREEN_UNKNOWN
  };

  // JS oobe/login screens names.
  static const char kScreenOobeNetwork[];
  static const char kScreenOobeEula[];
  static const char kScreenOobeUpdate[];
  static const char kScreenOobeEnrollment[];
  static const char kScreenGaiaSignin[];
  static const char kScreenAccountPicker[];
  static const char kScreenErrorMessage[];
  static const char kScreenUserImagePicker[];
  static const char kScreenTpmError[];
  static const char kScreenPasswordChanged[];
  static const char kScreenManagedUserCreation[];
  static const char kScreenTermsOfService[];
  static const char kScreenWrongHWID[];

  explicit OobeUI(content::WebUI* web_ui);
  virtual ~OobeUI();

  // OobeDisplay implementation:
  virtual void ShowScreen(WizardScreen* screen) OVERRIDE;
  virtual void HideScreen(WizardScreen* screen) OVERRIDE;
  virtual UpdateScreenActor* GetUpdateScreenActor() OVERRIDE;
  virtual NetworkScreenActor* GetNetworkScreenActor() OVERRIDE;
  virtual EulaScreenActor* GetEulaScreenActor() OVERRIDE;
  virtual EnterpriseEnrollmentScreenActor* GetEnterpriseEnrollmentScreenActor()
      OVERRIDE;
  virtual ResetScreenActor* GetResetScreenActor() OVERRIDE;
  virtual TermsOfServiceScreenActor*
      GetTermsOfServiceScreenActor() OVERRIDE;
  virtual UserImageScreenActor* GetUserImageScreenActor() OVERRIDE;
  virtual ViewScreenDelegate* GetRegistrationScreenActor() OVERRIDE;
  virtual WrongHWIDScreenActor* GetWrongHWIDScreenActor() OVERRIDE;

  // Collects localized strings from the owned handlers.
  void GetLocalizedStrings(base::DictionaryValue* localized_strings);

  // Initializes the handlers.
  void InitializeHandlers();

  // Shows or hides OOBE UI elements.
  void ShowOobeUI(bool show);

  // TODO(rkc): Create a separate retail mode login UI and move this method
  // there - see crbug.com/157671.
  // Shows a login spinner for retail mode logins.
  void ShowRetailModeLoginSpinner();

  // Shows the signin screen.
  void ShowSigninScreen(SigninScreenHandlerDelegate* delegate,
                        NativeWindowDelegate* native_window_delegate);

  // Resets the delegate set in ShowSigninScreen.
  void ResetSigninScreenHandlerDelegate();

  Screen current_screen() const { return current_screen_; }

  const std::string& GetScreenName(Screen screen) const;

 private:
  // Initializes |screen_ids_| and |screen_names_| structures.
  void InitializeScreenMaps();

  void AddScreenHandler(BaseScreenHandler* handler);

  // CoreOobeHandler::Delegate implementation:
  virtual void OnCurrentScreenChanged(const std::string& screen) OVERRIDE;

  // Reference to NetworkStateInformer that handles changes in network
  // state.
  scoped_refptr<NetworkStateInformer> network_state_informer_;

  // Reference to CoreOobeHandler that handles common requests of Oobe page.
  CoreOobeHandler* core_handler_;

  // Screens actors. Note, OobeUI owns them via |handlers_|, not directly here.
  UpdateScreenActor* update_screen_actor_;
  NetworkScreenActor* network_screen_actor_;
  EulaScreenActor* eula_screen_actor_;
  EnterpriseEnrollmentScreenActor* enterprise_enrollment_screen_actor_;
  ResetScreenActor* reset_screen_actor_;
  WrongHWIDScreenActor* wrong_hwid_screen_actor_;

  // Reference to ErrorScreenHandler that handles error screen
  // requests and forward calls from native code to JS side.
  ErrorScreenHandler* error_screen_handler_;

  // Reference to SigninScreenHandler that handles sign-in screen requests and
  // forward calls from native code to JS side.
  SigninScreenHandler* signin_screen_handler_;

  TermsOfServiceScreenActor* terms_of_service_screen_actor_;
  UserImageScreenActor* user_image_screen_actor_;

  std::vector<BaseScreenHandler*> handlers_;  // Non-owning pointers.

  // Id of the current oobe/login screen.
  Screen current_screen_;

  // Maps JS screen names to screen ids.
  std::map<std::string, Screen> screen_ids_;

  // Maps screen ids to JS screen names.
  std::vector<std::string> screen_names_;

  DISALLOW_COPY_AND_ASSIGN(OobeUI);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_OOBE_UI_H_
