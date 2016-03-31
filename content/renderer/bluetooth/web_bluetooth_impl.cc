// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/bluetooth/web_bluetooth_impl.h"

#include "content/child/mojo/type_converters.h"
#include "content/child/thread_safe_sender.h"
#include "content/public/common/service_registry.h"
#include "content/renderer/bluetooth/bluetooth_dispatcher.h"
#include "ipc/ipc_message.h"
#include "mojo/public/cpp/bindings/array.h"

namespace content {

WebBluetoothImpl::WebBluetoothImpl(ServiceRegistry* service_registry,
                                   ThreadSafeSender* thread_safe_sender,
                                   int frame_routing_id)
    : service_registry_(service_registry),
      thread_safe_sender_(thread_safe_sender),
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
    blink::WebBluetoothRemoteGATTServerConnectCallbacks* callbacks) {
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
    const blink::WebString& characteristics_uuid,
    blink::WebBluetoothGetCharacteristicCallbacks* callbacks) {
  GetDispatcher()->getCharacteristic(frame_routing_id_, service_instance_id,
                                     characteristics_uuid, callbacks);
}

void WebBluetoothImpl::getCharacteristics(
    const blink::WebString& service_instance_id,
    const blink::WebString& characteristic_uuid,
    blink::WebBluetoothGetCharacteristicsCallbacks* callbacks) {
  GetDispatcher()->getCharacteristics(frame_routing_id_, service_instance_id,
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
  GetWebBluetoothService().RemoteCharacteristicWriteValue(
      mojo::String::From(characteristic_instance_id),
      mojo::Array<uint8_t>::From(value),
      base::Bind(&WebBluetoothImpl::OnWriteValueComplete,
                 base::Unretained(this), value,
                 base::Passed(make_scoped_ptr(callbacks))));
}

void WebBluetoothImpl::startNotifications(
    const blink::WebString& characteristic_instance_id,
    blink::WebBluetoothRemoteGATTCharacteristic* characteristic,
    blink::WebBluetoothNotificationsCallbacks* callbacks) {
  GetDispatcher()->startNotifications(
      frame_routing_id_, characteristic_instance_id, characteristic, callbacks);
}

void WebBluetoothImpl::stopNotifications(
    const blink::WebString& characteristic_instance_id,
    blink::WebBluetoothRemoteGATTCharacteristic* characteristic,
    blink::WebBluetoothNotificationsCallbacks* callbacks) {
  GetDispatcher()->stopNotifications(
      frame_routing_id_, characteristic_instance_id, characteristic, callbacks);
}

void WebBluetoothImpl::characteristicObjectRemoved(
    const blink::WebString& characteristic_instance_id,
    blink::WebBluetoothRemoteGATTCharacteristic* characteristic) {
  GetDispatcher()->characteristicObjectRemoved(
      frame_routing_id_, characteristic_instance_id, characteristic);
}

void WebBluetoothImpl::registerCharacteristicObject(
    const blink::WebString& characteristic_instance_id,
    blink::WebBluetoothRemoteGATTCharacteristic* characteristic) {
  GetDispatcher()->registerCharacteristicObject(
      frame_routing_id_, characteristic_instance_id, characteristic);
}

void WebBluetoothImpl::OnWriteValueComplete(
    const blink::WebVector<uint8_t>& value,
    scoped_ptr<blink::WebBluetoothWriteValueCallbacks> callbacks,
    blink::mojom::WebBluetoothError error) {
  if (error == blink::mojom::WebBluetoothError::SUCCESS) {
    callbacks->onSuccess(value);
  } else {
    callbacks->onError(error);
  }
}

BluetoothDispatcher* WebBluetoothImpl::GetDispatcher() {
  return BluetoothDispatcher::GetOrCreateThreadSpecificInstance(
      thread_safe_sender_.get());
}

blink::mojom::WebBluetoothService& WebBluetoothImpl::GetWebBluetoothService() {
  if (!web_bluetooth_service_) {
    service_registry_->ConnectToRemoteService(
        mojo::GetProxy(&web_bluetooth_service_));
  }
  return *web_bluetooth_service_;
}

}  // namespace content
