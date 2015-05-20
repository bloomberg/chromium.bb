// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/bluetooth/bluetooth_dispatcher_host.h"

#include "base/strings/utf_string_conversions.h"
#include "content/common/bluetooth/bluetooth_messages.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_discovery_session.h"

using device::BluetoothAdapter;
using device::BluetoothAdapterFactory;

namespace content {

const uint32 kUnspecifiedDeviceClass =
    0x1F00;  // bluetooth.org/en-us/specification/assigned-numbers/baseband
const int kScanTime = 5;  // 5 seconds of scan time

BluetoothDispatcherHost::BluetoothDispatcherHost()
    : BrowserMessageFilter(BluetoothMsgStart),
      bluetooth_mock_data_set_(MockData::NOT_MOCKING),
      bluetooth_request_device_reject_type_(BluetoothError::NOT_FOUND),
      weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (BluetoothAdapterFactory::IsBluetoothAdapterAvailable())
    BluetoothAdapterFactory::GetAdapter(
        base::Bind(&BluetoothDispatcherHost::set_adapter,
                   weak_ptr_factory_.GetWeakPtr()));
}

void BluetoothDispatcherHost::OnDestruct() const {
  // See class comment: UI Thread Note.
  BrowserThread::DeleteOnUIThread::Destruct(this);
}

void BluetoothDispatcherHost::OverrideThreadForMessage(
    const IPC::Message& message,
    content::BrowserThread::ID* thread) {
  // See class comment: UI Thread Note.
  *thread = BrowserThread::UI;
}

bool BluetoothDispatcherHost::OnMessageReceived(const IPC::Message& message) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(BluetoothDispatcherHost, message)
  IPC_MESSAGE_HANDLER(BluetoothHostMsg_RequestDevice, OnRequestDevice)
  IPC_MESSAGE_HANDLER(BluetoothHostMsg_ConnectGATT, OnConnectGATT)
  IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void BluetoothDispatcherHost::SetBluetoothAdapterForTesting(
    const std::string& name) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (name == "RejectRequestDevice_NotFoundError") {
    bluetooth_mock_data_set_ = MockData::REJECT;
    bluetooth_request_device_reject_type_ = BluetoothError::NOT_FOUND;
  } else if (name == "RejectRequestDevice_SecurityError") {
    bluetooth_mock_data_set_ = MockData::REJECT;
    bluetooth_request_device_reject_type_ = BluetoothError::SECURITY;
  } else if (name == "ResolveRequestDevice_Empty" ||  // TODO(scheib): Remove.
             name == "Single Empty Device") {
    bluetooth_mock_data_set_ = MockData::RESOLVE;
  } else {
    bluetooth_mock_data_set_ = MockData::NOT_MOCKING;
  }
}

BluetoothDispatcherHost::~BluetoothDispatcherHost() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Clear adapter, releasing observer references.
  set_adapter(scoped_refptr<device::BluetoothAdapter>());
}

void BluetoothDispatcherHost::set_adapter(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (adapter_.get())
    adapter_->RemoveObserver(this);
  adapter_ = adapter;
  if (adapter_.get())
    adapter_->AddObserver(this);
}

void BluetoothDispatcherHost::OnRequestDevice(int thread_id, int request_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // TODO(scheib) Extend this very simple mock implementation by using
  // device/bluetooth/test mock adapter and related classes.
  switch (bluetooth_mock_data_set_) {
    case MockData::NOT_MOCKING: {
      // TODO(scheib): Filter devices by services: crbug.com/440594
      // TODO(scheib): Device selection UI: crbug.com/436280
      // TODO(scheib): Utilize BluetoothAdapter::Observer::DeviceAdded/Removed.
      BluetoothAdapter::DeviceList devices;

      if (adapter_.get()) {
        adapter_->StartDiscoverySession(
            base::Bind(&BluetoothDispatcherHost::OnDiscoverySessionStarted,
                       weak_ptr_factory_.GetWeakPtr(), thread_id, request_id),
            base::Bind(&BluetoothDispatcherHost::OnDiscoverySessionStartedError,
                       weak_ptr_factory_.GetWeakPtr(), thread_id, request_id));
      } else {
        DLOG(WARNING) << "No BluetoothAdapter. Can't serve requestDevice.";
        Send(new BluetoothMsg_RequestDeviceError(thread_id, request_id,
                                                 BluetoothError::NOT_FOUND));
      }
      return;
    }
    case MockData::REJECT: {
      Send(new BluetoothMsg_RequestDeviceError(
          thread_id, request_id, bluetooth_request_device_reject_type_));
      return;
    }
    case MockData::RESOLVE: {
      std::vector<std::string> uuids;
      uuids.push_back("00001800-0000-1000-8000-00805f9b34fb");
      uuids.push_back("00001801-0000-1000-8000-00805f9b34fb");
      content::BluetoothDevice device_ipc(
          "Empty Mock Device instanceID",                // instance_id
          base::UTF8ToUTF16("Empty Mock Device name"),   // name
          kUnspecifiedDeviceClass,                       // device_class
          device::BluetoothDevice::VENDOR_ID_BLUETOOTH,  // vendor_id_source
          0xFFFF,                                        // vendor_id
          1,                                             // product_id
          2,                                             // product_version
          true,                                          // paired
          uuids);                                        // uuids
      Send(new BluetoothMsg_RequestDeviceSuccess(thread_id, request_id,
                                                 device_ipc));
      return;
    }
  }
  NOTREACHED();
}

void BluetoothDispatcherHost::OnConnectGATT(
    int thread_id,
    int request_id,
    const std::string& device_instance_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // TODO(ortuno): Add actual implementation of connectGATT. This needs to be
  // done after the "allowed devices map" is implemented.
  Send(new BluetoothMsg_ConnectGATTSuccess(thread_id, request_id,
                                           device_instance_id));
}

void BluetoothDispatcherHost::OnDiscoverySessionStarted(
    int thread_id,
    int request_id,
    scoped_ptr<device::BluetoothDiscoverySession> discovery_session) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserThread::PostDelayedTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&BluetoothDispatcherHost::StopDiscoverySession,
                 weak_ptr_factory_.GetWeakPtr(), thread_id, request_id,
                 base::Passed(&discovery_session)),
      base::TimeDelta::FromSeconds(kScanTime));
}

void BluetoothDispatcherHost::OnDiscoverySessionStartedError(int thread_id,
                                                             int request_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DLOG(WARNING) << "BluetoothDispatcherHost::OnDiscoverySessionStartedError";
  Send(new BluetoothMsg_RequestDeviceError(thread_id, request_id,
                                           BluetoothError::NOT_FOUND));
}

void BluetoothDispatcherHost::StopDiscoverySession(
    int thread_id,
    int request_id,
    scoped_ptr<device::BluetoothDiscoverySession> discovery_session) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  discovery_session->Stop(
      base::Bind(&BluetoothDispatcherHost::OnDiscoverySessionStopped,
                 weak_ptr_factory_.GetWeakPtr(), thread_id, request_id),
      base::Bind(&BluetoothDispatcherHost::OnDiscoverySessionStoppedError,
                 weak_ptr_factory_.GetWeakPtr(), thread_id, request_id));
}

void BluetoothDispatcherHost::OnDiscoverySessionStopped(int thread_id,
                                                        int request_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BluetoothAdapter::DeviceList devices = adapter_->GetDevices();
  if (devices.begin() == devices.end()) {
    Send(new BluetoothMsg_RequestDeviceError(thread_id, request_id,
                                             BluetoothError::NOT_FOUND));
  } else {
    device::BluetoothDevice* device = *devices.begin();
    content::BluetoothDevice device_ipc(
        device->GetAddress(),         // instance_id
        device->GetName(),            // name
        device->GetBluetoothClass(),  // device_class
        device->GetVendorIDSource(),  // vendor_id_source
        device->GetVendorID(),        // vendor_id
        device->GetProductID(),       // product_id
        device->GetDeviceID(),        // product_version
        device->IsPaired(),           // paired
        content::BluetoothDevice::UUIDsFromBluetoothUUIDs(
            device->GetUUIDs()));  // uuids
    Send(new BluetoothMsg_RequestDeviceSuccess(thread_id, request_id,
                                               device_ipc));
  }
}

void BluetoothDispatcherHost::OnDiscoverySessionStoppedError(int thread_id,
                                                             int request_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DLOG(WARNING) << "BluetoothDispatcherHost::OnDiscoverySessionStoppedError";
  Send(new BluetoothMsg_RequestDeviceError(thread_id, request_id,
                                           BluetoothError::NOT_FOUND));
}

}  // namespace content
