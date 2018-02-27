// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_CONNECTION_PRESERVER_H_
#define CHROMEOS_COMPONENTS_TETHER_CONNECTION_PRESERVER_H_

#include <memory>

#include "base/timer/timer.h"
#include "chromeos/components/tether/active_host.h"
#include "chromeos/components/tether/connection_reason.h"

namespace chromeos {

class NetworkStateHandler;

namespace tether {

class BleConnectionManager;
class TetherHostResponseRecorder;

// Preserves a single BLE Connection beyond its immediately useful lifetime in
// the hope that the BLE Connection will be useful in the future -- thus
// preventing the need for a 2nd expensive setup of the Connection. This logic
// is only used after a host scan, in anticipation of a host connection attempt.
class ConnectionPreserver : public ActiveHost::Observer {
 public:
  // The maximum duration of time that a BLE Connection should be preserved.
  // A preserved BLE Connection will be torn down if not used within this time.
  // If the connection is used for a host connection before this time runs out,
  // the Connection will be torn down.
  static constexpr const uint32_t kTimeoutSeconds = 60;

  ConnectionPreserver(
      BleConnectionManager* ble_connection_manager,
      NetworkStateHandler* network_state_handler,
      ActiveHost* active_host,
      TetherHostResponseRecorder* tether_host_response_recorder);
  virtual ~ConnectionPreserver();

  // Should be called after each successful host scan result, to request that
  // the Connection with that device be preserved.
  void HandleSuccessfulTetherAvailabilityResponse(const std::string& device_id);

 protected:
  // ActiveHost::Observer:
  void OnActiveHostChanged(
      const ActiveHost::ActiveHostChangeInfo& change_info) override;

 private:
  friend class ConnectionPreserverTest;

  bool IsConnectedToInternet();
  // Between |preserved_connection_device_id_| and |device_id|, return which is
  // the "preferred" preserved Connection, i.e., which is higher priority.
  std::string GetPreferredPreservedConnectionDeviceId(
      const std::string& device_id);
  void SetPreservedConnection(const std::string& device_id);
  void RemovePreservedConnectionIfPresent();

  void SetTimerForTesting(std::unique_ptr<base::Timer> timer_for_test);

  BleConnectionManager* ble_connection_manager_;
  NetworkStateHandler* network_state_handler_;
  ActiveHost* active_host_;
  TetherHostResponseRecorder* tether_host_response_recorder_;
  std::unique_ptr<base::Timer> preserved_connection_timer_;

  std::string preserved_connection_device_id_;

  base::WeakPtrFactory<ConnectionPreserver> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ConnectionPreserver);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_CONNECTION_PRESERVER_H_
