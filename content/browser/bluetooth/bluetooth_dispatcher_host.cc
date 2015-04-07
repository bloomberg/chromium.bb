// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/bluetooth/bluetooth_dispatcher_host.h"

#include "base/strings/utf_string_conversions.h"
#include "content/common/bluetooth/bluetooth_messages.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_device.h"

using device::BluetoothAdapter;
using device::BluetoothAdapterFactory;

namespace content {

const uint32 kUnspecifiedDeviceClass =
    0x1F00;  // bluetooth.org/en-us/specification/assigned-numbers/baseband

scoped_refptr<BluetoothDispatcherHost> BluetoothDispatcherHost::Create() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Hold a reference to the BluetoothDispatcherHost because the callback below
  // may run and would otherwise release the BluetoothDispatcherHost
  // prematurely.
  scoped_refptr<BluetoothDispatcherHost> host(new BluetoothDispatcherHost());
  if (BluetoothAdapterFactory::IsBluetoothAdapterAvailable())
    BluetoothAdapterFactory::GetAdapter(
        base::Bind(&BluetoothDispatcherHost::set_adapter, host));
  return host;
}

bool BluetoothDispatcherHost::OnMessageReceived(const IPC::Message& message) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(BluetoothDispatcherHost, message)
  IPC_MESSAGE_HANDLER(BluetoothHostMsg_RequestDevice, OnRequestDevice)
  IPC_MESSAGE_HANDLER(BluetoothHostMsg_SetBluetoothMockDataSetForTesting,
                      OnSetBluetoothMockDataSetForTesting)
  IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

BluetoothDispatcherHost::BluetoothDispatcherHost()
    : BrowserMessageFilter(BluetoothMsgStart),
      bluetooth_mock_data_set_(MockData::NOT_MOCKING),
      bluetooth_request_device_reject_type_(BluetoothError::NOT_FOUND) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

BluetoothDispatcherHost::~BluetoothDispatcherHost() {
  // Clear adapter, releasing observer references.
  set_adapter(scoped_refptr<device::BluetoothAdapter>());
}

void BluetoothDispatcherHost::set_adapter(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  if (adapter_.get())
    adapter_->RemoveObserver(this);
  adapter_ = adapter;
  if (adapter_.get())
    adapter_->AddObserver(this);
}

void BluetoothDispatcherHost::OnRequestDevice(int thread_id, int request_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // TODO(scheib) Extend this very simple mock implementation by using
  // device/bluetooth/test mock adapter and related classes.
  switch (bluetooth_mock_data_set_) {
    case MockData::NOT_MOCKING: {
      // TODO(scheib): Filter devices by services: crbug.com/440594
      // TODO(scheib): Device selection UI: crbug.com/436280
      // TODO(scheib): Utilize BluetoothAdapter::Observer::DeviceAdded/Removed.
      BluetoothAdapter::DeviceList devices;
      if (adapter_.get())
        devices = adapter_->GetDevices();
      else
        DLOG(WARNING) << "No BluetoothAdapter. Can't serve requestDevice.";

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
            device->IsConnected(),        // connected
            content::BluetoothDevice::UUIDsFromBluetoothUUIDs(
                device->GetUUIDs()));  // uuids
        Send(new BluetoothMsg_RequestDeviceSuccess(thread_id, request_id,
                                                   device_ipc));
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
          false,                                         // connected
          uuids);                                        // uuids
      Send(new BluetoothMsg_RequestDeviceSuccess(thread_id, request_id,
                                                 device_ipc));
      return;
    }
  }
  NOTREACHED();
}

void BluetoothDispatcherHost::OnSetBluetoothMockDataSetForTesting(
    const std::string& name) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
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

}  // namespace content
