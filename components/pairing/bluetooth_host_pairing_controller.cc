// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/pairing/bluetooth_host_pairing_controller.h"

#include "base/bind.h"
#include "base/hash.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "components/pairing/bluetooth_pairing_constants.h"
#include "components/pairing/pairing_api.pb.h"
#include "components/pairing/proto_decoder.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "net/base/io_buffer.h"

namespace {
const int kReceiveSize = 16384;
}

namespace pairing_chromeos {

BluetoothHostPairingController::BluetoothHostPairingController()
    : current_stage_(STAGE_NONE),
      device_(NULL),
      proto_decoder_(new ProtoDecoder(this)),
      ptr_factory_(this) {
}

BluetoothHostPairingController::~BluetoothHostPairingController() {
  if (adapter_.get())
    adapter_->RemoveObserver(this);
}

void BluetoothHostPairingController::ChangeStage(Stage new_stage) {
  if (current_stage_ == new_stage)
    return;
  current_stage_ = new_stage;
  FOR_EACH_OBSERVER(Observer, observers_, PairingStageChanged(new_stage));
}

void BluetoothHostPairingController::SendHostStatus() {
  pairing_api::HostStatus host_status;

  host_status.set_api_version(kPairingAPIVersion);
  if (!enrollment_domain_.empty())
    host_status.mutable_parameters()->set_domain(enrollment_domain_);

  // TODO(zork): Get these values from the UI. (http://crbug.com/405744)
  host_status.mutable_parameters()->set_connectivity(
      pairing_api::HostStatusParameters::CONNECTIVITY_CONNECTED);
  host_status.mutable_parameters()->set_update_status(
      pairing_api::HostStatusParameters::UPDATE_STATUS_UPDATED);

  // TODO(zork): Get a list of other paired controllers.
  // (http://crbug.com/405757)

  int size = 0;
  scoped_refptr<net::IOBuffer> io_buffer(
      ProtoDecoder::SendHostStatus(host_status, &size));

  controller_socket_->Send(
      io_buffer, size,
      base::Bind(&BluetoothHostPairingController::OnSendComplete,
                 ptr_factory_.GetWeakPtr()),
      base::Bind(&BluetoothHostPairingController::OnSendError,
                 ptr_factory_.GetWeakPtr()));
}

void BluetoothHostPairingController::AbortWithError(
    int code,
    const std::string& message) {
  if (controller_socket_.get()) {
    pairing_api::Error error;

    error.set_api_version(kPairingAPIVersion);
    error.mutable_parameters()->set_code(PAIRING_ERROR_PAIRING_OR_ENROLLMENT);
    error.mutable_parameters()->set_description(message);

    int size = 0;
    scoped_refptr<net::IOBuffer> io_buffer(
        ProtoDecoder::SendError(error, &size));

    controller_socket_->Send(
        io_buffer, size,
        base::Bind(&BluetoothHostPairingController::OnSendComplete,
                   ptr_factory_.GetWeakPtr()),
        base::Bind(&BluetoothHostPairingController::OnSendError,
                   ptr_factory_.GetWeakPtr()));
  }
  Reset();
}

void BluetoothHostPairingController::Reset() {
  if (controller_socket_.get()) {
    controller_socket_->Close();
    controller_socket_ = NULL;
  }

  if (service_socket_.get()) {
    service_socket_->Close();
    service_socket_ = NULL;
  }
  ChangeStage(STAGE_NONE);
}

void BluetoothHostPairingController::OnGetAdapter(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!adapter_.get());
  adapter_ = adapter;

  if (adapter_->IsPresent()) {
    SetName();
  } else {
    // Set the name once the adapter is present.
    adapter_->AddObserver(this);
  }
}

void BluetoothHostPairingController::SetName() {
  // Hash the bluetooth address and take the lower 2 bytes to create a human
  // readable device name.
  const uint32 device_id = base::Hash(adapter_->GetAddress()) & 0xFFFF;
  device_name_ = base::StringPrintf("%s%04X", kDeviceNamePrefix, device_id);

  adapter_->SetName(
      device_name_,
      base::Bind(&BluetoothHostPairingController::OnSetName,
                 ptr_factory_.GetWeakPtr()),
      base::Bind(&BluetoothHostPairingController::OnSetError,
                 ptr_factory_.GetWeakPtr()));
}

void BluetoothHostPairingController::OnSetName() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (adapter_->IsPowered()) {
    OnSetPowered();
  } else {
    adapter_->SetPowered(
        true,
        base::Bind(&BluetoothHostPairingController::OnSetPowered,
                   ptr_factory_.GetWeakPtr()),
        base::Bind(&BluetoothHostPairingController::OnSetError,
                   ptr_factory_.GetWeakPtr()));
  }
}

void BluetoothHostPairingController::OnSetPowered() {
  DCHECK(thread_checker_.CalledOnValidThread());
  adapter_->AddPairingDelegate(
      this, device::BluetoothAdapter::PAIRING_DELEGATE_PRIORITY_HIGH);

  device::BluetoothAdapter::ServiceOptions options;
  options.name.reset(new std::string(kPairingServiceName));

  adapter_->CreateRfcommService(
      device::BluetoothUUID(kPairingServiceUUID), options,
      base::Bind(&BluetoothHostPairingController::OnCreateService,
                 ptr_factory_.GetWeakPtr()),
      base::Bind(&BluetoothHostPairingController::OnCreateServiceError,
                 ptr_factory_.GetWeakPtr()));
}

void BluetoothHostPairingController::OnCreateService(
    scoped_refptr<device::BluetoothSocket> socket) {
  DCHECK(thread_checker_.CalledOnValidThread());
  service_socket_ = socket;

  service_socket_->Accept(
      base::Bind(&BluetoothHostPairingController::OnAccept,
                 ptr_factory_.GetWeakPtr()),
      base::Bind(&BluetoothHostPairingController::OnAcceptError,
                 ptr_factory_.GetWeakPtr()));

  adapter_->SetDiscoverable(
      true,
      base::Bind(&BluetoothHostPairingController::OnSetDiscoverable,
                 ptr_factory_.GetWeakPtr(), true),
      base::Bind(&BluetoothHostPairingController::OnSetError,
                 ptr_factory_.GetWeakPtr()));
}

void BluetoothHostPairingController::OnAccept(
    const device::BluetoothDevice* device,
    scoped_refptr<device::BluetoothSocket> socket) {
  DCHECK(thread_checker_.CalledOnValidThread());
  adapter_->SetDiscoverable(
      false,
      base::Bind(&BluetoothHostPairingController::OnSetDiscoverable,
                 ptr_factory_.GetWeakPtr(), false),
      base::Bind(&BluetoothHostPairingController::OnSetError,
                 ptr_factory_.GetWeakPtr()));

  controller_socket_ = socket;
  service_socket_ = NULL;

  // TODO: Update Host. (http://crbug.com/405754)
  SendHostStatus();

  controller_socket_->Receive(
      kReceiveSize,
      base::Bind(&BluetoothHostPairingController::OnReceiveComplete,
                 ptr_factory_.GetWeakPtr()),
      base::Bind(&BluetoothHostPairingController::OnReceiveError,
                 ptr_factory_.GetWeakPtr()));

  ChangeStage(STAGE_WAITING_FOR_CREDENTIALS);
}

void BluetoothHostPairingController::OnSetDiscoverable(bool change_stage) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (change_stage) {
    DCHECK_EQ(current_stage_, STAGE_NONE);
    ChangeStage(STAGE_WAITING_FOR_CONTROLLER);
  }
}

void BluetoothHostPairingController::OnSendComplete(int bytes_sent) {}

void BluetoothHostPairingController::OnReceiveComplete(
    int bytes, scoped_refptr<net::IOBuffer> io_buffer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  proto_decoder_->DecodeIOBuffer(bytes, io_buffer);

  controller_socket_->Receive(
      kReceiveSize,
      base::Bind(&BluetoothHostPairingController::OnReceiveComplete,
                 ptr_factory_.GetWeakPtr()),
      base::Bind(&BluetoothHostPairingController::OnReceiveError,
                 ptr_factory_.GetWeakPtr()));
}

void BluetoothHostPairingController::OnCreateServiceError(
    const std::string& message) {
  LOG(ERROR) << message;
  ChangeStage(STAGE_INITIALIZATION_ERROR);
}

void BluetoothHostPairingController::OnSetError() {
  adapter_->RemovePairingDelegate(this);
  ChangeStage(STAGE_INITIALIZATION_ERROR);
}

void BluetoothHostPairingController::OnAcceptError(
    const std::string& error_message) {
  LOG(ERROR) << error_message;
  Reset();
}

void BluetoothHostPairingController::OnSendError(
    const std::string& error_message) {
  LOG(ERROR) << error_message;
}

void BluetoothHostPairingController::OnReceiveError(
    device::BluetoothSocket::ErrorReason reason,
    const std::string& error_message) {
  LOG(ERROR) << reason << ", " << error_message;
  Reset();
}

void BluetoothHostPairingController::OnHostStatusMessage(
    const pairing_api::HostStatus& message) {
  NOTREACHED();
}

void BluetoothHostPairingController::OnConfigureHostMessage(
    const pairing_api::ConfigureHost& message) {
  FOR_EACH_OBSERVER(Observer, observers_,
                    ConfigureHost(message.parameters().accepted_eula(),
                                  message.parameters().lang(),
                                  message.parameters().timezone(),
                                  message.parameters().send_reports(),
                                  message.parameters().keyboard_layout()));
}

void BluetoothHostPairingController::OnPairDevicesMessage(
    const pairing_api::PairDevices& message) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (current_stage_ != STAGE_WAITING_FOR_CREDENTIALS) {
    AbortWithError(PAIRING_ERROR_PAIRING_OR_ENROLLMENT, kErrorInvalidProtocol);
    return;
  }

  ChangeStage(STAGE_ENROLLING);
  FOR_EACH_OBSERVER(Observer, observers_,
                    EnrollHost(message.parameters().admin_access_token()));
}

void BluetoothHostPairingController::SetEnrollmentComplete(bool success) {
  DCHECK_EQ(current_stage_, STAGE_ENROLLING);
  DCHECK(thread_checker_.CalledOnValidThread());
  if (success) {
    ChangeStage(STAGE_PAIRING_DONE);
    SendHostStatus();
  } else {
    AbortWithError(PAIRING_ERROR_PAIRING_OR_ENROLLMENT, kErrorEnrollmentFailed);
  }
}

void BluetoothHostPairingController::OnCompleteSetupMessage(
    const pairing_api::CompleteSetup& message) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (current_stage_ != STAGE_PAIRING_DONE) {
    AbortWithError(PAIRING_ERROR_PAIRING_OR_ENROLLMENT, kErrorInvalidProtocol);
    return;
  }

  // TODO(zork): Handle adding another controller. (http://crbug.com/405757)
  ChangeStage(STAGE_FINISHED);
}

void BluetoothHostPairingController::OnErrorMessage(
    const pairing_api::Error& message) {
  NOTREACHED();
}

void BluetoothHostPairingController::AdapterPresentChanged(
    device::BluetoothAdapter* adapter,
    bool present) {
  DCHECK_EQ(adapter, adapter_.get());
  if (present) {
    adapter_->RemoveObserver(this);
    SetName();
  }
}

void BluetoothHostPairingController::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void BluetoothHostPairingController::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

HostPairingController::Stage BluetoothHostPairingController::GetCurrentStage() {
  return current_stage_;
}

void BluetoothHostPairingController::StartPairing() {
  DCHECK_EQ(current_stage_, STAGE_NONE);
  bool bluetooth_available =
      device::BluetoothAdapterFactory::IsBluetoothAdapterAvailable();
  if (!bluetooth_available) {
    ChangeStage(STAGE_INITIALIZATION_ERROR);
    return;
  }

  device::BluetoothAdapterFactory::GetAdapter(
      base::Bind(&BluetoothHostPairingController::OnGetAdapter,
                 ptr_factory_.GetWeakPtr()));
}

std::string BluetoothHostPairingController::GetDeviceName() {
  return device_name_;
}

std::string BluetoothHostPairingController::GetConfirmationCode() {
  DCHECK_EQ(current_stage_, STAGE_WAITING_FOR_CODE_CONFIRMATION);
  return confirmation_code_;
}

std::string BluetoothHostPairingController::GetEnrollmentDomain() {
  return enrollment_domain_;
}

void BluetoothHostPairingController::OnUpdateStatusChanged(
    UpdateStatus update_status) {
  // TODO(zork): Handling updating stages (http://crbug.com/405754).
}

void BluetoothHostPairingController::RequestPinCode(
    device::BluetoothDevice* device) {
  // Disallow unknown device.
  device->RejectPairing();
}

void BluetoothHostPairingController::RequestPasskey(
    device::BluetoothDevice* device) {
  // Disallow unknown device.
  device->RejectPairing();
}

void BluetoothHostPairingController::DisplayPinCode(
    device::BluetoothDevice* device,
    const std::string& pincode) {
  // Disallow unknown device.
  device->RejectPairing();
}

void BluetoothHostPairingController::DisplayPasskey(
    device::BluetoothDevice* device,
    uint32 passkey) {
  // Disallow unknown device.
  device->RejectPairing();
}

void BluetoothHostPairingController::KeysEntered(
    device::BluetoothDevice* device,
    uint32 entered) {
  // Disallow unknown device.
  device->RejectPairing();
}

void BluetoothHostPairingController::ConfirmPasskey(
    device::BluetoothDevice* device,
    uint32 passkey) {
  confirmation_code_ = base::StringPrintf("%06d", passkey);
  device->ConfirmPairing();
  ChangeStage(STAGE_WAITING_FOR_CODE_CONFIRMATION);
}

void BluetoothHostPairingController::AuthorizePairing(
    device::BluetoothDevice* device) {
  // Disallow unknown device.
  device->RejectPairing();
}

}  // namespace pairing_chromeos
