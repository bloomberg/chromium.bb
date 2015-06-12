// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/bluetooth/web_bluetooth_impl.h"

#include "content/child/bluetooth/bluetooth_dispatcher.h"
#include "content/child/thread_safe_sender.h"

namespace content {

WebBluetoothImpl::WebBluetoothImpl(ThreadSafeSender* thread_safe_sender)
    : thread_safe_sender_(thread_safe_sender) {
}

WebBluetoothImpl::~WebBluetoothImpl() {
}

void WebBluetoothImpl::requestDevice(
    blink::WebBluetoothRequestDeviceCallbacks* callbacks) {
  GetDispatcher()->requestDevice(callbacks);
}

void WebBluetoothImpl::connectGATT(const blink::WebString& device_instance_id,
    blink::WebBluetoothConnectGATTCallbacks* callbacks) {
  GetDispatcher()->connectGATT(device_instance_id, callbacks);
}

void WebBluetoothImpl::getPrimaryService(
    const blink::WebString& device_instance_id,
    const blink::WebString& service_uuid,
    blink::WebBluetoothGetPrimaryServiceCallbacks* callbacks) {
  GetDispatcher()->getPrimaryService(device_instance_id, service_uuid,
                                     callbacks);
}

void WebBluetoothImpl::getCharacteristic(
    const blink::WebString& service_instance_id,
    const blink::WebString& characteristic_uuid,
    blink::WebBluetoothGetCharacteristicCallbacks* callbacks) {
  GetDispatcher()->getCharacteristic(service_instance_id, characteristic_uuid,
                                     callbacks);
}

void WebBluetoothImpl::readValue(
    const blink::WebString& characteristic_instance_id,
    blink::WebBluetoothReadValueCallbacks* callbacks) {
  GetDispatcher()->readValue(characteristic_instance_id, callbacks);
}

BluetoothDispatcher* WebBluetoothImpl::GetDispatcher() {
  return BluetoothDispatcher::GetOrCreateThreadSpecificInstance(
      thread_safe_sender_.get());
}

}  // namespace content
