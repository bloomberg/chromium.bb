// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/bluetooth/web_bluetooth_impl.h"

#include "content/child/thread_safe_sender.h"
#include "content/renderer/bluetooth/bluetooth_dispatcher.h"
#include "ipc/ipc_message.h"

namespace content {

WebBluetoothImpl::WebBluetoothImpl(ThreadSafeSender* thread_safe_sender)
    : WebBluetoothImpl(thread_safe_sender, MSG_ROUTING_NONE) {}

WebBluetoothImpl::WebBluetoothImpl(ThreadSafeSender* thread_safe_sender,
                                   int frame_routing_id)
    : thread_safe_sender_(thread_safe_sender),
      frame_routing_id_(frame_routing_id) {}

WebBluetoothImpl::~WebBluetoothImpl() {
}

void WebBluetoothImpl::requestDevice(
    const blink::WebRequestDeviceOptions& options,
    blink::WebBluetoothRequestDeviceCallbacks* callbacks) {
  GetDispatcher()->requestDevice(frame_routing_id_, options, callbacks);
}

void WebBluetoothImpl::connect(
    const blink::WebString& device_id,
    blink::WebBluetoothGATTServerConnectCallbacks* callbacks) {
  GetDispatcher()->connect(frame_routing_id_, device_id, callbacks);
}

void WebBluetoothImpl::disconnect(const blink::WebString& device_id) {
  GetDispatcher()->disconnect(frame_routing_id_, device_id);
}

void WebBluetoothImpl::getPrimaryService(
    const blink::WebString& device_id,
    const blink::WebString& service_uuid,
    blink::WebBluetoothGetPrimaryServiceCallbacks* callbacks) {
  GetDispatcher()->getPrimaryService(frame_routing_id_, device_id, service_uuid,
                                     callbacks);
}

void WebBluetoothImpl::getCharacteristic(
    const blink::WebString& service_instance_id,
    const blink::WebString& characteristic_uuid,
    blink::WebBluetoothGetCharacteristicCallbacks* callbacks) {
  GetDispatcher()->getCharacteristic(frame_routing_id_, service_instance_id,
                                     characteristic_uuid, callbacks);
}

void WebBluetoothImpl::readValue(
    const blink::WebString& characteristic_instance_id,
    blink::WebBluetoothReadValueCallbacks* callbacks) {
  GetDispatcher()->readValue(frame_routing_id_, characteristic_instance_id,
                             callbacks);
}

void WebBluetoothImpl::writeValue(
    const blink::WebString& characteristic_instance_id,
    const blink::WebVector<uint8_t>& value,
    blink::WebBluetoothWriteValueCallbacks* callbacks) {
  GetDispatcher()->writeValue(frame_routing_id_, characteristic_instance_id,
                              value, callbacks);
}

void WebBluetoothImpl::startNotifications(
    const blink::WebString& characteristic_instance_id,
    blink::WebBluetoothGATTCharacteristic* characteristic,
    blink::WebBluetoothNotificationsCallbacks* callbacks) {
  GetDispatcher()->startNotifications(
      frame_routing_id_, characteristic_instance_id, characteristic, callbacks);
}

void WebBluetoothImpl::stopNotifications(
    const blink::WebString& characteristic_instance_id,
    blink::WebBluetoothGATTCharacteristic* characteristic,
    blink::WebBluetoothNotificationsCallbacks* callbacks) {
  GetDispatcher()->stopNotifications(
      frame_routing_id_, characteristic_instance_id, characteristic, callbacks);
}

void WebBluetoothImpl::characteristicObjectRemoved(
    const blink::WebString& characteristic_instance_id,
    blink::WebBluetoothGATTCharacteristic* characteristic) {
  GetDispatcher()->characteristicObjectRemoved(
      frame_routing_id_, characteristic_instance_id, characteristic);
}

void WebBluetoothImpl::registerCharacteristicObject(
    const blink::WebString& characteristic_instance_id,
    blink::WebBluetoothGATTCharacteristic* characteristic) {
  GetDispatcher()->registerCharacteristicObject(
      frame_routing_id_, characteristic_instance_id, characteristic);
}

BluetoothDispatcher* WebBluetoothImpl::GetDispatcher() {
  return BluetoothDispatcher::GetOrCreateThreadSpecificInstance(
      thread_safe_sender_.get());
}

}  // namespace content
