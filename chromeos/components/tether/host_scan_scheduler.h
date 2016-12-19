// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_HOST_SCAN_SCHEDULER_H
#define CHROMEOS_COMPONENTS_TETHER_HOST_SCAN_SCHEDULER_H

#include "chromeos/dbus/power_manager_client.h"
#include "chromeos/login/login_state.h"
#include "chromeos/network/network_state_handler_observer.h"
#include "components/cryptauth/cryptauth_device_manager.h"

namespace content {
class BrowserContext;
}

namespace cryptauth {
class CryptAuthDeviceManager;
}

namespace chromeos {

class LoginState;
class PowerManagerClient;

namespace tether {

class HostScanner;

// Schedules scans for tether hosts. To start a scan attempt, three conditions
// must be true:
//
//   (1) The user has just logged in or has just resumed using the device after
//       it had been sleeping/suspended.
//   (2) The device does not have an Internet connection.
//   (3) The device has synced data about other devices belonging to the user's
//       account, and at least one of those devices is capable of being a tether
//       host (i.e., it has a mobile data connection).
//
// When one of those conditions changes, this class checks the conditions and
// starts a scan automatically. Alternatively, a scan can be explicitly
// triggered via ScheduleScanNowIfPossible().
class HostScanScheduler : public LoginState::Observer,
                          public PowerManagerClient::Observer,
                          public NetworkStateHandlerObserver,
                          public cryptauth::CryptAuthDeviceManager::Observer {
 public:
  HostScanScheduler(const content::BrowserContext* browser_context,
                    std::unique_ptr<HostScanner> host_scanner);
  ~HostScanScheduler() override;

  // Sets up listeners so that scans can be automatically triggered when
  // needed.
  void InitializeAutomaticScans();

  // Schedules a scan now if the three conditions described above are met and
  // returns whether the scan was started.
  bool ScheduleScanNowIfPossible();

  // LoginState::Observer
  void LoggedInStateChanged() override;

  // PowerManagerClient::Observer
  void SuspendDone(const base::TimeDelta& sleep_duration) override;

  // NetworkStateHandlerObserver
  void NetworkConnectionStateChanged(const NetworkState* network) override;

  // cryptauth::CryptAuthDeviceManager::Observer
  void OnSyncFinished(cryptauth::CryptAuthDeviceManager::SyncResult sync_result,
                      cryptauth::CryptAuthDeviceManager::DeviceChangeResult
                          device_change_result) override;

 private:
  friend class HostScanSchedulerTest;

  class Delegate {
   public:
    virtual void AddObserver(HostScanScheduler* host_scan_scheduler) = 0;
    virtual void RemoveObserver(HostScanScheduler* host_scan_scheduler) = 0;
    virtual bool IsAuthenticatedUserLoggedIn() const = 0;
    virtual bool IsNetworkConnectedOrConnecting() const = 0;
    virtual bool AreTetherHostsSynced() const = 0;
  };

  class DelegateImpl : public Delegate {
   public:
    DelegateImpl(const content::BrowserContext* browser_context);

    void AddObserver(HostScanScheduler* host_scan_scheduler) override;
    void RemoveObserver(HostScanScheduler* host_scan_scheduler) override;
    bool IsAuthenticatedUserLoggedIn() const override;
    bool IsNetworkConnectedOrConnecting() const override;
    bool AreTetherHostsSynced() const override;
  };

  HostScanScheduler(std::unique_ptr<Delegate> delegate,
                    std::unique_ptr<HostScanner> host_scanner);

  std::unique_ptr<Delegate> delegate_;
  std::unique_ptr<HostScanner> host_scanner_;
  bool initialized_;

  DISALLOW_COPY_AND_ASSIGN(HostScanScheduler);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_HOST_SCAN_SCHEDULER_H
