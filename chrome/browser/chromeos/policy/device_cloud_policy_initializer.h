// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_CLOUD_POLICY_INITIALIZER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_CLOUD_POLICY_INITIALIZER_H_

#include <bitset>
#include <string>

#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/policy/server_backed_state_keys_broker.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"

class PrefService;

namespace base {
class SequencedTaskRunner;
}

namespace chromeos {
class OwnerSettingsServiceChromeOS;
}

namespace policy {

class DeviceCloudPolicyManagerChromeOS;
class DeviceCloudPolicyStoreChromeOS;
class DeviceManagementService;
struct EnrollmentConfig;
class EnrollmentHandlerChromeOS;
class EnrollmentStatus;
class EnterpriseInstallAttributes;

// This class connects DCPM to the correct device management service, and
// handles the enrollment process.
class DeviceCloudPolicyInitializer : public CloudPolicyStore::Observer {
 public:
  typedef std::bitset<32> AllowedDeviceModes;
  typedef base::Callback<void(EnrollmentStatus)> EnrollmentCallback;

  // |background_task_runner| is used to execute long-running background tasks
  // that may involve file I/O.
  DeviceCloudPolicyInitializer(
      PrefService* local_state,
      DeviceManagementService* enterprise_service,
      DeviceManagementService* consumer_service,
      const scoped_refptr<base::SequencedTaskRunner>& background_task_runner,
      EnterpriseInstallAttributes* install_attributes,
      ServerBackedStateKeysBroker* state_keys_broker,
      DeviceCloudPolicyStoreChromeOS* device_store,
      DeviceCloudPolicyManagerChromeOS* manager);

  ~DeviceCloudPolicyInitializer() override;

  virtual void Init();
  virtual void Shutdown();

  // Starts enrollment or re-enrollment. Once the enrollment process completes,
  // |enrollment_callback| is invoked and gets passed the status of the
  // operation.
  // |allowed_modes| specifies acceptable DEVICE_MODE_* constants for
  // enrollment.
  // |management_mode| should be either MANAGEMENT_MODE_ENTERPRISE or
  // MANAGEMENT_MODE_CONSUMER.
  virtual void StartEnrollment(
      ManagementMode management_mode,
      DeviceManagementService* device_management_service,
      chromeos::OwnerSettingsServiceChromeOS* owner_settings_service,
      const EnrollmentConfig& enrollment_config,
      const std::string& auth_token,
      const AllowedDeviceModes& allowed_modes,
      const EnrollmentCallback& enrollment_callback);

  // Get the enrollment configuration that has been set up via signals such as
  // device requisition, OEM manifest, pre-existing installation-time attributes
  // or server-backed state retrieval. The configuration is stored in |config|,
  // |config.mode| will be MODE_NONE if there is no prescribed configuration.
  // |config.management_domain| will contain the domain the device is supposed
  // to be enrolled to as decided by factors such as forced re-enrollment,
  // enrollment recovery, or already-present install attributes. Note that
  // |config.management_domain| may be non-empty even if |config.mode| is
  // MODE_NONE.
  EnrollmentConfig GetPrescribedEnrollmentConfig() const;

  // CloudPolicyStore::Observer:
  void OnStoreLoaded(CloudPolicyStore* store) override;
  void OnStoreError(CloudPolicyStore* store) override;

 private:
  // Handles completion signaled by |enrollment_handler_|.
  void EnrollmentCompleted(const EnrollmentCallback& enrollment_callback,
                           EnrollmentStatus status);

  // Creates a new CloudPolicyClient.
  scoped_ptr<CloudPolicyClient> CreateClient(
      DeviceManagementService* device_management_service);

  void TryToCreateClient();
  void StartConnection(scoped_ptr<CloudPolicyClient> client);

  PrefService* local_state_;
  DeviceManagementService* enterprise_service_;
  DeviceManagementService* consumer_service_;
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;
  EnterpriseInstallAttributes* install_attributes_;
  ServerBackedStateKeysBroker* state_keys_broker_;
  DeviceCloudPolicyStoreChromeOS* device_store_;
  DeviceCloudPolicyManagerChromeOS* manager_;
  bool is_initialized_;

  // Non-NULL if there is an enrollment operation pending.
  scoped_ptr<EnrollmentHandlerChromeOS> enrollment_handler_;

  ServerBackedStateKeysBroker::Subscription state_keys_update_subscription_;

  DISALLOW_COPY_AND_ASSIGN(DeviceCloudPolicyInitializer);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_CLOUD_POLICY_INITIALIZER_H_
