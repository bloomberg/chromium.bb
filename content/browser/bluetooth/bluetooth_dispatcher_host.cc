// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/bluetooth/bluetooth_dispatcher_host.h"

#include "content/common/bluetooth/bluetooth_messages.h"

namespace content {

BluetoothDispatcherHost::BluetoothDispatcherHost()
    : BrowserMessageFilter(BluetoothMsgStart),
      bluetooth_mock_data_set_(MockData::NOT_MOCKING),
      bluetooth_request_device_reject_type_(BluetoothError::NOT_FOUND) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
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

BluetoothDispatcherHost::~BluetoothDispatcherHost() {
}

void BluetoothDispatcherHost::OnRequestDevice(int thread_id, int request_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // Mock implementation util a more complete implementation is built out.
  switch (bluetooth_mock_data_set_) {
    case MockData::NOT_MOCKING: {
      Send(new BluetoothMsg_RequestDeviceError(thread_id, request_id,
                                               BluetoothError::NOT_FOUND));
      return;
    }
    case MockData::REJECT: {
      Send(new BluetoothMsg_RequestDeviceError(
          thread_id, request_id, bluetooth_request_device_reject_type_));
      return;
    }
    case MockData::RESOLVE: {
      Send(new BluetoothMsg_RequestDeviceSuccess(thread_id, request_id,
                                                 "Empty Mock deviceId"));
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
