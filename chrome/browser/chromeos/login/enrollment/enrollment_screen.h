// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_ENROLLMENT_ENROLLMENT_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_ENROLLMENT_ENROLLMENT_SCREEN_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/login/enrollment/enrollment_screen_actor.h"
#include "chrome/browser/chromeos/login/screens/wizard_screen.h"
#include "components/pairing/host_pairing_controller.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/enterprise_metrics.h"

namespace pairing_chromeos {
class ControllerPairingController;
}

namespace chromeos {

class ScreenManager;
class ScreenObserver;

// The screen implementation that links the enterprise enrollment UI into the
// OOBE wizard.
class EnrollmentScreen
    : public WizardScreen,
      public pairing_chromeos::HostPairingController::Observer,
      public EnrollmentScreenActor::Controller {
 public:
  typedef pairing_chromeos::HostPairingController::Stage Stage;

  EnrollmentScreen(ScreenObserver* observer,
                   EnrollmentScreenActor* actor);
  virtual ~EnrollmentScreen();

  static EnrollmentScreen* Get(ScreenManager* manager);

  // Setup how this screen will handle enrollment.
  //   |auth_token| is an optional OAuth token to attempt to enroll with.
  //   |shark_controller| is an interface that is used to communicate with a
  //     remora device for remote enrollment.
  //   |remora_controller| is an interface that is used to communicate with a
  //     shark device for remote enrollment.
  void SetParameters(
      EnrollmentScreenActor::EnrollmentMode enrollment_mode,
      const std::string& management_domain,
      const std::string& enrollment_user,
      const std::string& auth_token,
      pairing_chromeos::ControllerPairingController* shark_controller,
      pairing_chromeos::HostPairingController* remora_controller);

  // WizardScreen implementation:
  virtual void PrepareToShow() OVERRIDE;
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual std::string GetName() const OVERRIDE;

  // pairing_chromeos::HostPairingController::Observer:
  virtual void PairingStageChanged(Stage new_stage) OVERRIDE;
  virtual void ConfigureHost(bool accepted_eula,
                             const std::string& lang,
                             const std::string& timezone,
                             bool send_reports,
                             const std::string& keyboard_layout) OVERRIDE;
  virtual void EnrollHost(const std::string& auth_token) OVERRIDE;

  // EnrollmentScreenActor::Controller implementation:
  virtual void OnLoginDone(const std::string& user) OVERRIDE;
  virtual void OnAuthError(const GoogleServiceAuthError& error) OVERRIDE;
  virtual void OnOAuthTokenAvailable(const std::string& oauth_token) OVERRIDE;
  virtual void OnRetry() OVERRIDE;
  virtual void OnCancel() OVERRIDE;
  virtual void OnConfirmationClosed() OVERRIDE;

  // Used for testing.
  EnrollmentScreenActor* GetActor() {
    return actor_;
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(EnrollmentScreenTest, TestSuccess);

  // Starts the Lockbox storage process.
  void WriteInstallAttributesData();

  // Kicks off the policy infrastructure to register with the service.
  void RegisterForDevicePolicy(const std::string& token);

  // Sends an enrollment access token to a remote device.
  void SendEnrollmentAuthToken(const std::string& token);

  // Handles enrollment completion. Logs a UMA sample and requests the actor to
  // show the specified enrollment status.
  void ReportEnrollmentStatus(policy::EnrollmentStatus status);

  // Shows successful enrollment status after all enrollment related file
  // operations are completed.
  void ShowEnrollmentStatusOnSuccess(const policy::EnrollmentStatus& status);

  // Logs an UMA event in the kMetricEnrollment or the kMetricEnrollmentRecovery
  // histogram, depending on |enrollment_mode_|.
  void UMA(policy::MetricEnrollment sample);

  // Logs an UMA event in the kMetricEnrollment or the kMetricEnrollmentRecovery
  // histogram, depending on |enrollment_mode_|.  If auto-enrollment is on,
  // |sample| is ignored and a kMetricEnrollmentAutoFailed sample is logged
  // instead.
  void UMAFailure(policy::MetricEnrollment sample);

  // Shows the signin screen. Used as a callback to run after auth reset.
  void ShowSigninScreen();

  // Convenience helper to check for auto enrollment mode.
  bool is_auto_enrollment() const {
    return enrollment_mode_ == EnrollmentScreenActor::ENROLLMENT_MODE_AUTO;
  }

  pairing_chromeos::ControllerPairingController* shark_controller_;
  pairing_chromeos::HostPairingController* remora_controller_;
  EnrollmentScreenActor* actor_;
  EnrollmentScreenActor::EnrollmentMode enrollment_mode_;
  bool enrollment_failed_once_;
  bool remora_token_sent_;
  std::string user_;
  std::string auth_token_;
  int lockbox_init_duration_;
  base::WeakPtrFactory<EnrollmentScreen> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(EnrollmentScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_ENROLLMENT_ENROLLMENT_SCREEN_H_
