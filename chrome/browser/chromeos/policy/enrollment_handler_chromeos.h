// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_ENROLLMENT_HANDLER_CHROMEOS_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_ENROLLMENT_HANDLER_CHROMEOS_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_initializer.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_validator.h"
#include "chrome/browser/chromeos/policy/enterprise_install_attributes.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "google_apis/gaia/gaia_oauth_client.h"
#include "policy/proto/device_management_backend.pb.h"

namespace base {
class SequencedTaskRunner;
}

namespace chromeos {
class DeviceSettingsService;
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
  typedef DeviceCloudPolicyInitializer::AllowedDeviceModes
      AllowedDeviceModes;
  typedef DeviceCloudPolicyInitializer::EnrollmentCallback
      EnrollmentCallback;

  // |store| and |install_attributes| must remain valid for the life time of the
  // enrollment handler. |allowed_device_modes| determines what device modes
  // are acceptable. If the mode specified by the server is not acceptable,
  // enrollment will fail with an EnrollmentStatus indicating
  // STATUS_REGISTRATION_BAD_MODE.
  // |management_mode| should be either ENTERPRISE_MANAGED or CONSUMER_MANAGED.
  EnrollmentHandlerChromeOS(
      DeviceCloudPolicyStoreChromeOS* store,
      EnterpriseInstallAttributes* install_attributes,
      ServerBackedStateKeysBroker* state_keys_broker,
      chromeos::DeviceSettingsService* device_settings_service,
      scoped_ptr<CloudPolicyClient> client,
      scoped_refptr<base::SequencedTaskRunner> background_task_runner,
      const std::string& auth_token,
      const std::string& client_id,
      bool is_auto_enrollment,
      const std::string& requisition,
      const AllowedDeviceModes& allowed_device_modes,
      enterprise_management::PolicyData::ManagementMode management_mode,
      const EnrollmentCallback& completion_callback);
  virtual ~EnrollmentHandlerChromeOS();

  // Starts the enrollment process and reports the result to
  // |completion_callback_|.
  void StartEnrollment();

  // Releases the client.
  scoped_ptr<CloudPolicyClient> ReleaseClient();

  // CloudPolicyClient::Observer:
  virtual void OnPolicyFetched(CloudPolicyClient* client) OVERRIDE;
  virtual void OnRegistrationStateChanged(CloudPolicyClient* client) OVERRIDE;
  virtual void OnRobotAuthCodesFetched(CloudPolicyClient* client) OVERRIDE;
  virtual void OnClientError(CloudPolicyClient* client) OVERRIDE;

  // CloudPolicyStore::Observer:
  virtual void OnStoreLoaded(CloudPolicyStore* store) OVERRIDE;
  virtual void OnStoreError(CloudPolicyStore* store) OVERRIDE;

  // GaiaOAuthClient::Delegate:
  virtual void OnGetTokensResponse(const std::string& refresh_token,
                                   const std::string& access_token,
                                   int expires_in_seconds) OVERRIDE;
  virtual void OnRefreshTokenResponse(const std::string& access_token,
                                      int expires_in_seconds) OVERRIDE;
  virtual void OnOAuthError() OVERRIDE;
  virtual void OnNetworkError(int response_code) OVERRIDE;

 private:
  // Indicates what step of the process is currently pending. These steps need
  // to be listed in the order they are traversed in.
  enum EnrollmentStep {
    STEP_PENDING,             // Not started yet.
    STEP_STATE_KEYS,          // Waiting for state keys to become available.
    STEP_LOADING_STORE,       // Waiting for |store_| to initialize.
    STEP_REGISTRATION,        // Currently registering the client.
    STEP_POLICY_FETCH,        // Fetching policy.
    STEP_VALIDATION,          // Policy validation.
    STEP_ROBOT_AUTH_FETCH,    // Fetching device API auth code.
    STEP_ROBOT_AUTH_REFRESH,  // Fetching device API refresh token.
    STEP_LOCK_DEVICE,         // Writing installation-time attributes.
    STEP_STORE_TOKEN_AND_ID,  // Storing DM token and virtual device ID.
    STEP_STORE_ROBOT_AUTH,    // Encrypting & writing robot refresh token.
    STEP_STORE_POLICY,        // Storing policy and API refresh token.
    STEP_FINISHED,            // Enrollment process finished, no further action.
  };

  // Handles the response to a request for server-backed state keys.
  void CheckStateKeys(const std::vector<std::string>& state_keys,
                      bool first_boot);

  // Starts registration if the store is initialized.
  void AttemptRegistration();

  // Handles the policy validation result, proceeding with installation-time
  // attributes locking if successful.
  void PolicyValidated(DeviceCloudPolicyValidator* validator);

  // Calls LockDevice() and proceeds to policy installation. If unsuccessful,
  // reports the result. Actual installation or error report will be done in
  // HandleLockDeviceResult().
  void StartLockDevice();

  // Checks the status after SetManagementSettings() is done. Proceeds to
  // robot auth code storing if successful.
  void OnSetManagementSettingsDone();

  // Helper for StartLockDevice(). It performs the actual action based on
  // the result of LockDevice.
  void HandleLockDeviceResult(
      EnterpriseInstallAttributes::LockResult lock_result);

  // Stores robot auth token.
  void StoreRobotAuth();

  // Handles completion of the robot token store operation.
  void HandleRobotAuthTokenStored(bool result);

  // Drops any ongoing actions.
  void Stop();

  // Reports the result of the enrollment process to the initiator.
  void ReportResult(EnrollmentStatus status);

  DeviceCloudPolicyStoreChromeOS* store_;
  EnterpriseInstallAttributes* install_attributes_;
  ServerBackedStateKeysBroker* state_keys_broker_;
  chromeos::DeviceSettingsService* device_settings_service_;
  scoped_ptr<CloudPolicyClient> client_;
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;
  scoped_ptr<gaia::GaiaOAuthClient> gaia_oauth_client_;

  std::string auth_token_;
  std::string client_id_;
  bool is_auto_enrollment_;
  std::string requisition_;
  std::string current_state_key_;
  std::string refresh_token_;
  AllowedDeviceModes allowed_device_modes_;
  enterprise_management::PolicyData::ManagementMode management_mode_;
  EnrollmentCallback completion_callback_;

  // The device mode as received in the registration request.
  DeviceMode device_mode_;

  // The validated policy response info to be installed in the store.
  scoped_ptr<enterprise_management::PolicyFetchResponse> policy_;
  std::string username_;
  std::string device_id_;
  std::string request_token_;

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
