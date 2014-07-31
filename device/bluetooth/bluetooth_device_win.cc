// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_device_win.h"

#include <string>

#include "base/basictypes.h"
#include "base/containers/scoped_ptr_hash_map.h"
#include "base/logging.h"
#include "base/memory/scoped_vector.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/stringprintf.h"
#include "device/bluetooth/bluetooth_service_record_win.h"
#include "device/bluetooth/bluetooth_socket_thread.h"
#include "device/bluetooth/bluetooth_socket_win.h"
#include "device/bluetooth/bluetooth_task_manager_win.h"
#include "device/bluetooth/bluetooth_uuid.h"

namespace {

const int kSdpBytesBufferSize = 1024;

}  // namespace

namespace device {

BluetoothDeviceWin::BluetoothDeviceWin(
    const BluetoothTaskManagerWin::DeviceState& device_state,
    const scoped_refptr<base::SequencedTaskRunner>& ui_task_runner,
    const scoped_refptr<BluetoothSocketThread>& socket_thread,
    net::NetLog* net_log,
    const net::NetLog::Source& net_log_source)
    : BluetoothDevice(),
      ui_task_runner_(ui_task_runner),
      socket_thread_(socket_thread),
      net_log_(net_log),
      net_log_source_(net_log_source) {
  Update(device_state);
}

BluetoothDeviceWin::~BluetoothDeviceWin() {
}

void BluetoothDeviceWin::Update(
    const BluetoothTaskManagerWin::DeviceState& device_state) {
  address_ = device_state.address;
  // Note: Callers are responsible for providing a canonicalized address.
  DCHECK_EQ(address_, BluetoothDevice::CanonicalizeAddress(address_));
  name_ = device_state.name;
  bluetooth_class_ = device_state.bluetooth_class;
  visible_ = device_state.visible;
  connected_ = device_state.connected;
  paired_ = device_state.authenticated;
  UpdateServices(device_state);
}

void BluetoothDeviceWin::UpdateServices(
    const BluetoothTaskManagerWin::DeviceState& device_state) {
  uuids_.clear();
  service_record_list_.clear();

  for (ScopedVector<BluetoothTaskManagerWin::ServiceRecordState>::const_iterator
           iter = device_state.service_record_states.begin();
       iter != device_state.service_record_states.end();
       ++iter) {
    BluetoothServiceRecordWin* service_record =
        new BluetoothServiceRecordWin(device_state.address,
                                      (*iter)->name,
                                      (*iter)->sdp_bytes,
                                      (*iter)->gatt_uuid);
    service_record_list_.push_back(service_record);
    uuids_.push_back(service_record->uuid());
  }
}

bool BluetoothDeviceWin::IsEqual(
    const BluetoothTaskManagerWin::DeviceState& device_state) {
  if (address_ != device_state.address || name_ != device_state.name ||
      bluetooth_class_ != device_state.bluetooth_class ||
      visible_ != device_state.visible ||
      connected_ != device_state.connected ||
      paired_ != device_state.authenticated) {
    return false;
  }

  // Checks service collection
  typedef std::set<BluetoothUUID> UUIDSet;
  typedef base::ScopedPtrHashMap<std::string, BluetoothServiceRecordWin>
      ServiceRecordMap;

  UUIDSet known_services;
  for (UUIDList::const_iterator iter = uuids_.begin(); iter != uuids_.end();
       ++iter) {
    known_services.insert((*iter));
  }

  UUIDSet new_services;
  ServiceRecordMap new_service_records;
  for (ScopedVector<BluetoothTaskManagerWin::ServiceRecordState>::const_iterator
           iter = device_state.service_record_states.begin();
       iter != device_state.service_record_states.end();
       ++iter) {
    BluetoothServiceRecordWin* service_record = new BluetoothServiceRecordWin(
        address_, (*iter)->name, (*iter)->sdp_bytes, (*iter)->gatt_uuid);
    new_services.insert(service_record->uuid());
    new_service_records.set(
        service_record->uuid().canonical_value(),
        scoped_ptr<BluetoothServiceRecordWin>(service_record));
  }

  UUIDSet removed_services =
      base::STLSetDifference<UUIDSet>(known_services, new_services);
  if (!removed_services.empty()) {
    return false;
  }
  UUIDSet added_devices =
      base::STLSetDifference<UUIDSet>(new_services, known_services);
  if (!added_devices.empty()) {
    return false;
  }

  for (ServiceRecordList::const_iterator iter = service_record_list_.begin();
       iter != service_record_list_.end();
       ++iter) {
    BluetoothServiceRecordWin* service_record = (*iter);
    BluetoothServiceRecordWin* new_service_record =
        new_service_records.get((*iter)->uuid().canonical_value());
    if (!service_record->IsEqual(*new_service_record))
      return false;
  }
  return true;
}

void BluetoothDeviceWin::SetVisible(bool visible) {
  visible_ = visible;
}

uint32 BluetoothDeviceWin::GetBluetoothClass() const {
  return bluetooth_class_;
}

std::string BluetoothDeviceWin::GetDeviceName() const {
  return name_;
}

std::string BluetoothDeviceWin::GetAddress() const {
  return address_;
}

BluetoothDevice::VendorIDSource
BluetoothDeviceWin::GetVendorIDSource() const {
  return VENDOR_ID_UNKNOWN;
}

uint16 BluetoothDeviceWin::GetVendorID() const {
  return 0;
}

uint16 BluetoothDeviceWin::GetProductID() const {
  return 0;
}

uint16 BluetoothDeviceWin::GetDeviceID() const {
  return 0;
}

int BluetoothDeviceWin::GetRSSI() const {
  NOTIMPLEMENTED();
  return kUnknownPower;
}

int BluetoothDeviceWin::GetCurrentHostTransmitPower() const {
  NOTIMPLEMENTED();
  return kUnknownPower;
}

int BluetoothDeviceWin::GetMaximumHostTransmitPower() const {
  NOTIMPLEMENTED();
  return kUnknownPower;
}

bool BluetoothDeviceWin::IsPaired() const {
  return paired_;
}

bool BluetoothDeviceWin::IsConnected() const {
  return connected_;
}

bool BluetoothDeviceWin::IsConnectable() const {
  return false;
}

bool BluetoothDeviceWin::IsConnecting() const {
  return false;
}

BluetoothDevice::UUIDList BluetoothDeviceWin::GetUUIDs() const {
  return uuids_;
}

bool BluetoothDeviceWin::ExpectingPinCode() const {
  NOTIMPLEMENTED();
  return false;
}

bool BluetoothDeviceWin::ExpectingPasskey() const {
  NOTIMPLEMENTED();
  return false;
}

bool BluetoothDeviceWin::ExpectingConfirmation() const {
  NOTIMPLEMENTED();
  return false;
}

void BluetoothDeviceWin::Connect(
    PairingDelegate* pairing_delegate,
    const base::Closure& callback,
    const ConnectErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothDeviceWin::SetPinCode(const std::string& pincode) {
  NOTIMPLEMENTED();
}

void BluetoothDeviceWin::SetPasskey(uint32 passkey) {
  NOTIMPLEMENTED();
}

void BluetoothDeviceWin::ConfirmPairing() {
  NOTIMPLEMENTED();
}

void BluetoothDeviceWin::RejectPairing() {
  NOTIMPLEMENTED();
}

void BluetoothDeviceWin::CancelPairing() {
  NOTIMPLEMENTED();
}

void BluetoothDeviceWin::Disconnect(
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothDeviceWin::Forget(const ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothDeviceWin::ConnectToService(
    const BluetoothUUID& uuid,
    const ConnectToServiceCallback& callback,
    const ConnectToServiceErrorCallback& error_callback) {
  scoped_refptr<BluetoothSocketWin> socket(
      BluetoothSocketWin::CreateBluetoothSocket(
          ui_task_runner_, socket_thread_));
  socket->Connect(this, uuid, base::Bind(callback, socket), error_callback);
}

void BluetoothDeviceWin::CreateGattConnection(
      const GattConnectionCallback& callback,
      const ConnectErrorCallback& error_callback) {
  // TODO(armansito): Implement.
  error_callback.Run(ERROR_UNSUPPORTED_DEVICE);
}

void BluetoothDeviceWin::StartConnectionMonitor(
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

const BluetoothServiceRecordWin* BluetoothDeviceWin::GetServiceRecord(
    const device::BluetoothUUID& uuid) const {
  for (ServiceRecordList::const_iterator iter = service_record_list_.begin();
       iter != service_record_list_.end();
       ++iter) {
    if ((*iter)->uuid() == uuid)
      return *iter;
  }
  return NULL;
}

}  // namespace device
