// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_ENROLLMENT_HANDLER_CHROMEOS_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_ENROLLMENT_HANDLER_CHROMEOS_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_validator.h"
#include "chrome/browser/chromeos/policy/enterprise_install_attributes.h"
#include "chrome/browser/policy/cloud/cloud_policy_client.h"
#include "chrome/browser/policy/cloud/cloud_policy_store.h"

namespace enterprise_management {
class PolicyFetchResponse;
}

namespace policy {

// Implements the logic that establishes enterprise enrollment for Chromium OS
// devices. The process is as follows:
//   1. Given an auth token, register with the policy service.
//   2. Download the initial policy blob from the service.
//   3. Verify the policy blob. Everything up to this point doesn't touch device
//      state.
//   4. Establish the device lock in installation-time attributes.
//   5. Store the policy blob.
class EnrollmentHandlerChromeOS : public CloudPolicyClient::Observer,
                                  public CloudPolicyStore::Observer {
 public:
  typedef DeviceCloudPolicyManagerChromeOS::AllowedDeviceModes
      AllowedDeviceModes;
  typedef DeviceCloudPolicyManagerChromeOS::EnrollmentCallback
      EnrollmentCallback;

  // |store| and |install_attributes| must remain valid for the life time of the
  // enrollment handler. |allowed_device_modes| determines what device modes
  // are acceptable. If the mode specified by the server is not acceptable,
  // enrollment will fail with an EnrollmentStatus indicating
  // STATUS_REGISTRATION_BAD_MODE.
  EnrollmentHandlerChromeOS(DeviceCloudPolicyStoreChromeOS* store,
                            EnterpriseInstallAttributes* install_attributes,
                            scoped_ptr<CloudPolicyClient> client,
                            const std::string& auth_token,
                            const std::string& client_id,
                            bool is_auto_enrollment,
                            const AllowedDeviceModes& allowed_device_modes,
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
  virtual void OnClientError(CloudPolicyClient* client) OVERRIDE;

  // CloudPolicyStore::Observer:
  virtual void OnStoreLoaded(CloudPolicyStore* store) OVERRIDE;
  virtual void OnStoreError(CloudPolicyStore* store) OVERRIDE;

 private:
  // Indicates what step of the process is currently pending. These steps need
  // to be listed in the order they are traversed in.
  enum EnrollmentStep {
    STEP_PENDING,        // Not started yet.
    STEP_LOADING_STORE,  // Waiting for |store_| to initialize.
    STEP_REGISTRATION,   // Currently registering the client.
    STEP_POLICY_FETCH,   // Fetching policy.
    STEP_VALIDATION,     // Policy validation.
    STEP_LOCK_DEVICE,    // Writing installation-time attributes.
    STEP_STORE_POLICY,   // Storing policy.
    STEP_FINISHED,       // Enrollment process finished, no further action.
  };

  // Starts registration if the store is initialized.
  void AttemptRegistration();

  // Handles the policy validation result, proceeding with installation-time
  // attributes locking if successful.
  void PolicyValidated(DeviceCloudPolicyValidator* validator);

  // Calls LockDevice() and proceeds to policy installation. If unsuccessful,
  // reports the result. Actual installation or error report will be done in
  // HandleLockDeviceResult().
  void WriteInstallAttributes(const std::string& user,
                              DeviceMode device_mode,
                              const std::string& device_id);

  // Helper for WriteInstallAttributes(). It performs the actual action based on
  // the result of LockDevice.
  void HandleLockDeviceResult(
      const std::string& user,
      DeviceMode device_mode,
      const std::string& device_id,
      EnterpriseInstallAttributes::LockResult lock_result);

  // Drops any ongoing actions.
  void Stop();

  // Reports the result of the enrollment process to the initiator.
  void ReportResult(EnrollmentStatus status);

  DeviceCloudPolicyStoreChromeOS* store_;
  EnterpriseInstallAttributes* install_attributes_;
  scoped_ptr<CloudPolicyClient> client_;

  std::string auth_token_;
  std::string client_id_;
  bool is_auto_enrollment_;
  AllowedDeviceModes allowed_device_modes_;
  EnrollmentCallback completion_callback_;

  // The device mode as received in the registration request.
  DeviceMode device_mode_;

  // The validated policy response to be installed in the store.
  scoped_ptr<enterprise_management::PolicyFetchResponse> policy_;

  // Current enrollment step.
  EnrollmentStep enrollment_step_;

  // Total amount of time in milliseconds spent waiting for lockbox
  // initialization.
  int lockbox_init_duration_;

  base::WeakPtrFactory<EnrollmentHandlerChromeOS> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(EnrollmentHandlerChromeOS);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_ENROLLMENT_HANDLER_CHROMEOS_H_
