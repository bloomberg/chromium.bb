// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/bluetooth/bluetooth_dispatcher.h"

#include "base/lazy_instance.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/thread_task_runner_handle.h"
#include "content/child/thread_safe_sender.h"
#include "content/common/bluetooth/bluetooth_messages.h"
#include "device/bluetooth/bluetooth_uuid.h"
#include "third_party/WebKit/public/platform/WebPassOwnPtr.h"
#include "third_party/WebKit/public/platform/modules/bluetooth/WebBluetoothDevice.h"
#include "third_party/WebKit/public/platform/modules/bluetooth/WebBluetoothError.h"
#include "third_party/WebKit/public/platform/modules/bluetooth/WebBluetoothGATTCharacteristic.h"
#include "third_party/WebKit/public/platform/modules/bluetooth/WebBluetoothGATTRemoteServer.h"
#include "third_party/WebKit/public/platform/modules/bluetooth/WebBluetoothGATTService.h"
#include "third_party/WebKit/public/platform/modules/bluetooth/WebRequestDeviceOptions.h"

using blink::WebBluetoothConnectGATTCallbacks;
using blink::WebBluetoothDevice;
using blink::WebBluetoothError;
using blink::WebBluetoothGATTCharacteristic;
using blink::WebBluetoothGATTRemoteServer;
using blink::WebBluetoothGATTService;
using blink::WebBluetoothReadValueCallbacks;
using blink::WebBluetoothRequestDeviceCallbacks;
using blink::WebBluetoothScanFilter;
using blink::WebRequestDeviceOptions;
using blink::WebString;
using blink::WebVector;

struct BluetoothPrimaryServiceRequest {
  BluetoothPrimaryServiceRequest(
      blink::WebString device_instance_id,
      blink::WebString service_uuid,
      blink::WebBluetoothGetPrimaryServiceCallbacks* callbacks)
      : device_instance_id(device_instance_id),
        service_uuid(service_uuid),
        callbacks(callbacks) {}
  ~BluetoothPrimaryServiceRequest() {}

  blink::WebString device_instance_id;
  blink::WebString service_uuid;
  scoped_ptr<blink::WebBluetoothGetPrimaryServiceCallbacks> callbacks;
};

struct BluetoothCharacteristicRequest {
  BluetoothCharacteristicRequest(
      blink::WebString service_instance_id,
      blink::WebString characteristic_uuid,
      blink::WebBluetoothGetCharacteristicCallbacks* callbacks)
      : service_instance_id(service_instance_id),
        characteristic_uuid(characteristic_uuid),
        callbacks(callbacks) {}
  ~BluetoothCharacteristicRequest() {}

  blink::WebString service_instance_id;
  blink::WebString characteristic_uuid;
  scoped_ptr<blink::WebBluetoothGetCharacteristicCallbacks> callbacks;
};

namespace content {

namespace {

base::LazyInstance<base::ThreadLocalPointer<void>>::Leaky g_dispatcher_tls =
    LAZY_INSTANCE_INITIALIZER;

void* const kHasBeenDeleted = reinterpret_cast<void*>(0x1);

int CurrentWorkerId() {
  return WorkerTaskRunner::Instance()->CurrentWorkerId();
}

WebBluetoothDevice::VendorIDSource GetWebVendorIdSource(
    device::BluetoothDevice::VendorIDSource vendor_id_source) {
  switch (vendor_id_source) {
    case device::BluetoothDevice::VENDOR_ID_UNKNOWN:
      return WebBluetoothDevice::VendorIDSource::Unknown;
    case device::BluetoothDevice::VENDOR_ID_BLUETOOTH:
      return WebBluetoothDevice::VendorIDSource::Bluetooth;
    case device::BluetoothDevice::VENDOR_ID_USB:
      return WebBluetoothDevice::VendorIDSource::USB;
  }
  NOTREACHED();
  return WebBluetoothDevice::VendorIDSource::Unknown;
}

}  // namespace

BluetoothDispatcher::BluetoothDispatcher(ThreadSafeSender* sender)
    : thread_safe_sender_(sender) {
  g_dispatcher_tls.Pointer()->Set(static_cast<void*>(this));
}

BluetoothDispatcher::~BluetoothDispatcher() {
  g_dispatcher_tls.Pointer()->Set(kHasBeenDeleted);
}

BluetoothDispatcher* BluetoothDispatcher::GetOrCreateThreadSpecificInstance(
    ThreadSafeSender* thread_safe_sender) {
  if (g_dispatcher_tls.Pointer()->Get() == kHasBeenDeleted) {
    NOTREACHED() << "Re-instantiating TLS BluetoothDispatcher.";
    g_dispatcher_tls.Pointer()->Set(NULL);
  }
  if (g_dispatcher_tls.Pointer()->Get())
    return static_cast<BluetoothDispatcher*>(g_dispatcher_tls.Pointer()->Get());

  BluetoothDispatcher* dispatcher = new BluetoothDispatcher(thread_safe_sender);
  if (CurrentWorkerId())
    WorkerTaskRunner::Instance()->AddStopObserver(dispatcher);
  return dispatcher;
}

bool BluetoothDispatcher::Send(IPC::Message* msg) {
  return thread_safe_sender_->Send(msg);
}

void BluetoothDispatcher::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(BluetoothDispatcher, msg)
  IPC_MESSAGE_HANDLER(BluetoothMsg_RequestDeviceSuccess,
                      OnRequestDeviceSuccess);
  IPC_MESSAGE_HANDLER(BluetoothMsg_RequestDeviceError, OnRequestDeviceError);
  IPC_MESSAGE_HANDLER(BluetoothMsg_ConnectGATTSuccess, OnConnectGATTSuccess);
  IPC_MESSAGE_HANDLER(BluetoothMsg_ConnectGATTError, OnConnectGATTError);
  IPC_MESSAGE_HANDLER(BluetoothMsg_GetPrimaryServiceSuccess,
                      OnGetPrimaryServiceSuccess);
  IPC_MESSAGE_HANDLER(BluetoothMsg_GetPrimaryServiceError,
                      OnGetPrimaryServiceError);
  IPC_MESSAGE_HANDLER(BluetoothMsg_GetCharacteristicSuccess,
                      OnGetCharacteristicSuccess);
  IPC_MESSAGE_HANDLER(BluetoothMsg_GetCharacteristicError,
                      OnGetCharacteristicError);
  IPC_MESSAGE_HANDLER(BluetoothMsg_ReadCharacteristicValueSuccess,
                      OnReadValueSuccess);
  IPC_MESSAGE_HANDLER(BluetoothMsg_ReadCharacteristicValueError,
                      OnReadValueError);
  IPC_MESSAGE_HANDLER(BluetoothMsg_WriteCharacteristicValueSuccess,
                      OnWriteValueSuccess);
  IPC_MESSAGE_HANDLER(BluetoothMsg_WriteCharacteristicValueError,
                      OnWriteValueError);
  IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  DCHECK(handled) << "Unhandled message:" << msg.type();
}

void BluetoothDispatcher::requestDevice(
    int frame_routing_id,
    const WebRequestDeviceOptions& options,
    blink::WebBluetoothRequestDeviceCallbacks* callbacks) {
  int request_id = pending_requests_.Add(callbacks);

  // Convert |options| to its IPC form.
  std::vector<content::BluetoothScanFilter> filters(options.filters.size());
  for (size_t i = 0; i < options.filters.size(); ++i) {
    const WebBluetoothScanFilter& web_filter = options.filters[i];
    BluetoothScanFilter& filter = filters[i];
    filter.services.reserve(web_filter.services.size());
    for (const WebString& service : web_filter.services) {
      filter.services.push_back(device::BluetoothUUID(service.utf8()));
    }
  }
  std::vector<device::BluetoothUUID> optional_services;
  optional_services.reserve(options.optionalServices.size());
  for (const WebString& optional_service : options.optionalServices) {
    optional_services.push_back(device::BluetoothUUID(optional_service.utf8()));
  }

  Send(new BluetoothHostMsg_RequestDevice(CurrentWorkerId(), request_id,
                                          frame_routing_id, filters,
                                          optional_services));
}

void BluetoothDispatcher::connectGATT(
    const blink::WebString& device_instance_id,
    blink::WebBluetoothConnectGATTCallbacks* callbacks) {
  int request_id = pending_connect_requests_.Add(callbacks);
  Send(new BluetoothHostMsg_ConnectGATT(CurrentWorkerId(), request_id,
                                        device_instance_id.utf8()));
}

void BluetoothDispatcher::getPrimaryService(
    const blink::WebString& device_instance_id,
    const blink::WebString& service_uuid,
    blink::WebBluetoothGetPrimaryServiceCallbacks* callbacks) {
  int request_id =
      pending_primary_service_requests_.Add(new BluetoothPrimaryServiceRequest(
          device_instance_id, service_uuid, callbacks));
  Send(new BluetoothHostMsg_GetPrimaryService(CurrentWorkerId(), request_id,
                                              device_instance_id.utf8(),
                                              service_uuid.utf8()));
}

void BluetoothDispatcher::getCharacteristic(
    const blink::WebString& service_instance_id,
    const blink::WebString& characteristic_uuid,
    blink::WebBluetoothGetCharacteristicCallbacks* callbacks) {
  int request_id =
      pending_characteristic_requests_.Add(new BluetoothCharacteristicRequest(
          service_instance_id, characteristic_uuid, callbacks));
  Send(new BluetoothHostMsg_GetCharacteristic(CurrentWorkerId(), request_id,
                                              service_instance_id.utf8(),
                                              characteristic_uuid.utf8()));
}

void BluetoothDispatcher::readValue(
    const blink::WebString& characteristic_instance_id,
    blink::WebBluetoothReadValueCallbacks* callbacks) {
  int request_id = pending_read_value_requests_.Add(callbacks);
  Send(new BluetoothHostMsg_ReadValue(CurrentWorkerId(), request_id,
                                      characteristic_instance_id.utf8()));
}

void BluetoothDispatcher::writeValue(
    const blink::WebString& characteristic_instance_id,
    const std::vector<uint8_t>& value,
    blink::WebBluetoothWriteValueCallbacks* callbacks) {
  int request_id = pending_write_value_requests_.Add(callbacks);

  Send(new BluetoothHostMsg_WriteValue(
      CurrentWorkerId(), request_id, characteristic_instance_id.utf8(), value));
}

void BluetoothDispatcher::OnWorkerRunLoopStopped() {
  delete this;
}

void BluetoothDispatcher::OnRequestDeviceSuccess(
    int thread_id,
    int request_id,
    const BluetoothDevice& device) {
  DCHECK(pending_requests_.Lookup(request_id)) << request_id;

  WebVector<WebString> uuids(device.uuids.size());
  for (size_t i = 0; i < device.uuids.size(); ++i)
    uuids[i] = WebString::fromUTF8(device.uuids[i].c_str());

  pending_requests_.Lookup(request_id)
      ->onSuccess(blink::adoptWebPtr(new WebBluetoothDevice(
          WebString::fromUTF8(device.instance_id), WebString(device.name),
          device.device_class, GetWebVendorIdSource(device.vendor_id_source),
          device.vendor_id, device.product_id, device.product_version,
          device.paired, uuids)));
  pending_requests_.Remove(request_id);
}

void BluetoothDispatcher::OnRequestDeviceError(int thread_id,
                                               int request_id,
                                               WebBluetoothError error) {
  DCHECK(pending_requests_.Lookup(request_id)) << request_id;
  pending_requests_.Lookup(request_id)->onError(WebBluetoothError(error));
  pending_requests_.Remove(request_id);
}

void BluetoothDispatcher::OnConnectGATTSuccess(
    int thread_id,
    int request_id,
    const std::string& device_instance_id) {
  DCHECK(pending_connect_requests_.Lookup(request_id)) << request_id;
  pending_connect_requests_.Lookup(request_id)
      ->onSuccess(blink::adoptWebPtr(new WebBluetoothGATTRemoteServer(
          WebString::fromUTF8(device_instance_id), true /* connected */)));
  pending_connect_requests_.Remove(request_id);
}

void BluetoothDispatcher::OnConnectGATTError(int thread_id,
                                             int request_id,
                                             WebBluetoothError error) {
  DCHECK(pending_connect_requests_.Lookup(request_id)) << request_id;
  pending_connect_requests_.Lookup(request_id)
      ->onError(WebBluetoothError(error));
  pending_connect_requests_.Remove(request_id);
}

void BluetoothDispatcher::OnGetPrimaryServiceSuccess(
    int thread_id,
    int request_id,
    const std::string& service_instance_id) {
  DCHECK(pending_primary_service_requests_.Lookup(request_id)) << request_id;
  BluetoothPrimaryServiceRequest* request =
      pending_primary_service_requests_.Lookup(request_id);
  request->callbacks->onSuccess(blink::adoptWebPtr(new WebBluetoothGATTService(
      WebString::fromUTF8(service_instance_id), request->service_uuid,
      true /* isPrimary */, request->device_instance_id)));
  pending_primary_service_requests_.Remove(request_id);
}

void BluetoothDispatcher::OnGetPrimaryServiceError(int thread_id,
                                                   int request_id,
                                                   WebBluetoothError error) {
  DCHECK(pending_primary_service_requests_.Lookup(request_id)) << request_id;

  // Since we couldn't find the service return null. See Step 3 of
  // getPrimaryService algorithm:
  // https://webbluetoothchrome.github.io/web-bluetooth/#dom-bluetoothgattremoteserver-getprimaryservice
  if (error == WebBluetoothError::ServiceNotFound) {
    pending_primary_service_requests_.Lookup(request_id)
        ->callbacks->onSuccess(nullptr);
    pending_primary_service_requests_.Remove(request_id);
    return;
  }

  pending_primary_service_requests_.Lookup(request_id)
      ->callbacks->onError(WebBluetoothError(error));
  pending_primary_service_requests_.Remove(request_id);
}

void BluetoothDispatcher::OnGetCharacteristicSuccess(
    int thread_id,
    int request_id,
    const std::string& characteristic_instance_id) {
  DCHECK(pending_characteristic_requests_.Lookup(request_id)) << request_id;

  BluetoothCharacteristicRequest* request =
      pending_characteristic_requests_.Lookup(request_id);
  request->callbacks->onSuccess(
      blink::adoptWebPtr(new WebBluetoothGATTCharacteristic(
          WebString::fromUTF8(characteristic_instance_id),
          request->service_instance_id, request->characteristic_uuid)));

  pending_characteristic_requests_.Remove(request_id);
}

void BluetoothDispatcher::OnGetCharacteristicError(int thread_id,
                                                   int request_id,
                                                   WebBluetoothError error) {
  DCHECK(pending_characteristic_requests_.Lookup(request_id)) << request_id;

  // Since we couldn't find the characteristic return null. See Step 3 of
  // getCharacteristic algorithm:
  // https://webbluetoothchrome.github.io/web-bluetooth/#dom-bluetoothgattservice-getcharacteristic
  if (error == WebBluetoothError::CharacteristicNotFound) {
    pending_characteristic_requests_.Lookup(request_id)
        ->callbacks->onSuccess(nullptr);
  } else {
    pending_characteristic_requests_.Lookup(request_id)
        ->callbacks->onError(WebBluetoothError(error));
  }
  pending_characteristic_requests_.Remove(request_id);
}

void BluetoothDispatcher::OnReadValueSuccess(
    int thread_id,
    int request_id,
    const std::vector<uint8_t>& value) {
  DCHECK(pending_read_value_requests_.Lookup(request_id)) << request_id;

  // WebArrayBuffer is not accessible from Source/modules so we pass a
  // WebVector instead.
  pending_read_value_requests_.Lookup(request_id)->onSuccess(value);

  pending_read_value_requests_.Remove(request_id);
}

void BluetoothDispatcher::OnReadValueError(int thread_id,
                                           int request_id,
                                           WebBluetoothError error) {
  DCHECK(pending_read_value_requests_.Lookup(request_id)) << request_id;

  pending_read_value_requests_.Lookup(request_id)
      ->onError(WebBluetoothError(error));

  pending_read_value_requests_.Remove(request_id);
}

void BluetoothDispatcher::OnWriteValueSuccess(int thread_id, int request_id) {
  DCHECK(pending_write_value_requests_.Lookup(request_id)) << request_id;

  pending_write_value_requests_.Lookup(request_id)->onSuccess();

  pending_write_value_requests_.Remove(request_id);
}

void BluetoothDispatcher::OnWriteValueError(int thread_id,
                                            int request_id,
                                            WebBluetoothError error) {
  DCHECK(pending_write_value_requests_.Lookup(request_id)) << request_id;

  pending_write_value_requests_.Lookup(request_id)
      ->onError(WebBluetoothError(error));

  pending_write_value_requests_.Remove(request_id);
}

}  // namespace content
