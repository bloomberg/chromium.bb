// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/bluetooth/web_bluetooth_impl.h"

namespace content {

WebBluetoothImpl::WebBluetoothImpl()
    : m_bluetoothMockDataSet(MockData::NOT_MOCKING),
      m_bluetoothRequestDeviceRejectType(
          blink::WebBluetoothError::NotFoundError) {
}

void WebBluetoothImpl::requestDevice(
    blink::WebBluetoothRequestDeviceCallbacks* callbacks) {
  // Mock implementation of blink::WebBluetooth until a more complete
  // implementation is built out.
  switch (m_bluetoothMockDataSet) {
    case MockData::NOT_MOCKING: {
      blink::WebBluetoothError* error = new blink::WebBluetoothError(
          blink::WebBluetoothError::NotFoundError, "");
      callbacks->onError(error);
      break;
    }
    case MockData::REJECT: {
      blink::WebBluetoothError* error =
          new blink::WebBluetoothError(m_bluetoothRequestDeviceRejectType, "");
      callbacks->onError(error);
      break;
    }
    case MockData::RESOLVE: {
      callbacks->onSuccess();
      break;
    }
  }
}

void WebBluetoothImpl::SetBluetoothMockDataSetForTesting(
    const std::string& name) {
  if (name == "RejectRequestDevice_NotFoundError") {
    m_bluetoothMockDataSet = MockData::REJECT;
    m_bluetoothRequestDeviceRejectType =
        blink::WebBluetoothError::NotFoundError;
  } else if (name == "RejectRequestDevice_SecurityError") {
    m_bluetoothMockDataSet = MockData::REJECT;
    m_bluetoothRequestDeviceRejectType =
        blink::WebBluetoothError::SecurityError;
  } else if (name == "ResolveRequestDevice_Empty") {
    m_bluetoothMockDataSet = MockData::RESOLVE;
  } else {
    m_bluetoothMockDataSet = MockData::NOT_MOCKING;
  }
}

}  // namespace content
