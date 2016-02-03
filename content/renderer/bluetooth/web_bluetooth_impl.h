// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_BLUETOOTH_WEB_BLUETOOTH_IMPL_H_
#define CONTENT_CHILD_BLUETOOTH_WEB_BLUETOOTH_IMPL_H_

#include <stdint.h>

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/modules/bluetooth/WebBluetooth.h"

namespace blink {
class WebBluetoothGATTCharacteristic;
}

namespace content {

class BluetoothDispatcher;
class ThreadSafeSender;

// Implementation of blink::WebBluetooth. Passes calls through to the thread
// specific BluetoothDispatcher.
class CONTENT_EXPORT WebBluetoothImpl
    : NON_EXPORTED_BASE(public blink::WebBluetooth) {
 public:
  explicit WebBluetoothImpl(ThreadSafeSender* thread_safe_sender);
  WebBluetoothImpl(ThreadSafeSender* thread_safe_sender, int frame_routing_id);
  ~WebBluetoothImpl() override;

  // blink::WebBluetooth interface:
  void requestDevice(
      const blink::WebRequestDeviceOptions& options,
      blink::WebBluetoothRequestDeviceCallbacks* callbacks) override;
  void connect(
      const blink::WebString& device_id,
      blink::WebBluetoothGATTServerConnectCallbacks* callbacks) override;
  void disconnect(const blink::WebString& device_id) override;
  void getPrimaryService(
      const blink::WebString& device_id,
      const blink::WebString& service_uuid,
      blink::WebBluetoothGetPrimaryServiceCallbacks* callbacks) override;
  void getCharacteristic(
      const blink::WebString& service_instance_id,
      const blink::WebString& characteristic_uuid,
      blink::WebBluetoothGetCharacteristicCallbacks* callbacks) override;
  void readValue(const blink::WebString& characteristic_instance_id,
                 blink::WebBluetoothReadValueCallbacks* callbacks) override;
  void writeValue(const blink::WebString& characteristic_instance_id,
                  const blink::WebVector<uint8_t>& value,
                  blink::WebBluetoothWriteValueCallbacks*) override;
  void startNotifications(const blink::WebString& characteristic_instance_id,
                          blink::WebBluetoothGATTCharacteristic* characteristic,
                          blink::WebBluetoothNotificationsCallbacks*) override;
  void stopNotifications(const blink::WebString& characteristic_instance_id,
                         blink::WebBluetoothGATTCharacteristic* characteristic,
                         blink::WebBluetoothNotificationsCallbacks*) override;
  void characteristicObjectRemoved(
      const blink::WebString& characteristic_instance_id,
      blink::WebBluetoothGATTCharacteristic* characteristic) override;
  void registerCharacteristicObject(
      const blink::WebString& characteristic_instance_id,
      blink::WebBluetoothGATTCharacteristic* characteristic) override;

 private:
  BluetoothDispatcher* GetDispatcher();

  const scoped_refptr<ThreadSafeSender> thread_safe_sender_;
  const int frame_routing_id_;

  DISALLOW_COPY_AND_ASSIGN(WebBluetoothImpl);
};

}  // namespace content

#endif  // CONTENT_CHILD_BLUETOOTH_WEB_BLUETOOTH_IMPL_H_
