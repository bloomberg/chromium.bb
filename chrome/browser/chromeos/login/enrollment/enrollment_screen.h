// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_ENROLLMENT_ENROLLMENT_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_ENROLLMENT_ENROLLMENT_SCREEN_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/login/enrollment/enrollment_screen_actor.h"
#include "chrome/browser/chromeos/login/enrollment/enterprise_enrollment_helper.h"
#include "chrome/browser/chromeos/login/screens/base_screen.h"
#include "chrome/browser/chromeos/policy/enrollment_config.h"
#include "components/pairing/host_pairing_controller.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/enterprise_metrics.h"

namespace base {
class ElapsedTimer;
}

namespace pairing_chromeos {
class ControllerPairingController;
}

namespace chromeos {

class BaseScreenDelegate;
class ScreenManager;

// The screen implementation that links the enterprise enrollment UI into the
// OOBE wizard.
class EnrollmentScreen
    : public BaseScreen,
      public pairing_chromeos::HostPairingController::Observer,
      public EnterpriseEnrollmentHelper::EnrollmentStatusConsumer,
      public EnrollmentScreenActor::Controller {
 public:
  typedef pairing_chromeos::HostPairingController::Stage Stage;

  EnrollmentScreen(BaseScreenDelegate* base_screen_delegate,
                   EnrollmentScreenActor* actor);
  ~EnrollmentScreen() override;

  static EnrollmentScreen* Get(ScreenManager* manager);

  // Setup how this screen will handle enrollment.
  //   |shark_controller| is an interface that is used to communicate with a
  //     remora device for remote enrollment.
  //   |remora_controller| is an interface that is used to communicate with a
  //     shark device for remote enrollment.
  void SetParameters(
      const policy::EnrollmentConfig& enrollment_config,
      pairing_chromeos::ControllerPairingController* shark_controller,
      pairing_chromeos::HostPairingController* remora_controller);

  // BaseScreen implementation:
  void PrepareToShow() override;
  void Show() override;
  void Hide() override;
  std::string GetName() const override;

  // pairing_chromeos::HostPairingController::Observer:
  void PairingStageChanged(Stage new_stage) override;
  void ConfigureHost(bool accepted_eula,
                     const std::string& lang,
                     const std::string& timezone,
                     bool send_reports,
                     const std::string& keyboard_layout) override;
  void EnrollHost(const std::string& auth_token) override;

  // EnrollmentScreenActor::Controller implementation:
  void OnLoginDone(const std::string& user) override;
  void OnRetry() override;
  void OnCancel() override;
  void OnConfirmationClosed() override;

  // EnterpriseEnrollmentHelper::EnrollmentStatusConsumer implementation:
  void OnAuthError(const GoogleServiceAuthError& error) override;
  void OnEnrollmentError(policy::EnrollmentStatus status) override;
  void OnOtherError(EnterpriseEnrollmentHelper::OtherError error) override;
  void OnDeviceEnrolled(const std::string& additional_token) override;

  // Used for testing.
  EnrollmentScreenActor* GetActor() {
    return actor_;
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(EnrollmentScreenTest, TestSuccess);

  // Creates an enrollment helper.
  void CreateEnrollmentHelper();

  // Clears auth in |enrollment_helper_|. Deletes |enrollment_helper_| and runs
  // |callback| on completion. See the comment for
  // EnterpriseEnrollmentHelper::ClearAuth for details.
  void ClearAuth(const base::Closure& callback);

  // Used as a callback for EnterpriseEnrollmentHelper::ClearAuth.
  virtual void OnAuthCleared(const base::Closure& callback);

  // Sends an enrollment access token to a remote device.
  void SendEnrollmentAuthToken(const std::string& token);

  // Shows successful enrollment status after all enrollment related file
  // operations are completed.
  void ShowEnrollmentStatusOnSuccess();

  // Logs an UMA event in one of the "Enrollment.*" histograms, depending on
  // |enrollment_mode_|.
  void UMA(policy::MetricEnrollment sample);

  // Shows the signin screen. Used as a callback to run after auth reset.
  void ShowSigninScreen();

  void OnAnyEnrollmentError();

  pairing_chromeos::ControllerPairingController* shark_controller_;
  pairing_chromeos::HostPairingController* remora_controller_;
  EnrollmentScreenActor* actor_;
  policy::EnrollmentConfig enrollment_config_;
  bool enrollment_failed_once_;
  std::string enrolling_user_domain_;
  scoped_ptr<base::ElapsedTimer> elapsed_timer_;
  scoped_ptr<EnterpriseEnrollmentHelper> enrollment_helper_;
  base::WeakPtrFactory<EnrollmentScreen> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(EnrollmentScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_ENROLLMENT_ENROLLMENT_SCREEN_H_
