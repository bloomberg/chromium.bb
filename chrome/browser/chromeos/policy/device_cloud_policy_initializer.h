// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_CLOUD_POLICY_INITIALIZER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_CLOUD_POLICY_INITIALIZER_H_

#include <bitset>
#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/policy/enrollment_status_chromeos.h"
#include "chrome/browser/chromeos/policy/server_backed_state_keys_broker.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "policy/proto/device_management_backend.pb.h"

class PrefService;

namespace base {
class SequencedTaskRunner;
}

namespace chromeos {
class DeviceSettingsService;
}

namespace policy {

class DeviceCloudPolicyManagerChromeOS;
class DeviceCloudPolicyStoreChromeOS;
class DeviceManagementService;
class EnrollmentHandlerChromeOS;
class EnterpriseInstallAttributes;

// This class connects DCPM to the correct device management service, and
// handles the enrollment process.
class DeviceCloudPolicyInitializer : public CloudPolicyStore::Observer {
 public:
  typedef std::bitset<32> AllowedDeviceModes;
  typedef base::Callback<void(EnrollmentStatus)> EnrollmentCallback;

  // |background_task_runner| is used to execute long-running background tasks
  // that may involve file I/O.
  // |on_connected_callback| is invoked after the device cloud policy manager
  // is connected.
  DeviceCloudPolicyInitializer(
      PrefService* local_state,
      DeviceManagementService* enterprise_service,
      DeviceManagementService* consumer_service,
      const scoped_refptr<base::SequencedTaskRunner>& background_task_runner,
      EnterpriseInstallAttributes* install_attributes,
      ServerBackedStateKeysBroker* state_keys_broker,
      DeviceCloudPolicyStoreChromeOS* device_store,
      DeviceCloudPolicyManagerChromeOS* manager,
      chromeos::DeviceSettingsService* device_settings_service,
      const base::Closure& on_connected_callback);

  virtual ~DeviceCloudPolicyInitializer();

  void Shutdown();

  // Starts enrollment or re-enrollment. Once the enrollment process completes,
  // |enrollment_callback| is invoked and gets passed the status of the
  // operation.
  // |allowed_modes| specifies acceptable DEVICE_MODE_* constants for
  // enrollment.
  // |management_mode| should be either ENTERPRISE_MANAGED or CONSUMER_MANAGED.
  void StartEnrollment(
      enterprise_management::PolicyData::ManagementMode management_mode,
      DeviceManagementService* device_management_service,
      const std::string& auth_token,
      bool is_auto_enrollment,
      const AllowedDeviceModes& allowed_modes,
      const EnrollmentCallback& enrollment_callback);

  // Checks whether enterprise enrollment should be a regular step during OOBE.
  bool ShouldAutoStartEnrollment() const;

  // Checks whether enterprise enrollment recovery is required.
  bool ShouldRecoverEnrollment() const;

  // Looks up the domain from |install_attributes_|.
  std::string GetEnrollmentRecoveryDomain() const;

  // Checks whether the user can cancel enrollment.
  bool CanExitEnrollment() const;

  // Gets the domain this device is supposed to be enrolled to.
  std::string GetForcedEnrollmentDomain() const;

  // CloudPolicyStore::Observer:
  virtual void OnStoreLoaded(CloudPolicyStore* store) OVERRIDE;
  virtual void OnStoreError(CloudPolicyStore* store) OVERRIDE;

 private:
  // Handles completion signaled by |enrollment_handler_|.
  void EnrollmentCompleted(const EnrollmentCallback& enrollment_callback,
                           EnrollmentStatus status);

  // Creates a new CloudPolicyClient.
  scoped_ptr<CloudPolicyClient> CreateClient(
      DeviceManagementService* device_management_service);

  void TryToCreateClient();
  void StartConnection(scoped_ptr<CloudPolicyClient> client);

  // Gets the device restore mode as stored in |local_state_|.
  std::string GetRestoreMode() const;

  PrefService* local_state_;
  DeviceManagementService* enterprise_service_;
  DeviceManagementService* consumer_service_;
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;
  EnterpriseInstallAttributes* install_attributes_;
  ServerBackedStateKeysBroker* state_keys_broker_;
  DeviceCloudPolicyStoreChromeOS* device_store_;
  DeviceCloudPolicyManagerChromeOS* manager_;
  chromeos::DeviceSettingsService* device_settings_service_;
  base::Closure on_connected_callback_;

  // Non-NULL if there is an enrollment operation pending.
  scoped_ptr<EnrollmentHandlerChromeOS> enrollment_handler_;

  ServerBackedStateKeysBroker::Subscription state_keys_update_subscription_;

  scoped_ptr<CloudPolicyClient::StatusProvider> device_status_provider_;

  DISALLOW_COPY_AND_ASSIGN(DeviceCloudPolicyInitializer);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_CLOUD_POLICY_INITIALIZER_H_
