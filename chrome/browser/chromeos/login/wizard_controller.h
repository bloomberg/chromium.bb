// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_WIZARD_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_WIZARD_CONTROLLER_H_

#include <string>

#include "base/gtest_prod_util.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/chromeos/login/screen_observer.h"
#include "chrome/browser/chromeos/login/view_screen.h"
#include "chrome/browser/chromeos/login/wizard_screen.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

class PrefService;
class WizardContentsView;
class WizardScreen;

namespace chromeos {
class AccountScreen;
class BackgroundView;
class EulaScreen;
class LoginScreen;
class NetworkScreen;
class RegistrationScreen;
class StartupCustomizationDocument;
class UpdateScreen;
class UserImageScreen;
}

namespace gfx {
class Rect;
}

namespace views {
class Views;
class Widget;
}

// Class that manages control flow between wizard screens. Wizard controller
// interacts with screen controllers to move the user between screens.
class WizardController : public chromeos::ScreenObserver,
                         public WizardScreenDelegate {
 public:
  WizardController();
  ~WizardController();

  // Returns the default wizard controller if it has been created.
  static WizardController* default_controller() {
    return default_controller_;
  }

  // Shows the first screen defined by |first_screen_name| or by default
  // if the parameter is empty. |screen_bounds| are used to calculate position
  // of the wizard screen.
  void Init(const std::string& first_screen_name,
            const gfx::Rect& screen_bounds);

  // Returns the view that contains all the other views.
  views::View* contents() { return contents_; }

  // Shows the wizard controller in a window.
  void Show();

  // Creates and shows a background window.
  void ShowBackground(const gfx::Rect& bounds);

  // Takes ownership of the specified background widget and view.
  void OwnBackground(views::Widget* background_widget,
                     chromeos::BackgroundView* background_view);

  // Lazy initializers and getters for screens.
  chromeos::NetworkScreen* GetNetworkScreen();
  chromeos::LoginScreen* GetLoginScreen();
  chromeos::AccountScreen* GetAccountScreen();
  chromeos::UpdateScreen* GetUpdateScreen();
  chromeos::UserImageScreen* GetUserImageScreen();
  chromeos::EulaScreen* GetEulaScreen();
  chromeos::RegistrationScreen* GetRegistrationScreen();

  // Show specific screen.
  void ShowNetworkScreen();
  void ShowLoginScreen();
  void ShowAccountScreen();
  void ShowUpdateScreen();
  void ShowUserImageScreen();
  void ShowEulaScreen();
  void ShowRegistrationScreen();

  // Returns a pointer to the current screen or NULL if there's no such
  // screen.
  WizardScreen* current_screen() const { return current_screen_; }

  // True if WizardController is in OOBE mode.
  bool is_oobe() { return is_out_of_box_; }

  // Overrides observer for testing.
  void set_observer(ScreenObserver* observer) { observer_ = observer; }

  // Sets partner startup customization. WizardController takes ownership
  // of the document object.
  void SetCustomization(
      const chromeos::StartupCustomizationDocument* customization);

  // Returns partner startup customization document owned by WizardController.
  const chromeos::StartupCustomizationDocument* GetCustomization();

  // Registers OOBE preferences.
  static void RegisterPrefs(PrefService* local_state);

  static const char kNetworkScreenName[];
  static const char kLoginScreenName[];
  static const char kAccountScreenName[];
  static const char kUpdateScreenName[];
  static const char kUserImageScreenName[];
  static const char kRegistrationScreenName[];
  static const char kOutOfBoxScreenName[];
  static const char kTestNoScreenName[];
  static const char kEulaScreenName[];

 private:
  // Exit handlers:
  void OnLoginSignInSelected();
  void OnLoginGuestUser();
  void OnLoginCreateAccount();
  void OnNetworkConnected();
  void OnNetworkOffline();
  void OnAccountCreateBack();
  void OnAccountCreated();
  void OnConnectionFailed();
  void OnUpdateCompleted();
  void OnEulaAccepted();
  void OnUpdateErrorCheckingForUpdate();
  void OnUpdateErrorUpdating();
  void OnUserImageSelected();
  void OnUserImageSkipped();
  void OnRegistrationSuccess();
  void OnRegistrationSkipped();

  // Switches from one screen to another.
  void SetCurrentScreen(WizardScreen* screen);

  // Changes status area visibility.
  void SetStatusAreaVisible(bool visible);

  // Overridden from chromeos::ScreenObserver:
  virtual void OnExit(ExitCodes exit_code);
  virtual void OnSetUserNamePassword(const std::string& username,
                                     const std::string& password);

  // Overridden from WizardScreenDelegate:
  virtual views::View* GetWizardView();
  virtual chromeos::ScreenObserver* GetObserver(WizardScreen* screen);

  // Determines which screen to show first by the parameter, shows it and
  // sets it as the current one.
  void ShowFirstScreen(const std::string& first_screen_name);

  // Marks OOBE process as completed.
  void MarkOobeCompleted();

  // Widget we're showing in.
  views::Widget* widget_;

  // Used to render the background.
  views::Widget* background_widget_;
  chromeos::BackgroundView* background_view_;

  // Contents view.
  views::View* contents_;

  // Screens.
  scoped_ptr<chromeos::NetworkScreen> network_screen_;
  scoped_ptr<chromeos::LoginScreen> login_screen_;
  scoped_ptr<chromeos::AccountScreen> account_screen_;
  scoped_ptr<chromeos::UpdateScreen> update_screen_;
  scoped_ptr<chromeos::UserImageScreen> user_image_screen_;
  scoped_ptr<chromeos::EulaScreen> eula_screen_;
  scoped_ptr<chromeos::RegistrationScreen> registration_screen_;

  // Screen that's currently active.
  WizardScreen* current_screen_;

  std::string username_;
  std::string password_;

  // True if full OOBE flow should be shown.
  bool is_out_of_box_;

  // True if this is run under automation test and we need to show only
  // login screen.
  bool is_test_mode_;

  // NULL by default - controller itself is observer. Mock could be assigned.
  ScreenObserver* observer_;

  // Default WizardController.
  static WizardController* default_controller_;

  // Partner startup customizations.
  scoped_ptr<const chromeos::StartupCustomizationDocument> customization_;

  FRIEND_TEST_ALL_PREFIXES(WizardControllerFlowTest, ControlFlowErrorNetwork);
  FRIEND_TEST_ALL_PREFIXES(WizardControllerFlowTest, ControlFlowErrorUpdate);
  FRIEND_TEST_ALL_PREFIXES(WizardControllerFlowTest,
                           ControlFlowLanguageOnLogin);
  FRIEND_TEST_ALL_PREFIXES(WizardControllerFlowTest,
                           ControlFlowLanguageOnNetwork);
  FRIEND_TEST_ALL_PREFIXES(WizardControllerFlowTest, ControlFlowMain);
  FRIEND_TEST_ALL_PREFIXES(WizardControllerTest, SwitchLanguage);
  friend class WizardControllerFlowTest;

  DISALLOW_COPY_AND_ASSIGN(WizardController);
};

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_WIZARD_CONTROLLER_H_
