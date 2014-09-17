// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PAIRING_BLUETOOTH_HOST_PAIRING_CONTROLLER_H_
#define CHROMEOS_PAIRING_BLUETOOTH_HOST_PAIRING_CONTROLLER_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/threading/thread_checker.h"
#include "components/pairing/host_pairing_controller.h"
#include "components/pairing/proto_decoder.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_socket.h"

namespace device {
class BluetoothAdapter;
}

namespace net {
class IOBuffer;
}

namespace pairing_chromeos {

class BluetoothHostPairingController
    : public HostPairingController,
      public ProtoDecoder::Observer,
      public device::BluetoothAdapter::Observer,
      public device::BluetoothDevice::PairingDelegate {
 public:
  typedef HostPairingController::Observer Observer;

  BluetoothHostPairingController();
  virtual ~BluetoothHostPairingController();

 private:
  void ChangeStage(Stage new_stage);
  void SendHostStatus();
  void AbortWithError(int code, const std::string& message);
  void Reset();

  void OnGetAdapter(scoped_refptr<device::BluetoothAdapter> adapter);
  void SetName();
  void OnSetName();
  void OnSetPowered();
  void OnCreateService(scoped_refptr<device::BluetoothSocket> socket);
  void OnSetDiscoverable(bool change_stage);
  void OnAccept(const device::BluetoothDevice* device,
                scoped_refptr<device::BluetoothSocket> socket);
  void OnSendComplete(int bytes_sent);
  void OnReceiveComplete(int bytes, scoped_refptr<net::IOBuffer> io_buffer);

  void OnCreateServiceError(const std::string& message);
  void OnSetError();
  void OnAcceptError(const std::string& error_message);
  void OnSendError(const std::string& error_message);
  void OnReceiveError(device::BluetoothSocket::ErrorReason reason,
                      const std::string& error_message);

  // HostPairingController:
  virtual void AddObserver(Observer* observer) OVERRIDE;
  virtual void RemoveObserver(Observer* observer) OVERRIDE;
  virtual Stage GetCurrentStage() OVERRIDE;
  virtual void StartPairing() OVERRIDE;
  virtual std::string GetDeviceName() OVERRIDE;
  virtual std::string GetConfirmationCode() OVERRIDE;
  virtual std::string GetEnrollmentDomain() OVERRIDE;
  virtual void OnUpdateStatusChanged(UpdateStatus update_status) OVERRIDE;
  virtual void SetEnrollmentComplete(bool success) OVERRIDE;

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
  virtual void AdapterPresentChanged(device::BluetoothAdapter* adapter,
                                     bool present) OVERRIDE;

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
  std::string device_name_;
  std::string confirmation_code_;
  std::string enrollment_domain_;

  scoped_refptr<device::BluetoothAdapter> adapter_;
  device::BluetoothDevice* device_;
  scoped_refptr<device::BluetoothSocket> service_socket_;
  scoped_refptr<device::BluetoothSocket> controller_socket_;
  scoped_ptr<ProtoDecoder> proto_decoder_;

  base::ThreadChecker thread_checker_;
  ObserverList<Observer> observers_;
  base::WeakPtrFactory<BluetoothHostPairingController> ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothHostPairingController);
};

}  // namespace pairing_chromeos

#endif  // CHROMEOS_PAIRING_BLUETOOTH_HOST_PAIRING_CONTROLLER_H_
