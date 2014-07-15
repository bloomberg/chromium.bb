// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_CLOUD_POLICY_MANAGER_CHROMEOS_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_CLOUD_POLICY_MANAGER_CHROMEOS_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/policy/server_backed_state_keys_broker.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_manager.h"

namespace base {
class SequencedTaskRunner;
}

namespace chromeos {
namespace attestation {
class AttestationPolicyObserver;
}
}

class PrefRegistrySimple;
class PrefService;

namespace policy {

class DeviceCloudPolicyStoreChromeOS;

// CloudPolicyManager specialization for device policy on Chrome OS.
class DeviceCloudPolicyManagerChromeOS : public CloudPolicyManager {
 public:
  // |task_runner| is the runner for policy refresh tasks.
  DeviceCloudPolicyManagerChromeOS(
      scoped_ptr<DeviceCloudPolicyStoreChromeOS> store,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner,
      ServerBackedStateKeysBroker* state_keys_broker);
  virtual ~DeviceCloudPolicyManagerChromeOS();

  // Initializes state keys and requisition information.
  void Initialize(PrefService* local_state);

  // TODO(davidyu): Move these two functions to a more appropriate place. See
  // http://crbug.com/383695.
  // Gets/Sets the device requisition.
  std::string GetDeviceRequisition() const;
  void SetDeviceRequisition(const std::string& requisition);
  bool IsRemoraRequisition() const;
  bool IsSharkRequisition() const;

  // CloudPolicyManager:
  virtual void Shutdown() OVERRIDE;

  // Pref registration helper.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Returns the device serial number, or an empty string if not available.
  static std::string GetMachineID();

  // Returns the machine model, or an empty string if not available.
  static std::string GetMachineModel();

  // Returns the robot 'email address' associated with the device robot
  // account (sometimes called a service account) associated with this device
  // during enterprise enrollment.
  std::string GetRobotAccountId();

  // Starts the connection via |client_to_connect|.
  void StartConnection(scoped_ptr<CloudPolicyClient> client_to_connect,
                       scoped_ptr<CloudPolicyClient::StatusProvider>
                           device_status_provider);

  DeviceCloudPolicyStoreChromeOS* device_store() {
    return device_store_.get();
  }

 private:
  // Saves the state keys received from |session_manager_client_|.
  void OnStateKeysUpdated();

  // Initializes requisition settings at OOBE with values from VPD.
  void InitializeRequisition();

  // Points to the same object as the base CloudPolicyManager::store(), but with
  // actual device policy specific type.
  scoped_ptr<DeviceCloudPolicyStoreChromeOS> device_store_;
  ServerBackedStateKeysBroker* state_keys_broker_;

  ServerBackedStateKeysBroker::Subscription state_keys_update_subscription_;

  // PrefService instance to read the policy refresh rate from.
  PrefService* local_state_;

  scoped_ptr<chromeos::attestation::AttestationPolicyObserver>
      attestation_policy_observer_;

  // TODO(davidyu): Currently we need to keep this object alive while
  // CloudPolicyClient is in use. We should have CPC take over the
  // ownership of this object instead. See http://crbug.com/383696.
  scoped_ptr<CloudPolicyClient::StatusProvider> device_status_provider_;

  DISALLOW_COPY_AND_ASSIGN(DeviceCloudPolicyManagerChromeOS);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_CLOUD_POLICY_MANAGER_CHROMEOS_H_
