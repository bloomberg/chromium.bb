// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_WIZARD_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_WIZARD_CONTROLLER_H_
#pragma once

#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/timer.h"
#include "chrome/browser/chromeos/login/screen_observer.h"
#include "chrome/browser/chromeos/login/wizard_screen.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest_prod.h"
#include "ui/gfx/rect.h"

class PrefService;
class WizardContentsView;
class WizardScreen;

namespace chromeos {
class AccountScreen;
class EnterpriseEnrollmentScreen;
class EulaScreen;
class ExistingUserController;
class HTMLPageScreen;
class LoginDisplayHost;
class NetworkScreen;
class RegistrationScreen;
class UpdateScreen;
class UserImageScreen;
class WizardInProcessBrowserTest;
}

namespace gfx {
class Rect;
}

namespace views {
class Views;
class Widget;
class WidgetGtk;
}

// Class that manages control flow between wizard screens. Wizard controller
// interacts with screen controllers to move the user between screens.
class WizardController : public chromeos::ScreenObserver,
                         public WizardScreenDelegate {
 public:
  explicit WizardController(chromeos::LoginDisplayHost* host,
                            const gfx::Rect& screen_bounds);
  ~WizardController();

  // Returns the default wizard controller if it has been created.
  static WizardController* default_controller() {
    return default_controller_;
  }

  // Returns true if EULA has been accepted.
  static bool IsEulaAccepted();

  // Returns OOBE completion status.
  static bool IsOobeCompleted();

  // Marks EULA status as accepted.
  static void MarkEulaAccepted();

  // Marks OOBE process as completed.
  static void MarkOobeCompleted();

  // Returns device registration completion status, i.e. second part of OOBE.
  static bool IsDeviceRegistered();

  // Returns true if valid registration URL is defined.
  static bool IsRegisterScreenDefined();

  // Marks device registered. i.e. second part of OOBE is completed.
  static void MarkDeviceRegistered();

  // Returns initial locale from local settings.
  static std::string GetInitialLocale();

  // Sets initial locale in local settings.
  static void SetInitialLocale(const std::string& locale);

  // Shows the first screen defined by |first_screen_name| or by default
  // if the parameter is empty. |screen_bounds| are used to calculate position
  // of the wizard screen.
  void Init(const std::string& first_screen_name);

  // Returns the view that contains all the other views.
  views::View* contents() { return contents_; }

  // Skips OOBE update screen if it's currently shown.
  void CancelOOBEUpdate();

  // Lazy initializers and getters for screens.
  chromeos::NetworkScreen* GetNetworkScreen();
  chromeos::AccountScreen* GetAccountScreen();
  chromeos::UpdateScreen* GetUpdateScreen();
  chromeos::UserImageScreen* GetUserImageScreen();
  chromeos::EulaScreen* GetEulaScreen();
  chromeos::RegistrationScreen* GetRegistrationScreen();
  chromeos::HTMLPageScreen* GetHTMLPageScreen();
  chromeos::EnterpriseEnrollmentScreen* GetEnterpriseEnrollmentScreen();

  // Show specific screen.
  void ShowNetworkScreen();
  void ShowAccountScreen();
  void ShowUpdateScreen();
  void ShowUserImageScreen();
  void ShowEulaScreen();
  void ShowRegistrationScreen();
  void ShowHTMLPageScreen();
  void ShowEnterpriseEnrollmentScreen();

  // Shows images login screen.
  void ShowLoginScreen();

  // Returns a pointer to the current screen or NULL if there's no such
  // screen.
  WizardScreen* current_screen() const { return current_screen_; }

  // Overrides observer for testing.
  void set_observer(ScreenObserver* observer) { observer_ = observer; }

  // Set URL to open on browser launch.
  void set_start_url(const GURL& start_url) { start_url_ = start_url; }

  // If being at register screen proceeds to the next one.
  void SkipRegistration();

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
  static const char kHTMLPageScreenName[];
  static const char kEnterpriseEnrollmentScreenName[];

 private:
  // Exit handlers:
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
  void OnEnterpriseEnrollmentDone();
  void OnOOBECompleted();

  // Shows update screen and starts update process.
  void InitiateOOBEUpdate();

  // Overridden from chromeos::ScreenObserver:
  virtual void OnExit(ExitCodes exit_code);
  virtual void OnSetUserNamePassword(const std::string& username,
                                     const std::string& password);
  virtual void set_usage_statistics_reporting(bool val) {
    usage_statistics_reporting_ = val;
  }
  virtual bool usage_statistics_reporting() const {
    return usage_statistics_reporting_;
  }

  // Creates wizard screen window with the specified |bounds|.
  // If |initial_show| initial animation (window & background) is shown.
  // Otherwise only window is animated.
  views::Widget* CreateScreenWindow(const gfx::Rect& bounds,
                                    bool initial_show);

  // Returns bounds for the wizard screen host window in screen coordinates.
  // Calculates bounds using screen_bounds_.
  gfx::Rect GetWizardScreenBounds(int screen_width, int screen_height) const;

  // Switches from one screen to another.
  void SetCurrentScreen(WizardScreen* screen);

  // Switches from one screen to another with delay before showing. Calling
  // ShowCurrentScreen directly forces screen to be shown immediately.
  void SetCurrentScreenSmooth(WizardScreen* screen, bool use_smoothing);

  // Changes status area visibility.
  void SetStatusAreaVisible(bool visible);

  // Overridden from WizardScreenDelegate:
  virtual views::View* GetWizardView();
  virtual chromeos::ScreenObserver* GetObserver(WizardScreen* screen);
  virtual void ShowCurrentScreen();

  // Determines which screen to show first by the parameter, shows it and
  // sets it as the current one.
  void ShowFirstScreen(const std::string& first_screen_name);

  // Logs in the specified user via default login screen.
  void Login(const std::string& username, const std::string& password);

  // Sets delays to zero. MUST be used only for browser tests.
  static void SetZeroDelays();

  // Widget we're showing in.
  views::Widget* widget_;

  // Contents view.
  views::View* contents_;

  // Used to calculate position of the wizard screen.
  gfx::Rect screen_bounds_;

  // Screens.
  scoped_ptr<chromeos::NetworkScreen> network_screen_;
  scoped_ptr<chromeos::AccountScreen> account_screen_;
  scoped_ptr<chromeos::UpdateScreen> update_screen_;
  scoped_ptr<chromeos::UserImageScreen> user_image_screen_;
  scoped_ptr<chromeos::EulaScreen> eula_screen_;
  scoped_ptr<chromeos::RegistrationScreen> registration_screen_;
  scoped_ptr<chromeos::HTMLPageScreen> html_page_screen_;
  scoped_ptr<chromeos::EnterpriseEnrollmentScreen>
      enterprise_enrollment_screen_;

  // Screen that's currently active.
  WizardScreen* current_screen_;

  // Holds whether this is initial show.
  bool initial_show_;

  std::string username_;
  std::string password_;

  // True if controller is active and should proceed things like Timer.
  bool is_active_;

  // True if running official BUILD.
  bool is_official_build_;

  // True if full OOBE flow should be shown.
  bool is_out_of_box_;

  // Value of the screen name that WizardController was started with.
  std::string first_screen_name_;

  // OOBE/login display host.
  chromeos::LoginDisplayHost* host_;

  // NULL by default - controller itself is observer. Mock could be assigned.
  ScreenObserver* observer_;

  // Default WizardController.
  static WizardController* default_controller_;

  // URL to open on browser launch.
  GURL start_url_;

  base::OneShotTimer<WizardController> smooth_show_timer_;

  // State of Usage stat/error reporting checkbox on EULA screen
  // during wizard lifetime.
  bool usage_statistics_reporting_;

  FRIEND_TEST_ALL_PREFIXES(WizardControllerTest, SwitchLanguage);
  friend class WizardControllerFlowTest;
  friend class chromeos::WizardInProcessBrowserTest;

  DISALLOW_COPY_AND_ASSIGN(WizardController);
};

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_WIZARD_CONTROLLER_H_
