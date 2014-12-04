// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/copresence_endpoints/public/copresence_endpoint.h"

#include <stdint.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/format_macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "components/copresence_endpoints/transports/bluetooth/copresence_socket_bluetooth.h"
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

namespace copresence_endpoints {

CopresenceEndpoint::CopresenceEndpoint(
    int endpoint_id,
    const CreateEndpointCallback& create_callback,
    const base::Closure& accept_callback,
    const CopresenceSocket::ReceiveCallback& receive_callback)
    : endpoint_id_(endpoint_id),
      create_callback_(create_callback),
      accept_callback_(accept_callback),
      receive_callback_(receive_callback),
      delegate_(nullptr),
      weak_ptr_factory_(this) {
  CHECK(!create_callback.is_null());
  CHECK(!accept_callback.is_null());
  CHECK(!receive_callback.is_null());

  if (!device::BluetoothAdapterFactory::IsBluetoothAdapterAvailable()) {
    create_callback_.Run(std::string());
    return;
  }

  device::BluetoothAdapterFactory::GetAdapter(base::Bind(
      &CopresenceEndpoint::OnGetAdapter, weak_ptr_factory_.GetWeakPtr()));
}

CopresenceEndpoint::~CopresenceEndpoint() {
  server_socket_->Disconnect(base::Bind(&base::DoNothing));
  server_socket_->Close();
  if (delegate_)
    adapter_->RemovePairingDelegate(delegate_.get());
}

std::string CopresenceEndpoint::GetLocator() {
  if (!adapter_.get())
    return kAdapterError;
  return base::IntToString(endpoint_id_) + "." + adapter_->GetAddress() + "." +
         kSocketServiceUuid;
}

bool CopresenceEndpoint::Send(const scoped_refptr<net::IOBuffer>& buffer,
                              int buffer_size) {
  if (!client_socket_)
    return false;

  return client_socket_->Send(buffer, buffer_size);
}

// Private methods.

void CopresenceEndpoint::OnGetAdapter(
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
      base::Bind(&CopresenceEndpoint::OnCreateService,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&CopresenceEndpoint::OnCreateServiceError,
                 weak_ptr_factory_.GetWeakPtr()));
}

void CopresenceEndpoint::OnCreateService(
    scoped_refptr<device::BluetoothSocket> socket) {
  if (!socket.get()) {
    LOG(WARNING) << "Couldn't create service!";
    create_callback_.Run(std::string());
    return;
  }

  VLOG(3) << "Starting Accept Socket.";
  server_socket_ = socket;
  create_callback_.Run(GetLocator());
  server_socket_->Accept(
      base::Bind(&CopresenceEndpoint::OnAccept, weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&CopresenceEndpoint::OnAcceptError,
                 weak_ptr_factory_.GetWeakPtr()));
}

void CopresenceEndpoint::OnCreateServiceError(const std::string& message) {
  LOG(WARNING) << "Couldn't create Bluetooth service: " << message;
  create_callback_.Run(std::string());
}

void CopresenceEndpoint::OnAccept(
    const device::BluetoothDevice* device,
    scoped_refptr<device::BluetoothSocket> socket) {
  if (!socket.get())
    return;
  VLOG(3) << "Accepted Socket.";
  client_socket_.reset(new CopresenceSocketBluetooth(socket));
  accept_callback_.Run();
  client_socket_->Receive(receive_callback_);
}

void CopresenceEndpoint::OnAcceptError(const std::string& message) {
  LOG(WARNING) << "Couldn't accept Bluetooth connection: " << message;
}

}  // namespace copresence_endpoints
