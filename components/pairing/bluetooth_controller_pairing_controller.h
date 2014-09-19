// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PAIRING_BLUETOOTH_CONTROLLER_PAIRING_FLOW_H_
#define CHROMEOS_PAIRING_BLUETOOTH_CONTROLLER_PAIRING_FLOW_H_

#include <set>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/threading/thread_checker.h"
#include "components/pairing/controller_pairing_controller.h"
#include "components/pairing/proto_decoder.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_socket.h"

namespace net {
class IOBuffer;
}

namespace pairing_chromeos {

class BluetoothControllerPairingController
    : public ControllerPairingController,
      public ProtoDecoder::Observer,
      public device::BluetoothAdapter::Observer,
      public device::BluetoothDevice::PairingDelegate  {
 public:
  BluetoothControllerPairingController();
  virtual ~BluetoothControllerPairingController();

 private:
  void ChangeStage(Stage new_stage);
  device::BluetoothDevice* GetController();
  void Reset();
  void DeviceFound(device::BluetoothDevice* device);
  void DeviceLost(device::BluetoothDevice* device);
  void SendBuffer(scoped_refptr<net::IOBuffer> io_buffer, int size);

  void OnSetPowered();
  void OnGetAdapter(scoped_refptr<device::BluetoothAdapter> adapter);
  void OnStartDiscoverySession(
      scoped_ptr<device::BluetoothDiscoverySession> discovery_session);
  void OnConnect();
  void OnConnectToService(scoped_refptr<device::BluetoothSocket> socket);
  void OnSendComplete(int bytes_sent);
  void OnReceiveComplete(int bytes, scoped_refptr<net::IOBuffer> io_buffer);

  void OnError();
  void OnErrorWithMessage(const std::string& message);
  void OnConnectError(device::BluetoothDevice::ConnectErrorCode error_code);
  void OnReceiveError(device::BluetoothSocket::ErrorReason reason,
                      const std::string& error_message);

  // ControllerPairingController:
  virtual void AddObserver(
      ControllerPairingController::Observer* observer) OVERRIDE;
  virtual void RemoveObserver(
      ControllerPairingController::Observer* observer) OVERRIDE;
  virtual Stage GetCurrentStage() OVERRIDE;
  virtual void StartPairing() OVERRIDE;
  virtual DeviceIdList GetDiscoveredDevices() OVERRIDE;
  virtual void ChooseDeviceForPairing(const std::string& device_id) OVERRIDE;
  virtual void RepeatDiscovery() OVERRIDE;
  virtual std::string GetConfirmationCode() OVERRIDE;
  virtual void SetConfirmationCodeIsCorrect(bool correct) OVERRIDE;
  virtual void SetHostConfiguration(
      bool accepted_eula,
      const std::string& lang,
      const std::string& timezone,
      bool send_reports,
      const std::string& keyboard_layout) OVERRIDE;
  virtual void OnAuthenticationDone(const std::string& domain,
                                    const std::string& auth_token) OVERRIDE;
  virtual void StartSession() OVERRIDE;

  // ProtoDecoder::Observer:
  virtual void OnHostStatusMessage(
      const pairing_api::HostStatus& message) OVERRIDE;
  virtual void OnConfigureHostMessage(
      const pairing_api::ConfigureHost& message) OVERRIDE;
  virtual void OnPairDevicesMessage(
      const pairing_api::PairDevices& message) OVERRIDE;
  virtual void OnCompleteSetupMessage(
      const pairing_api::CompleteSetup& message) OVERRIDE;
  virtual void OnErrorMessage(const pairing_api::Error& message) OVERRIDE;

  // BluetoothAdapter::Observer:
  virtual void DeviceAdded(device::BluetoothAdapter* adapter,
                           device::BluetoothDevice* device) OVERRIDE;
  virtual void DeviceRemoved(device::BluetoothAdapter* adapter,
                             device::BluetoothDevice* device) OVERRIDE;

  // device::BluetoothDevice::PairingDelegate:
  virtual void RequestPinCode(device::BluetoothDevice* device) OVERRIDE;
  virtual void RequestPasskey(device::BluetoothDevice* device) OVERRIDE;
  virtual void DisplayPinCode(device::BluetoothDevice* device,
                              const std::string& pincode) OVERRIDE;
  virtual void DisplayPasskey(device::BluetoothDevice* device,
                              uint32 passkey) OVERRIDE;
  virtual void KeysEntered(device::BluetoothDevice* device,
                           uint32 entered) OVERRIDE;
  virtual void ConfirmPasskey(device::BluetoothDevice* device,
                              uint32 passkey) OVERRIDE;
  virtual void AuthorizePairing(device::BluetoothDevice* device) OVERRIDE;

  Stage current_stage_;
  bool got_initial_status_;
  scoped_refptr<device::BluetoothAdapter> adapter_;
  scoped_ptr<device::BluetoothDiscoverySession> discovery_session_;
  scoped_refptr<device::BluetoothSocket> socket_;
  std::string controller_device_id_;

  std::string confirmation_code_;
  std::set<std::string> discovered_devices_;

  scoped_ptr<ProtoDecoder> proto_decoder_;

  base::ThreadChecker thread_checker_;
  ObserverList<ControllerPairingController::Observer> observers_;
  base::WeakPtrFactory<BluetoothControllerPairingController> ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothControllerPairingController);
};

}  // namespace pairing_chromeos

#endif  // CHROMEOS_PAIRING_BLUETOOTH_CONTROLLER_PAIRING_FLOW_H_
