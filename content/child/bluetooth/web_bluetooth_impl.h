// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_BLUETOOTH_WEB_BLUETOOTH_IMPL_H_
#define CONTENT_CHILD_BLUETOOTH_WEB_BLUETOOTH_IMPL_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/modules/bluetooth/WebBluetooth.h"

namespace content {

class BluetoothDispatcher;
class ThreadSafeSender;

// Implementation of blink::WebBluetooth. Passes calls through to the thread
// specific BluetoothDispatcher.
class CONTENT_EXPORT WebBluetoothImpl
    : NON_EXPORTED_BASE(public blink::WebBluetooth) {
 public:
  explicit WebBluetoothImpl(ThreadSafeSender* thread_safe_sender);
  ~WebBluetoothImpl();

  // blink::WebBluetooth interface:
  void requestDevice(
      blink::WebBluetoothRequestDeviceCallbacks* callbacks) override;
  void connectGATT(const blink::WebString& device_instance_id,
      blink::WebBluetoothConnectGATTCallbacks* callbacks) override;
  void getPrimaryService(
      const blink::WebString& device_instance_id,
      const blink::WebString& service_uuid,
      blink::WebBluetoothGetPrimaryServiceCallbacks* callbacks) override;
  void getCharacteristic(
      const blink::WebString& service_instance_id,
      const blink::WebString& characteristic_uuid,
      blink::WebBluetoothGetCharacteristicCallbacks* callbacks) override;

 private:
  BluetoothDispatcher* GetDispatcher();

  scoped_refptr<ThreadSafeSender> thread_safe_sender_;

  DISALLOW_COPY_AND_ASSIGN(WebBluetoothImpl);
};

}  // namespace content

#endif  // CONTENT_CHILD_BLUETOOTH_WEB_BLUETOOTH_IMPL_H_
