// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_ENROLLMENT_HANDLER_CHROMEOS_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_ENROLLMENT_HANDLER_CHROMEOS_H_

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_initializer.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_validator.h"
#include "chrome/browser/chromeos/policy/enrollment_config.h"
#include "chrome/browser/chromeos/settings/install_attributes.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "google_apis/gaia/gaia_oauth_client.h"

namespace base {
class SequencedTaskRunner;
}

namespace chromeos {
namespace attestation {
class AttestationFlow;
}
}

namespace policy {

class DeviceCloudPolicyStoreChromeOS;
class ServerBackedStateKeysBroker;

// Implements the logic that establishes enterprise enrollment for Chromium OS
// devices. The process is as follows:
//   1. Given an auth token, register with the policy service.
//   2. Download the initial policy blob from the service.
//   3. Verify the policy blob. Everything up to this point doesn't touch device
//      state.
//   4. Download the OAuth2 authorization code for device-level API access.
//   5. Download the OAuth2 refresh token for device-level API access and store
//      it.
//   6. Establish the device lock in installation-time attributes.
//   7. Store the policy blob and API refresh token.
class EnrollmentHandlerChromeOS : public CloudPolicyClient::Observer,
                                  public CloudPolicyStore::Observer,
                                  public gaia::GaiaOAuthClient::Delegate {
 public:
  typedef DeviceCloudPolicyInitializer::EnrollmentCallback
      EnrollmentCallback;

  // |store| and |install_attributes| must remain valid for the life time of the
  // enrollment handler.
  EnrollmentHandlerChromeOS(
      DeviceCloudPolicyStoreChromeOS* store,
      chromeos::InstallAttributes* install_attributes,
      ServerBackedStateKeysBroker* state_keys_broker,
      chromeos::attestation::AttestationFlow* attestation_flow,
      std::unique_ptr<CloudPolicyClient> client,
      scoped_refptr<base::SequencedTaskRunner> background_task_runner,
      const EnrollmentConfig& enrollment_config,
      const std::string& auth_token,
      const std::string& client_id,
      const std::string& requisition,
      const EnrollmentCallback& completion_callback);
  ~EnrollmentHandlerChromeOS() override;

  // Starts the enrollment process and reports the result to
  // |completion_callback_|.
  void StartEnrollment();

  // Releases the client.
  std::unique_ptr<CloudPolicyClient> ReleaseClient();

  // CloudPolicyClient::Observer:
  void OnPolicyFetched(CloudPolicyClient* client) override;
  void OnRegistrationStateChanged(CloudPolicyClient* client) override;
  void OnRobotAuthCodesFetched(CloudPolicyClient* client) override;
  void OnClientError(CloudPolicyClient* client) override;

  // CloudPolicyStore::Observer:
  void OnStoreLoaded(CloudPolicyStore* store) override;
  void OnStoreError(CloudPolicyStore* store) override;

  // GaiaOAuthClient::Delegate:
  void OnGetTokensResponse(const std::string& refresh_token,
                           const std::string& access_token,
                           int expires_in_seconds) override;
  void OnRefreshTokenResponse(const std::string& access_token,
                              int expires_in_seconds) override;
  void OnOAuthError() override;
  void OnNetworkError(int response_code) override;

 private:
  // Indicates what step of the process is currently pending. These steps need
  // to be listed in the order they are traversed in.  (Steps are numbered
  // explicitly to make it easier to read debug logs.)
  enum EnrollmentStep {
    STEP_PENDING = 0,             // Not started yet.
    STEP_STATE_KEYS = 1,          // Waiting for state keys to become available.
    STEP_LOADING_STORE = 2,       // Waiting for |store_| to initialize.
    STEP_REGISTRATION = 3,        // Currently registering the client.
    STEP_POLICY_FETCH = 4,        // Fetching policy.
    STEP_VALIDATION = 5,          // Policy validation.
    STEP_ROBOT_AUTH_FETCH = 6,    // Fetching device API auth code.
    STEP_ROBOT_AUTH_REFRESH = 7,  // Fetching device API refresh token.
    STEP_LOCK_DEVICE = 8,         // Writing installation-time attributes.
    STEP_STORE_TOKEN_AND_ID = 9,  // Storing DM token and virtual device ID.
    STEP_STORE_ROBOT_AUTH = 10,   // Encrypting & writing robot refresh token.
    STEP_STORE_POLICY = 11,       // Storing policy and API refresh token. For
                                  // AD, includes policy fetch via authpolicyd.
    STEP_FINISHED = 12,           // Enrollment process done, no further action.
  };

  // Handles the response to a request for server-backed state keys.
  void HandleStateKeysResult(const std::vector<std::string>& state_keys);

  // Starts attestation based enrollment flow.
  void StartAttestationBasedEnrollmentFlow();

  // Handles the response to a request for a registration certificate.
  void HandleRegistrationCertificateResult(
      bool success,
      const std::string& pem_certificate_chain);

  // Starts registration if the store is initialized.
  void StartRegistration();

  // Handles the policy validation result, proceeding with device lock if
  // successful.
  void HandlePolicyValidationResult(DeviceCloudPolicyValidator* validator);

  // Calls InstallAttributes::LockDevice() for enterprise enrollment and
  // DeviceSettingsService::SetManagementSettings() for consumer
  // enrollment.
  void StartLockDevice();

  // Handle callback from InstallAttributes::LockDevice() and retry on failure.
  void HandleLockDeviceResult(
      chromeos::InstallAttributes::LockResult lock_result);

  // Initiates storing of robot auth token.
  void StartStoreRobotAuth();

  // Handles completion of the robot token store operation.
  void HandleStoreRobotAuthTokenResult(bool result);

  // Handles result from device policy refresh via authpolicyd.
  void HandleActiveDirectoryPolicyRefreshed(bool success);

  // Drops any ongoing actions.
  void Stop();

  // Reports the result of the enrollment process to the initiator.
  void ReportResult(EnrollmentStatus status);

  // Set |enrollment_step_| to |step|.
  void SetStep(EnrollmentStep step);

  DeviceCloudPolicyStoreChromeOS* store_;
  chromeos::InstallAttributes* install_attributes_;
  ServerBackedStateKeysBroker* state_keys_broker_;
  chromeos::attestation::AttestationFlow* attestation_flow_;
  std::unique_ptr<CloudPolicyClient> client_;
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;
  std::unique_ptr<gaia::GaiaOAuthClient> gaia_oauth_client_;

  EnrollmentConfig enrollment_config_;
  std::string auth_token_;
  std::string client_id_;
  std::string requisition_;
  EnrollmentCallback completion_callback_;

  // The current state key provided by |state_keys_broker_|.
  std::string current_state_key_;

  // The device mode as received in the registration request.
  DeviceMode device_mode_;

  // Whether the server signaled to skip robot auth setup.
  bool skip_robot_auth_;

  // The robot account refresh token.
  std::string robot_refresh_token_;

  // The validated policy response info to be installed in the store.
  std::unique_ptr<enterprise_management::PolicyFetchResponse> policy_;
  std::string domain_;
  std::string device_id_;

  // Current enrollment step.
  EnrollmentStep enrollment_step_;

  // Total amount of time in milliseconds spent waiting for lockbox
  // initialization.
  int lockbox_init_duration_;

  base::WeakPtrFactory<EnrollmentHandlerChromeOS> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(EnrollmentHandlerChromeOS);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_ENROLLMENT_HANDLER_CHROMEOS_H_
