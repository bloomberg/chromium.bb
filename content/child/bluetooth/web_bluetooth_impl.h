// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_BLUETOOTH_WEB_BLUETOOTH_IMPL_H_
#define CONTENT_CHILD_BLUETOOTH_WEB_BLUETOOTH_IMPL_H_

#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/WebBluetooth.h"
#include "third_party/WebKit/public/platform/WebBluetoothError.h"

namespace content {

// Mock implementation of blink::WebBluetooth until a more complete
// implementation is built out.
class CONTENT_EXPORT WebBluetoothImpl
    : NON_EXPORTED_BASE(public blink::WebBluetooth) {
 public:
  WebBluetoothImpl();

  // WebBluetooth interface:
  void requestDevice(blink::WebBluetoothRequestDeviceCallbacks*) override;

  // Testing interface:
  void SetBluetoothMockDataSetForTesting(const std::string& name);

 private:
  enum class MockData { NOT_MOCKING, REJECT, RESOLVE };
  MockData m_bluetoothMockDataSet;
  blink::WebBluetoothError::ErrorType m_bluetoothRequestDeviceRejectType;
};

}  // namespace content

#endif  // CONTENT_CHILD_BLUETOOTH_WEB_BLUETOOTH_IMPL_H_
