// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/copresence_sockets/public/copresence_peer.h"

#include <stdint.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/format_macros.h"
#include "base/memory/scoped_ptr.h"
#include "components/copresence_sockets/transports/bluetooth/copresence_socket_bluetooth.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_socket.h"
#include "device/bluetooth/bluetooth_uuid.h"

namespace {

const char kAdapterError[] = "NOADAPTER";
const char kSocketServiceUuid[] = "2491fb14-0077-4d4d-bd41-b18e9a570f56";

// This class will confirm pairing for a device that is expecting a pairing
// confirmation.
class DefaultApprovalDelegate
    : public device::BluetoothDevice::PairingDelegate {
 public:
  DefaultApprovalDelegate() {}
  ~DefaultApprovalDelegate() override {}

  // device::BluetoothDevice::PairingDelegate overrides:
  void RequestPinCode(device::BluetoothDevice* device) override {}
  void RequestPasskey(device::BluetoothDevice* device) override {}
  void DisplayPinCode(device::BluetoothDevice* device,
                      const std::string& pincode) override {}
  void DisplayPasskey(device::BluetoothDevice* device,
                      uint32 passkey) override {}
  void KeysEntered(device::BluetoothDevice* device, uint32 entered) override {}
  void ConfirmPasskey(device::BluetoothDevice* device,
                      uint32 passkey) override {}
  void AuthorizePairing(device::BluetoothDevice* device) override {
    if (device->ExpectingConfirmation())
      device->ConfirmPairing();
  }
};

}  // namespace

namespace copresence_sockets {

CopresencePeer::CopresencePeer(const CreatePeerCallback& create_callback,
                               const AcceptCallback& accept_callback)
    : create_callback_(create_callback),
      accept_callback_(accept_callback),
      delegate_(nullptr),
      weak_ptr_factory_(this) {
  DCHECK(!create_callback.is_null());
  DCHECK(!accept_callback.is_null());

  if (!device::BluetoothAdapterFactory::IsBluetoothAdapterAvailable()) {
    create_callback_.Run(std::string());
    return;
  }

  device::BluetoothAdapterFactory::GetAdapter(base::Bind(
      &CopresencePeer::OnGetAdapter, weak_ptr_factory_.GetWeakPtr()));
}

std::string CopresencePeer::GetLocatorData() {
  if (!adapter_.get())
    return kAdapterError;
  // TODO(rkc): Fix the "1." once we have finalized the locator format with
  // other platforms. http://crbug.com/418616
  return "1." + adapter_->GetAddress() + "." + kSocketServiceUuid;
}

CopresencePeer::~CopresencePeer() {
  server_socket_->Disconnect(base::Bind(&base::DoNothing));
  server_socket_->Close();
  if (delegate_)
    adapter_->RemovePairingDelegate(delegate_.get());
}

// Private methods.

void CopresencePeer::OnGetAdapter(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  if (!adapter.get() || !adapter->IsPresent() || !adapter->IsPowered()) {
    LOG(WARNING) << "Unable to use BT adapter";
    create_callback_.Run(std::string());
    return;
  }

  adapter_ = adapter;
  delegate_ = make_scoped_ptr(new DefaultApprovalDelegate());
  VLOG(2) << "Got Adapter, creating service with UUID: " << kSocketServiceUuid;
  adapter_->AddPairingDelegate(
      delegate_.get(),
      device::BluetoothAdapter::PAIRING_DELEGATE_PRIORITY_HIGH);
  adapter_->CreateRfcommService(
      device::BluetoothUUID(kSocketServiceUuid),
      device::BluetoothAdapter::ServiceOptions(),
      base::Bind(&CopresencePeer::OnCreateService,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&CopresencePeer::OnCreateServiceError,
                 weak_ptr_factory_.GetWeakPtr()));
}

void CopresencePeer::OnCreateService(
    scoped_refptr<device::BluetoothSocket> socket) {
  if (!socket.get()) {
    LOG(WARNING) << "Couldn't create service!";
    create_callback_.Run(std::string());
    return;
  }

  VLOG(3) << "Starting Accept Socket.";
  server_socket_ = socket;
  create_callback_.Run(GetLocatorData());
  server_socket_->Accept(
      base::Bind(&CopresencePeer::OnAccept, weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&CopresencePeer::OnAcceptError,
                 weak_ptr_factory_.GetWeakPtr()));
}

void CopresencePeer::OnCreateServiceError(const std::string& message) {
  LOG(WARNING) << "Couldn't create Bluetooth service: " << message;
  create_callback_.Run(std::string());
}

void CopresencePeer::OnAccept(const device::BluetoothDevice* device,
                              scoped_refptr<device::BluetoothSocket> socket) {
  if (!socket.get())
    return;
  VLOG(3) << "Accepted Socket.";
  accept_callback_.Run(make_scoped_ptr(new CopresenceSocketBluetooth(socket)));
}

void CopresencePeer::OnAcceptError(const std::string& message) {
  LOG(WARNING) << "Couldn't accept Bluetooth connection: " << message;
}

}  // namespace copresence_sockets
