// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_ENROLLMENT_ENROLLMENT_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_ENROLLMENT_ENROLLMENT_SCREEN_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/login/enrollment/enrollment_screen_actor.h"
#include "chrome/browser/chromeos/login/enrollment/enterprise_enrollment_helper.h"
#include "chrome/browser/chromeos/login/screens/base_screen.h"
#include "chrome/browser/chromeos/policy/enrollment_config.h"
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
      public EnterpriseEnrollmentHelper::EnrollmentStatusConsumer,
      public EnrollmentScreenActor::Controller {
 public:
  EnrollmentScreen(BaseScreenDelegate* base_screen_delegate,
                   EnrollmentScreenActor* actor);
  ~EnrollmentScreen() override;

  static EnrollmentScreen* Get(ScreenManager* manager);

  // Setup how this screen will handle enrollment.
  //   |shark_controller| is an interface that is used to communicate with a
  //     remora device or a slave device for remote enrollment.
  void SetParameters(
      const policy::EnrollmentConfig& enrollment_config,
      pairing_chromeos::ControllerPairingController* shark_controller);

  // BaseScreen implementation:
  void PrepareToShow() override;
  void Show() override;
  void Hide() override;
  std::string GetName() const override;

  // EnrollmentScreenActor::Controller implementation:
  void OnLoginDone(const std::string& user,
                   const std::string& auth_code) override;
  void OnRetry() override;
  void OnCancel() override;
  void OnConfirmationClosed() override;
  void OnDeviceAttributeProvided(const std::string& asset_id,
                                 const std::string& location) override;

  // EnterpriseEnrollmentHelper::EnrollmentStatusConsumer implementation:
  void OnAuthError(const GoogleServiceAuthError& error) override;
  void OnEnrollmentError(policy::EnrollmentStatus status) override;
  void OnOtherError(EnterpriseEnrollmentHelper::OtherError error) override;
  void OnDeviceEnrolled(const std::string& additional_token) override;
  void OnDeviceAttributeUploadCompleted(bool success) override;
  void OnDeviceAttributeUpdatePermission(bool granted) override;

  // Used for testing.
  EnrollmentScreenActor* GetActor() {
    return actor_;
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(EnrollmentScreenTest, TestSuccess);
  FRIEND_TEST_ALL_PREFIXES(AttestationAuthEnrollmentScreenTest, TestCancel);
  FRIEND_TEST_ALL_PREFIXES(ForcedAttestationAuthEnrollmentScreenTest,
                           TestCancel);
  FRIEND_TEST_ALL_PREFIXES(MultiAuthEnrollmentScreenTest, TestCancel);
  FRIEND_TEST_ALL_PREFIXES(EnterpriseEnrollmentTest,
                           TestProperPageGetsLoadedOnEnrollmentSuccess);
  FRIEND_TEST_ALL_PREFIXES(EnterpriseEnrollmentTest,
                           TestAttributePromptPageGetsLoaded);
  FRIEND_TEST_ALL_PREFIXES(EnterpriseEnrollmentTest,
                           TestAuthCodeGetsProperlyReceivedFromGaia);
  FRIEND_TEST_ALL_PREFIXES(HandsOffNetworkScreenTest, RequiresNoInput);

  // The authentication mechanisms that this class can use.
  enum Auth {
    AUTH_ATTESTATION,
    AUTH_OAUTH,
  };

  // Sets the current config to use for enrollment.
  void SetConfig();

  // Creates an enrollment helper if needed.
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

  // Do attestation based enrollment.
  void AuthenticateUsingAttestation();

  // Shows the interactive screen. Resets auth then shows the signin screen.
  void ShowInteractiveScreen();

  // Shows the signin screen. Used as a callback to run after auth reset.
  void ShowSigninScreen();

  // Shows the device attribute prompt screen.
  // Used as a callback to run after successful enrollment.
  void ShowAttributePromptScreen();

  // Record metrics when we encounter an enrollment error.
  void RecordEnrollmentErrorMetrics();

  // Advance to the next authentication mechanism if possible.
  bool AdvanceToNextAuth();

  pairing_chromeos::ControllerPairingController* shark_controller_ = nullptr;

  EnrollmentScreenActor* actor_;
  policy::EnrollmentConfig config_;
  policy::EnrollmentConfig enrollment_config_;
  Auth current_auth_ = AUTH_OAUTH;
  Auth last_auth_ = AUTH_OAUTH;
  bool enrollment_failed_once_ = false;
  std::string enrolling_user_domain_;
  std::unique_ptr<base::ElapsedTimer> elapsed_timer_;
  std::unique_ptr<EnterpriseEnrollmentHelper> enrollment_helper_;
  base::WeakPtrFactory<EnrollmentScreen> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(EnrollmentScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_ENROLLMENT_ENROLLMENT_SCREEN_H_
