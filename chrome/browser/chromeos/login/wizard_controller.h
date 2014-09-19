// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_WIZARD_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_WIZARD_CONTROLLER_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/containers/hash_tables.h"
#include "base/gtest_prod_util.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/login/screen_manager.h"
#include "chrome/browser/chromeos/login/screens/screen_observer.h"

class PrefRegistrySimple;
class PrefService;

namespace base {
class DictionaryValue;
}

namespace pairing_chromeos {
class SharkConnectionListener;
class ControllerPairingController;
class HostPairingController;
}

namespace chromeos {

class AutoEnrollmentCheckScreen;
class EnrollmentScreen;
class ErrorScreen;
struct Geoposition;
class LoginDisplayHost;
class LoginScreenContext;
class NetworkScreen;
class OobeDisplay;
class SimpleGeolocationProvider;
class SupervisedUserCreationScreen;
class TimeZoneProvider;
struct TimeZoneResponseData;
class UpdateScreen;
class UserImageScreen;

// Class that manages control flow between wizard screens. Wizard controller
// interacts with screen controllers to move the user between screens.
class WizardController : public ScreenObserver, public ScreenManager {
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

  // Whether to skip any screens that may normally be shown after login
  // (registration, Terms of Service, user image selection).
  static bool skip_post_login_screens() {
    return skip_post_login_screens_;
  }

  // Sets delays to zero. MUST be used only for tests.
  static void SetZeroDelays();

  // If true zero delays have been enabled (for browser tests).
  static bool IsZeroDelayEnabled();

  // Checks whether screen show time should be tracked with UMA.
  static bool IsOOBEStepToTrack(const std::string& screen_id);

  // Skips any screens that may normally be shown after login (registration,
  // Terms of Service, user image selection).
  static void SkipPostLoginScreensForTesting();

  // Checks whether OOBE should start enrollment automatically.
  static bool ShouldAutoStartEnrollment();

  // Checks whether OOBE should recover enrollment.  Note that this flips to
  // false once device policy has been restored as a part of recovery.
  static bool ShouldRecoverEnrollment();

  // Obtains domain the device used to be enrolled to from install attributes.
  static std::string GetEnrollmentRecoveryDomain();

  // Shows the first screen defined by |first_screen_name| or by default
  // if the parameter is empty. Takes ownership of |screen_parameters|.
  void Init(const std::string& first_screen_name,
            scoped_ptr<base::DictionaryValue> screen_parameters);

  // Advances to screen defined by |screen_name| and shows it.
  void AdvanceToScreen(const std::string& screen_name);

  // Advances to login screen. Should be used in for testing only.
  void SkipToLoginForTesting(const LoginScreenContext& context);

  // Adds and removes an observer.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Called right after the browser session has started.
  void OnSessionStart();

  // Skip update, go straight to enrollment after EULA is accepted.
  void SkipUpdateEnrollAfterEula();

  // TODO(antrim) : temporary hack. Should be removed once screen system is
  // reworked at hackaton.
  void EnableUserImageScreenReturnToPreviousHack();

  // Callback for enrollment auth token.
  void OnEnrollmentAuthTokenReceived(const std::string& auth_token);

  // Returns a pointer to the current screen or NULL if there's no such
  // screen.
  WizardScreen* current_screen() const { return current_screen_; }

  // Returns true if the current wizard instance has reached the login screen.
  bool login_screen_started() const { return login_screen_started_; }

  // ScreenManager implementation.
  virtual WizardScreen* CreateScreen(const std::string& screen_name) OVERRIDE;

  static const char kNetworkScreenName[];
  static const char kLoginScreenName[];
  static const char kUpdateScreenName[];
  static const char kUserImageScreenName[];
  static const char kOutOfBoxScreenName[];
  static const char kTestNoScreenName[];
  static const char kEulaScreenName[];
  static const char kEnrollmentScreenName[];
  static const char kResetScreenName[];
  static const char kKioskEnableScreenName[];
  static const char kKioskAutolaunchScreenName[];
  static const char kErrorScreenName[];
  static const char kTermsOfServiceScreenName[];
  static const char kAutoEnrollmentCheckScreenName[];
  static const char kWrongHWIDScreenName[];
  static const char kSupervisedUserCreationScreenName[];
  static const char kAppLaunchSplashScreenName[];
  static const char kHIDDetectionScreenName[];
  static const char kControllerPairingScreenName[];
  static const char kHostPairingScreenName[];

  // Volume percent at which spoken feedback is still audible.
  static const int kMinAudibleOutputVolumePercent;

 private:
  // Show specific screen.
  void ShowNetworkScreen();
  void ShowUpdateScreen();
  void ShowUserImageScreen();
  void ShowEulaScreen();
  void ShowEnrollmentScreen();
  void ShowResetScreen();
  void ShowKioskAutolaunchScreen();
  void ShowKioskEnableScreen();
  void ShowTermsOfServiceScreen();
  void ShowWrongHWIDScreen();
  void ShowAutoEnrollmentCheckScreen();
  void ShowSupervisedUserCreationScreen();
  void ShowHIDDetectionScreen();
  void ShowControllerPairingScreen();
  void ShowHostPairingScreen();

  // Shows images login screen.
  void ShowLoginScreen(const LoginScreenContext& context);

  // Resumes a pending login screen.
  void ResumeLoginScreen();

  // Invokes corresponding first OOBE screen.
  void OnHIDScreenNecessityCheck(bool screen_needed);

  // Exit handlers:
  void OnHIDDetectionCompleted();
  void OnNetworkConnected();
  void OnNetworkOffline();
  void OnConnectionFailed();
  void OnUpdateCompleted();
  void OnEulaAccepted();
  void OnUpdateErrorCheckingForUpdate();
  void OnUpdateErrorUpdating();
  void OnUserImageSelected();
  void OnUserImageSkipped();
  void OnEnrollmentDone();
  void OnAutoEnrollmentDone();
  void OnResetCanceled();
  void OnKioskAutolaunchCanceled();
  void OnKioskAutolaunchConfirmed();
  void OnKioskEnableCompleted();
  void OnWrongHWIDWarningSkipped();
  void OnAutoEnrollmentCheckCompleted();
  void OnTermsOfServiceDeclined();
  void OnTermsOfServiceAccepted();
  void OnControllerPairingFinished();
  void OnHostPairingFinished();

  // Callback function after setting MetricsReporting.
  void InitiateMetricsReportingChangeCallback(bool enabled);

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
  void PerformOOBECompletedActions();

  // Overridden from ScreenObserver:
  virtual void OnExit(ExitCodes exit_code) OVERRIDE;
  virtual void ShowCurrentScreen() OVERRIDE;
  virtual void OnSetUserNamePassword(const std::string& username,
                                     const std::string& password) OVERRIDE;
  virtual void SetUsageStatisticsReporting(bool val) OVERRIDE;
  virtual bool GetUsageStatisticsReporting() const OVERRIDE;
  virtual ErrorScreen* GetErrorScreen() OVERRIDE;
  virtual void ShowErrorScreen() OVERRIDE;
  virtual void HideErrorScreen(WizardScreen* parent_screen) OVERRIDE;

  // Notification of a change in the state of an accessibility setting.
  void OnAccessibilityStatusChanged(
      const AccessibilityStatusEventDetails& details);

  // Switches from one screen to another.
  void SetCurrentScreen(WizardScreen* screen);

  // Switches from one screen to another with delay before showing. Calling
  // ShowCurrentScreen directly forces screen to be shown immediately.
  void SetCurrentScreenSmooth(WizardScreen* screen, bool use_smoothing);

  // Changes status area visibility.
  void SetStatusAreaVisible(bool visible);

  // Logs in the specified user via default login screen.
  void Login(const std::string& username, const std::string& password);

  // Launched kiosk app configured for auto-launch.
  void AutoLaunchKioskApp();

  // Checks whether the user is allowed to exit enrollment.
  static bool CanExitEnrollment();

  // Gets the management domain.
  static std::string GetForcedEnrollmentDomain();

  // Called when LocalState is initialized.
  void OnLocalStateInitialized(bool /* succeeded */);

  // Returns local state.
  PrefService* GetLocalState();

  static void set_local_state_for_testing(PrefService* local_state) {
    local_state_for_testing_ = local_state;
  }

  std::string first_screen_name() { return first_screen_name_; }

  // Called when network is UP.
  void StartTimezoneResolve();

  // Creates provider on demand.
  TimeZoneProvider* GetTimezoneProvider();

  // TimeZoneRequest::TimeZoneResponseCallback implementation.
  void OnTimezoneResolved(scoped_ptr<TimeZoneResponseData> timezone,
                          bool server_error);

  // Called from SimpleGeolocationProvider when location is resolved.
  void OnLocationResolved(const Geoposition& position,
                          bool server_error,
                          const base::TimeDelta elapsed);

  // Returns true if callback has been installed.
  // Returns false if timezone has already been resolved.
  bool SetOnTimeZoneResolvedForTesting(const base::Closure& callback);

  // Returns true for pairing remora OOBE.
  bool IsHostPairingOobe() const;

  // Starts listening for an incoming shark controller connection, if we are
  // running remora OOBE.
  void MaybeStartListeningForSharkConnection();

  // Called when a connection to controller has been established. Wizard
  // controller takes the ownership of |pairing_controller| after that call.
  void OnSharkConnected(
      scoped_ptr<pairing_chromeos::HostPairingController> pairing_controller);

  // Whether to skip any screens that may normally be shown after login
  // (registration, Terms of Service, user image selection).
  static bool skip_post_login_screens_;

  static bool zero_delay_enabled_;

  // Screen that's currently active.
  WizardScreen* current_screen_;

  // Screen that was active before, or NULL for login screen.
  WizardScreen* previous_screen_;

  std::string username_;
  std::string password_;
  std::string auth_token_;

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

  // Whether enrollment will be or has been recovered in the current wizard
  // instance.
  bool enrollment_recovery_;

  // Time when the EULA was accepted. Used to measure the duration from the EULA
  // acceptance until the Sign-In screen is displayed.
  base::Time time_eula_accepted_;

  // Time when OOBE was started. Used to measure the total time from boot to
  // user Sign-In completed.
  base::Time time_oobe_started_;

  ObserverList<Observer> observer_list_;

  bool login_screen_started_;

  // Indicates that once image selection screen finishes we should return to
  // a previous screen instead of proceeding with usual flow.
  bool user_image_screen_return_to_previous_hack_;

  // Non-owning pointer to local state used for testing.
  static PrefService* local_state_for_testing_;

  FRIEND_TEST_ALL_PREFIXES(EnrollmentScreenTest, TestCancel);
  FRIEND_TEST_ALL_PREFIXES(WizardControllerFlowTest, Accelerators);
  friend class WizardControllerFlowTest;
  friend class WizardControllerOobeResumeTest;
  friend class WizardInProcessBrowserTest;
  friend class WizardControllerBrokenLocalStateTest;

  scoped_ptr<AccessibilityStatusSubscription> accessibility_subscription_;

  scoped_ptr<SimpleGeolocationProvider> geolocation_provider_;
  scoped_ptr<TimeZoneProvider> timezone_provider_;

  // Pairing controller for shark devices.
  scoped_ptr<pairing_chromeos::ControllerPairingController>
      controller_pairing_controller_;

  // Pairing controller for remora devices.
  scoped_ptr<pairing_chromeos::HostPairingController> host_pairing_controller_;

  // Maps screen ids to last time of their shows.
  base::hash_map<std::string, base::Time> screen_show_times_;

  // Tests check result of timezone resolve.
  bool timezone_resolved_;
  base::Closure on_timezone_resolved_for_testing_;

  // True if shark device initiated connection to this device.
  bool shark_controller_detected_;

  // Listens for incoming connection from a shark controller if a regular (not
  // pairing) remora OOBE is active. If connection is established, wizard
  // conroller swithces to a pairing OOBE.
  scoped_ptr<pairing_chromeos::SharkConnectionListener>
      shark_connection_listener_;

  base::WeakPtrFactory<WizardController> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(WizardController);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_WIZARD_CONTROLLER_H_
