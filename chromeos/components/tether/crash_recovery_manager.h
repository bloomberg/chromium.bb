// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_CRASH_RECOVERY_MANAGER_H_
#define CHROMEOS_COMPONENTS_TETHER_CRASH_RECOVERY_MANAGER_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "chromeos/components/tether/active_host.h"

namespace chromeos {

class NetworkStateHandler;

namespace tether {

class HostScanCache;

// Restores Tether state after a browser crash.
class CrashRecoveryManager {
 public:
  class Factory {
   public:
    static std::unique_ptr<CrashRecoveryManager> NewInstance(
        NetworkStateHandler* network_state_handler,
        ActiveHost* active_host,
        HostScanCache* host_scan_cache);

    static void SetInstanceForTesting(Factory* factory);

   protected:
    virtual std::unique_ptr<CrashRecoveryManager> BuildInstance(
        NetworkStateHandler* network_state_handler,
        ActiveHost* active_host,
        HostScanCache* host_scan_cache);

   private:
    static Factory* factory_instance_;
  };

  CrashRecoveryManager(NetworkStateHandler* network_state_handler,
                       ActiveHost* active_host,
                       HostScanCache* host_scan_cache);
  virtual ~CrashRecoveryManager();

  // Restores state which was lost by a browser crash. If a crash did not occur
  // the last time that the Tether component was active, this function is a
  // no-op. If there was an active Tether connection and the browser crashed,
  // this function restores the Tether connection.
  //
  // This function should only be called during the initialization of the Tether
  // component.
  void RestorePreCrashStateIfNecessary(
      const base::Closure& on_restoration_finished);

 private:
  void RestoreConnectedState(const base::Closure& on_restoration_finished,
                             const std::string& active_host_device_id,
                             const std::string& tether_network_guid,
                             const std::string& wifi_network_guid);
  void OnActiveHostFetched(const base::Closure& on_restoration_finished,
                           ActiveHost::ActiveHostStatus active_host_status,
                           std::shared_ptr<cryptauth::RemoteDevice> active_host,
                           const std::string& tether_network_guid,
                           const std::string& wifi_network_guid);

  NetworkStateHandler* network_state_handler_;
  ActiveHost* active_host_;
  HostScanCache* host_scan_cache_;

  base::WeakPtrFactory<CrashRecoveryManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CrashRecoveryManager);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_CRASH_RECOVERY_MANAGER_H_
