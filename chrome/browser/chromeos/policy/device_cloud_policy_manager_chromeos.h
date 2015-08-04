// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_CLOUD_POLICY_MANAGER_CHROMEOS_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_CLOUD_POLICY_MANAGER_CHROMEOS_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
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
class EnterpriseInstallAttributes;
class HeartbeatScheduler;
class StatusUploader;
class SystemLogUploader;

// CloudPolicyManager specialization for device policy on Chrome OS.
class DeviceCloudPolicyManagerChromeOS : public CloudPolicyManager {
 public:
  class Observer {
   public:
    // Invoked when the device cloud policy manager connects.
    virtual void OnDeviceCloudPolicyManagerConnected() = 0;
    // Invoked when the device cloud policy manager disconnects.
    virtual void OnDeviceCloudPolicyManagerDisconnected() = 0;
  };

  using UnregisterCallback = base::Callback<void(bool)>;

  // |task_runner| is the runner for policy refresh, heartbeat, and status
  // upload tasks.
  DeviceCloudPolicyManagerChromeOS(
      scoped_ptr<DeviceCloudPolicyStoreChromeOS> store,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner,
      ServerBackedStateKeysBroker* state_keys_broker);
  ~DeviceCloudPolicyManagerChromeOS() override;

  // Initializes state keys and requisition information.
  void Initialize(PrefService* local_state);

  void AddDeviceCloudPolicyManagerObserver(Observer* observer);
  void RemoveDeviceCloudPolicyManagerObserver(Observer* observer);

  // TODO(davidyu): Move these two functions to a more appropriate place. See
  // http://crbug.com/383695.
  // Gets/Sets the device requisition.
  std::string GetDeviceRequisition() const;
  void SetDeviceRequisition(const std::string& requisition);
  bool IsRemoraRequisition() const;
  bool IsSharkRequisition() const;

  // CloudPolicyManager:
  void Shutdown() override;

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
                       EnterpriseInstallAttributes* install_attributes);

  // Sends the unregister request. |callback| is invoked with a boolean
  // parameter indicating the result when done.
  virtual void Unregister(const UnregisterCallback& callback);

  // Disconnects the manager.
  virtual void Disconnect();

  DeviceCloudPolicyStoreChromeOS* device_store() {
    return device_store_.get();
  }

  // Return the StatusUploader used to communicate device status to the
  // policy server.
  StatusUploader* GetStatusUploader() const { return status_uploader_.get(); }

 private:
  // Saves the state keys received from |session_manager_client_|.
  void OnStateKeysUpdated();

  // Initializes requisition settings at OOBE with values from VPD.
  void InitializeRequisition();

  void NotifyConnected();
  void NotifyDisconnected();

  // Factory function to create the StatusUploader.
  void CreateStatusUploader();

  // Points to the same object as the base CloudPolicyManager::store(), but with
  // actual device policy specific type.
  scoped_ptr<DeviceCloudPolicyStoreChromeOS> device_store_;
  ServerBackedStateKeysBroker* state_keys_broker_;

  // Helper object that handles updating the server with our current device
  // state.
  scoped_ptr<StatusUploader> status_uploader_;

  // Helper object that handles uploading system logs to the server.
  scoped_ptr<SystemLogUploader> syslog_uploader_;

  // Helper object that handles sending heartbeats over the GCM channel to
  // the server, to monitor connectivity.
  scoped_ptr<HeartbeatScheduler> heartbeat_scheduler_;

  // The TaskRunner used to do device status and log uploads.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  ServerBackedStateKeysBroker::Subscription state_keys_update_subscription_;

  // PrefService instance to read the policy refresh rate from.
  PrefService* local_state_;

  scoped_ptr<chromeos::attestation::AttestationPolicyObserver>
      attestation_policy_observer_;

  base::ObserverList<Observer, true> observers_;

  DISALLOW_COPY_AND_ASSIGN(DeviceCloudPolicyManagerChromeOS);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_CLOUD_POLICY_MANAGER_CHROMEOS_H_
