// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_HOST_SCANNER_OPERATION_H_
#define CHROMEOS_COMPONENTS_TETHER_HOST_SCANNER_OPERATION_H_

#include <map>
#include <vector>

#include "base/macros.h"
#include "base/observer_list.h"
#include "chromeos/components/tether/ble_connection_manager.h"
#include "chromeos/components/tether/message_transfer_operation.h"
#include "components/cryptauth/remote_device.h"

namespace chromeos {

namespace tether {

class HostScanDevicePrioritizer;
class MessageWrapper;

// Operation used to perform a host scan. Attempts to connect to each of the
// devices passed and sends a TetherAvailabilityRequest to each connected device
// once an authenticated channel has been established; once a response has been
// received, HostScannerOperation alerts observers of devices which can provide
// a tethering connection.
// TODO(khorimoto): Add a timeout which gives up if no response is received in
// a reasonable amount of time.
class HostScannerOperation : public MessageTransferOperation {
 public:
  class Factory {
   public:
    static std::unique_ptr<HostScannerOperation> NewInstance(
        const std::vector<cryptauth::RemoteDevice>& devices_to_connect,
        BleConnectionManager* connection_manager,
        HostScanDevicePrioritizer* host_scan_device_prioritizer);

    static void SetInstanceForTesting(Factory* factory);

   protected:
    virtual std::unique_ptr<HostScannerOperation> BuildInstance(
        const std::vector<cryptauth::RemoteDevice>& devices_to_connect,
        BleConnectionManager* connection_manager,
        HostScanDevicePrioritizer* host_scan_device_prioritizer);

   private:
    static Factory* factory_instance_;
  };

  struct ScannedDeviceInfo {
    ScannedDeviceInfo(const cryptauth::RemoteDevice& remote_device,
                      const DeviceStatus& device_status,
                      bool set_up_required);
    ~ScannedDeviceInfo();

    friend bool operator==(const ScannedDeviceInfo& first,
                           const ScannedDeviceInfo& second);

    cryptauth::RemoteDevice remote_device;
    DeviceStatus device_status;
    bool set_up_required;
  };

  class Observer {
   public:
    // Invoked once with an empty list when the operation begins, then invoked
    // repeatedly once each result comes in. After all devices have been
    // processed, the callback is invoked one final time with
    // |is_final_scan_result| = true.
    virtual void OnTetherAvailabilityResponse(
        std::vector<ScannedDeviceInfo>& scanned_device_list_so_far,
        bool is_final_scan_result) = 0;
  };

  HostScannerOperation(
      const std::vector<cryptauth::RemoteDevice>& devices_to_connect,
      BleConnectionManager* connection_manager,
      HostScanDevicePrioritizer* host_scan_device_prioritizer);
  ~HostScannerOperation() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  void NotifyObserversOfScannedDeviceList(bool is_final_scan_result);

  // MessageTransferOperation:
  void OnDeviceAuthenticated(
      const cryptauth::RemoteDevice& remote_device) override;
  void OnMessageReceived(std::unique_ptr<MessageWrapper> message_wrapper,
                         const cryptauth::RemoteDevice& remote_device) override;
  void OnOperationStarted() override;
  void OnOperationFinished() override;
  MessageType GetMessageTypeForConnection() override;

  std::vector<ScannedDeviceInfo> scanned_device_list_so_far_;

 private:
  friend class HostScannerOperationTest;

  HostScanDevicePrioritizer* host_scan_device_prioritizer_;
  base::ObserverList<Observer> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(HostScannerOperation);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_HOST_SCANNER_OPERATION_H_
