// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_WIZARD_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_WIZARD_CONTROLLER_H_
#pragma once

#include <string>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/timer.h"
#include "chrome/browser/chromeos/login/screen_observer.h"
#include "chrome/browser/chromeos/login/wizard_screen.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest_prod.h"
#include "ui/gfx/rect.h"

class PrefService;

namespace chromeos {

class EnterpriseEnrollmentScreen;
class EulaScreen;
class HTMLPageScreen;
class LoginDisplayHost;
class NetworkScreen;
class OobeDisplay;
class RegistrationScreen;
class UpdateScreen;
class UserImageScreen;
class WizardScreen;

// Class that manages control flow between wizard screens. Wizard controller
// interacts with screen controllers to move the user between screens.
class WizardController : public ScreenObserver {
 public:
  WizardController(LoginDisplayHost* host, OobeDisplay* oobe_display);
  virtual ~WizardController();

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

  // Skips OOBE update screen if it's currently shown.
  void CancelOOBEUpdate();

  // Lazy initializers and getters for screens.
  NetworkScreen* GetNetworkScreen();
  UpdateScreen* GetUpdateScreen();
  UserImageScreen* GetUserImageScreen();
  EulaScreen* GetEulaScreen();
  RegistrationScreen* GetRegistrationScreen();
  HTMLPageScreen* GetHTMLPageScreen();
  EnterpriseEnrollmentScreen* GetEnterpriseEnrollmentScreen();

  // Show specific screen.
  void ShowNetworkScreen();
  void ShowUpdateScreen();
  void ShowUserImageScreen();
  void ShowEulaScreen();
  void ShowRegistrationScreen();
  void ShowHTMLPageScreen();
  void ShowEnterpriseEnrollmentScreen();

  // Determines which screen to show first by the parameter, shows it and
  // sets it as the current one.
  void ShowFirstScreen(const std::string& first_screen_name);

  // Shows images login screen.
  void ShowLoginScreen();

  // Returns a pointer to the current screen or NULL if there's no such
  // screen.
  WizardScreen* current_screen() const { return current_screen_; }

  // Set URL to open on browser launch.
  void set_start_url(const GURL& start_url) { start_url_ = start_url; }

  // If being at register screen proceeds to the next one.
  void SkipRegistration();

  // Registers OOBE preferences.
  static void RegisterPrefs(PrefService* local_state);

  static const char kNetworkScreenName[];
  static const char kLoginScreenName[];
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

  // Overridden from ScreenObserver:
  virtual void OnExit(ExitCodes exit_code) OVERRIDE;
  virtual void ShowCurrentScreen() OVERRIDE;
  virtual void OnSetUserNamePassword(const std::string& username,
                                     const std::string& password) OVERRIDE;
  virtual void set_usage_statistics_reporting(bool val) OVERRIDE;
  virtual bool usage_statistics_reporting() const OVERRIDE;

  // Switches from one screen to another.
  void SetCurrentScreen(WizardScreen* screen);

  // Switches from one screen to another with delay before showing. Calling
  // ShowCurrentScreen directly forces screen to be shown immediately.
  void SetCurrentScreenSmooth(WizardScreen* screen, bool use_smoothing);

  // Changes status area visibility.
  void SetStatusAreaVisible(bool visible);

  // Logs in the specified user via default login screen.
  void Login(const std::string& username, const std::string& password);

  // Sets delays to zero. MUST be used only for browser tests.
  static void SetZeroDelays();

  // Screens.
  scoped_ptr<NetworkScreen> network_screen_;
  scoped_ptr<UpdateScreen> update_screen_;
  scoped_ptr<UserImageScreen> user_image_screen_;
  scoped_ptr<EulaScreen> eula_screen_;
  scoped_ptr<RegistrationScreen> registration_screen_;
  scoped_ptr<HTMLPageScreen> html_page_screen_;
  scoped_ptr<EnterpriseEnrollmentScreen>
      enterprise_enrollment_screen_;

  // Screen that's currently active.
  WizardScreen* current_screen_;

  std::string username_;
  std::string password_;

  // True if running official BUILD.
  bool is_official_build_;

  // True if full OOBE flow should be shown.
  bool is_out_of_box_;

  // Value of the screen name that WizardController was started with.
  std::string first_screen_name_;

  // OOBE/login display host.
  LoginDisplayHost* host_;

  // Default WizardController.
  static WizardController* default_controller_;

  // URL to open on browser launch.
  GURL start_url_;

  base::OneShotTimer<WizardController> smooth_show_timer_;

  OobeDisplay* oobe_display_;

  // State of Usage stat/error reporting checkbox on EULA screen
  // during wizard lifetime.
  bool usage_statistics_reporting_;

  FRIEND_TEST_ALL_PREFIXES(EnterpriseEnrollmentScreenTest, TestCancel);
  FRIEND_TEST_ALL_PREFIXES(WizardControllerFlowTest, Accelerators);
  friend class WizardControllerFlowTest;
  friend class WizardInProcessBrowserTest;

  DISALLOW_COPY_AND_ASSIGN(WizardController);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_WIZARD_CONTROLLER_H_
