// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_WIZARD_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_WIZARD_CONTROLLER_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time.h"
#include "base/timer.h"
#include "chrome/browser/chromeos/login/screen_observer.h"
#include "chrome/browser/chromeos/login/wizard_screen.h"
#include "googleurl/src/gurl.h"
#include "ui/gfx/rect.h"

class PrefService;

namespace base {
class DictionaryValue;
}

namespace chromeos {

class EnterpriseEnrollmentScreen;
class EulaScreen;
class HTMLPageScreen;
class LoginDisplayHost;
class NetworkScreen;
class OobeDisplay;
class RegistrationScreen;
class ResetScreen;
class UpdateScreen;
class UserImageScreen;
class WizardScreen;

// Class that manages control flow between wizard screens. Wizard controller
// interacts with screen controllers to move the user between screens.
class WizardController : public ScreenObserver {
 public:
  // Observes screen changes.
  class Observer {
   public:
    // Called before a screen change happens.
    virtual void OnScreenChanged(WizardScreen* next_screen) = 0;

    // Called after the browser session has started.
    virtual void OnSessionStart() = 0;
  };

  WizardController(LoginDisplayHost* host, OobeDisplay* oobe_display);
  virtual ~WizardController();

  // Returns the default wizard controller if it has been created.
  static WizardController* default_controller() {
    return default_controller_;
  }

  // Whether the user image selection step should be skipped.
  static bool skip_user_image_selection() {
    return skip_user_image_selection_;
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

  // Sets delays to zero. MUST be used only for tests.
  static void SetZeroDelays();

  // If true zero delays have been enabled (for browser tests).
  static bool IsZeroDelayEnabled();

  // Sets initial locale in local settings.
  static void SetInitialLocale(const std::string& locale);

  // Registers OOBE preferences.
  static void RegisterPrefs(PrefService* local_state);

  // Marks user image screen to be always skipped after login.
  static void SkipImageSelectionForTesting();

  // Shows the first screen defined by |first_screen_name| or by default
  // if the parameter is empty. Takes ownership of |screen_parameters|.
  void Init(const std::string& first_screen_name,
            base::DictionaryValue* screen_parameters);

  // Advances to screen defined by |screen_name| and shows it.
  void AdvanceToScreen(const std::string& screen_name);

  // Advances to login screen. Should be used in for testing only.
  void SkipToLoginForTesting();

  // If being at register screen proceeds to the next one.
  void SkipRegistration();

  // Adds and removes an observer.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Called right after the browser session has started.
  void OnSessionStart();

  // Skip update, go straight to enrollment after EULA is accepted.
  void SkipUpdateEnrollAfterEula();

  // Lazy initializers and getters for screens.
  NetworkScreen* GetNetworkScreen();
  UpdateScreen* GetUpdateScreen();
  UserImageScreen* GetUserImageScreen();
  EulaScreen* GetEulaScreen();
  RegistrationScreen* GetRegistrationScreen();
  HTMLPageScreen* GetHTMLPageScreen();
  EnterpriseEnrollmentScreen* GetEnterpriseEnrollmentScreen();
  ResetScreen* GetResetScreen();

  // Returns a pointer to the current screen or NULL if there's no such
  // screen.
  WizardScreen* current_screen() const { return current_screen_; }

  // Returns true if the current wizard instance has reached the login screen.
  bool login_screen_started() const { return login_screen_started_; }

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
  static const char kResetScreenName[];

 private:
  // Show specific screen.
  void ShowNetworkScreen();
  void ShowUpdateScreen();
  void ShowUserImageScreen();
  void ShowEulaScreen();
  void ShowRegistrationScreen();
  void ShowHTMLPageScreen();
  void ShowEnterpriseEnrollmentScreen();
  void ShowResetScreen();

  // Shows images login screen.
  void ShowLoginScreen();

  // Resumes a pending login screen.
  void ResumeLoginScreen();

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
  void OnEnterpriseAutoEnrollmentDone();
  void OnResetCanceled();
  void OnOOBECompleted();

  // Loads brand code on I/O enabled thread and stores to Local State.
  void LoadBrandCodeFromFile();

  // Called after all post-EULA blocking tasks have been completed.
  void OnEulaBlockingTasksDone();

  // Shows update screen and starts update process.
  void InitiateOOBEUpdate();

  // Actions that should be done right after EULA is accepted,
  // before update check.
  void PerformPostEulaActions();

  // Actions that should be done right after update stage is finished.
  void PerformPostUpdateActions();

  // Overridden from ScreenObserver:
  virtual void OnExit(ExitCodes exit_code) OVERRIDE;
  virtual void ShowCurrentScreen() OVERRIDE;
  virtual void OnSetUserNamePassword(const std::string& username,
                                     const std::string& password) OVERRIDE;
  virtual void SetUsageStatisticsReporting(bool val) OVERRIDE;
  virtual bool GetUsageStatisticsReporting() const OVERRIDE;

  // Switches from one screen to another.
  void SetCurrentScreen(WizardScreen* screen);

  // Switches from one screen to another with delay before showing. Calling
  // ShowCurrentScreen directly forces screen to be shown immediately.
  void SetCurrentScreenSmooth(WizardScreen* screen, bool use_smoothing);

  // Changes status area visibility.
  void SetStatusAreaVisible(bool visible);

  // Logs in the specified user via default login screen.
  void Login(const std::string& username, const std::string& password);

  static bool skip_user_image_selection_;

  static bool zero_delay_enabled_;

  // Screens.
  scoped_ptr<NetworkScreen> network_screen_;
  scoped_ptr<UpdateScreen> update_screen_;
  scoped_ptr<UserImageScreen> user_image_screen_;
  scoped_ptr<EulaScreen> eula_screen_;
  scoped_ptr<RegistrationScreen> registration_screen_;
  scoped_ptr<ResetScreen> reset_screen_;
  scoped_ptr<HTMLPageScreen> html_page_screen_;
  scoped_ptr<EnterpriseEnrollmentScreen>
      enterprise_enrollment_screen_;

  // Screen that's currently active.
  WizardScreen* current_screen_;

  // Screen that was active before, or NULL for login screen.
  WizardScreen* previous_screen_;

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

  // Parameters for the first screen. May be NULL.
  scoped_ptr<base::DictionaryValue> screen_parameters_;

  base::OneShotTimer<WizardController> smooth_show_timer_;

  OobeDisplay* oobe_display_;

  // State of Usage stat/error reporting checkbox on EULA screen
  // during wizard lifetime.
  bool usage_statistics_reporting_;

  // If true then update check is cancelled and enrollment is started after
  // EULA is accepted.
  bool skip_update_enroll_after_eula_;

  // Time when the EULA was accepted. Used to measure the duration from the EULA
  // acceptance until the Sign-In screen is displayed.
  base::Time time_eula_accepted_;

  ObserverList<Observer> observer_list_;

  bool login_screen_started_;

  base::WeakPtrFactory<WizardController> weak_ptr_factory_;

  FRIEND_TEST_ALL_PREFIXES(EnterpriseEnrollmentScreenTest, TestCancel);
  FRIEND_TEST_ALL_PREFIXES(WizardControllerFlowTest, Accelerators);
  friend class WizardControllerFlowTest;
  friend class WizardInProcessBrowserTest;

  DISALLOW_COPY_AND_ASSIGN(WizardController);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_WIZARD_CONTROLLER_H_
