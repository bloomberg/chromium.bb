// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/bluetooth/bluetooth_dispatcher.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/thread_task_runner_handle.h"
#include "content/child/thread_safe_sender.h"
#include "content/common/bluetooth/bluetooth_messages.h"
#include "device/bluetooth/bluetooth_uuid.h"
#include "third_party/WebKit/public/platform/modules/bluetooth/WebBluetoothDevice.h"
#include "third_party/WebKit/public/platform/modules/bluetooth/WebBluetoothError.h"
#include "third_party/WebKit/public/platform/modules/bluetooth/WebBluetoothRemoteGATTCharacteristic.h"
#include "third_party/WebKit/public/platform/modules/bluetooth/WebBluetoothRemoteGATTCharacteristicInit.h"
#include "third_party/WebKit/public/platform/modules/bluetooth/WebBluetoothRemoteGATTService.h"
#include "third_party/WebKit/public/platform/modules/bluetooth/WebRequestDeviceOptions.h"

using blink::WebBluetoothDevice;
using blink::WebBluetoothError;
using blink::WebBluetoothRemoteGATTCharacteristicInit;
using blink::WebBluetoothRemoteGATTServerConnectCallbacks;
using blink::WebBluetoothRemoteGATTService;
using blink::WebBluetoothReadValueCallbacks;
using blink::WebBluetoothRequestDeviceCallbacks;
using blink::WebBluetoothScanFilter;
using blink::WebRequestDeviceOptions;
using blink::WebString;
using blink::WebVector;

struct BluetoothPrimaryServiceRequest {
  BluetoothPrimaryServiceRequest(
      blink::WebString device_id,
      blink::WebString service_uuid,
      blink::WebBluetoothGetPrimaryServiceCallbacks* callbacks)
      : device_id(device_id),
        service_uuid(service_uuid),
        callbacks(callbacks) {}
  ~BluetoothPrimaryServiceRequest() {}

  blink::WebString device_id;
  blink::WebString service_uuid;
  std::unique_ptr<blink::WebBluetoothGetPrimaryServiceCallbacks> callbacks;
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
  std::unique_ptr<blink::WebBluetoothGetCharacteristicCallbacks> callbacks;
};

struct BluetoothCharacteristicsRequest {
  BluetoothCharacteristicsRequest(
      blink::WebString service_instance_id,
      blink::WebBluetoothGetCharacteristicsCallbacks* callbacks)
      : service_instance_id(service_instance_id), callbacks(callbacks) {}
  ~BluetoothCharacteristicsRequest() {}

  blink::WebString service_instance_id;
  std::unique_ptr<blink::WebBluetoothGetCharacteristicsCallbacks> callbacks;
};

namespace content {

namespace {

base::LazyInstance<base::ThreadLocalPointer<void>>::Leaky g_dispatcher_tls =
    LAZY_INSTANCE_INITIALIZER;

void* const kHasBeenDeleted = reinterpret_cast<void*>(0x1);

int CurrentWorkerId() {
  return WorkerThread::GetCurrentId();
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
    WorkerThread::AddObserver(dispatcher);
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
  IPC_MESSAGE_HANDLER(BluetoothMsg_GATTServerConnectSuccess,
                      OnGATTServerConnectSuccess);
  IPC_MESSAGE_HANDLER(BluetoothMsg_GATTServerConnectError,
                      OnGATTServerConnectError);
  IPC_MESSAGE_HANDLER(BluetoothMsg_GetPrimaryServiceSuccess,
                      OnGetPrimaryServiceSuccess);
  IPC_MESSAGE_HANDLER(BluetoothMsg_GetPrimaryServiceError,
                      OnGetPrimaryServiceError);
  IPC_MESSAGE_HANDLER(BluetoothMsg_GetCharacteristicSuccess,
                      OnGetCharacteristicSuccess);
  IPC_MESSAGE_HANDLER(BluetoothMsg_GetCharacteristicError,
                      OnGetCharacteristicError);
  IPC_MESSAGE_HANDLER(BluetoothMsg_GetCharacteristicsSuccess,
                      OnGetCharacteristicsSuccess);
  IPC_MESSAGE_HANDLER(BluetoothMsg_GetCharacteristicsError,
                      OnGetCharacteristicsError);
  IPC_MESSAGE_HANDLER(BluetoothMsg_ReadCharacteristicValueSuccess,
                      OnReadValueSuccess);
  IPC_MESSAGE_HANDLER(BluetoothMsg_ReadCharacteristicValueError,
                      OnReadValueError);
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
    filter.name = web_filter.name.utf8();
    filter.namePrefix = web_filter.namePrefix.utf8();
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

void BluetoothDispatcher::connect(
    int frame_routing_id,
    const blink::WebString& device_id,
    blink::WebBluetoothRemoteGATTServerConnectCallbacks* callbacks) {
  int request_id = pending_connect_requests_.Add(callbacks);
  Send(new BluetoothHostMsg_GATTServerConnect(
      CurrentWorkerId(), request_id, frame_routing_id, device_id.utf8()));
}

void BluetoothDispatcher::disconnect(int frame_routing_id,
                                     const blink::WebString& device_id) {
  Send(new BluetoothHostMsg_GATTServerDisconnect(
      CurrentWorkerId(), frame_routing_id, device_id.utf8()));
}

void BluetoothDispatcher::getPrimaryService(
    int frame_routing_id,
    const blink::WebString& device_id,
    const blink::WebString& service_uuid,
    blink::WebBluetoothGetPrimaryServiceCallbacks* callbacks) {
  int request_id = pending_primary_service_requests_.Add(
      new BluetoothPrimaryServiceRequest(device_id, service_uuid, callbacks));
  Send(new BluetoothHostMsg_GetPrimaryService(
      CurrentWorkerId(), request_id, frame_routing_id, device_id.utf8(),
      service_uuid.utf8()));
}

void BluetoothDispatcher::getCharacteristic(
    int frame_routing_id,
    const blink::WebString& service_instance_id,
    const blink::WebString& characteristic_uuid,
    blink::WebBluetoothGetCharacteristicCallbacks* callbacks) {
  int request_id =
      pending_characteristic_requests_.Add(new BluetoothCharacteristicRequest(
          service_instance_id, characteristic_uuid, callbacks));
  Send(new BluetoothHostMsg_GetCharacteristic(
      CurrentWorkerId(), request_id, frame_routing_id,
      service_instance_id.utf8(), characteristic_uuid.utf8()));
}

void BluetoothDispatcher::getCharacteristics(
    int frame_routing_id,
    const blink::WebString& service_instance_id,
    const blink::WebString& characteristics_uuid,
    blink::WebBluetoothGetCharacteristicsCallbacks* callbacks) {
  int request_id = pending_characteristics_requests_.Add(
      new BluetoothCharacteristicsRequest(service_instance_id, callbacks));
  Send(new BluetoothHostMsg_GetCharacteristics(
      CurrentWorkerId(), request_id, frame_routing_id,
      service_instance_id.utf8(), characteristics_uuid.utf8()));
}

void BluetoothDispatcher::readValue(
    int frame_routing_id,
    const blink::WebString& characteristic_instance_id,
    blink::WebBluetoothReadValueCallbacks* callbacks) {
  int request_id = pending_read_value_requests_.Add(callbacks);
  Send(new BluetoothHostMsg_ReadValue(CurrentWorkerId(), request_id,
                                      frame_routing_id,
                                      characteristic_instance_id.utf8()));
}

void BluetoothDispatcher::WillStopCurrentWorkerThread() {
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
      ->onSuccess(base::WrapUnique(new WebBluetoothDevice(
          WebString::fromUTF8(device.id), WebString(device.name),
          device.tx_power, device.rssi, device.device_class,
          GetWebVendorIdSource(device.vendor_id_source), device.vendor_id,
          device.product_id, device.product_version, uuids)));
  pending_requests_.Remove(request_id);
}

void BluetoothDispatcher::OnRequestDeviceError(int thread_id,
                                               int request_id,
                                               WebBluetoothError error) {
  DCHECK(pending_requests_.Lookup(request_id)) << request_id;
  pending_requests_.Lookup(request_id)->onError(WebBluetoothError(error));
  pending_requests_.Remove(request_id);
}

void BluetoothDispatcher::OnGATTServerConnectSuccess(int thread_id,
                                                     int request_id) {
  DCHECK(pending_connect_requests_.Lookup(request_id)) << request_id;
  pending_connect_requests_.Lookup(request_id)->onSuccess();
  pending_connect_requests_.Remove(request_id);
}

void BluetoothDispatcher::OnGATTServerConnectError(int thread_id,
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
  request->callbacks->onSuccess(
      base::WrapUnique(new WebBluetoothRemoteGATTService(
          WebString::fromUTF8(service_instance_id), request->service_uuid,
          true /* isPrimary */, request->device_id)));
  pending_primary_service_requests_.Remove(request_id);
}

void BluetoothDispatcher::OnGetPrimaryServiceError(int thread_id,
                                                   int request_id,
                                                   WebBluetoothError error) {
  DCHECK(pending_primary_service_requests_.Lookup(request_id)) << request_id;

  pending_primary_service_requests_.Lookup(request_id)
      ->callbacks->onError(WebBluetoothError(error));
  pending_primary_service_requests_.Remove(request_id);
}

void BluetoothDispatcher::OnGetCharacteristicSuccess(
    int thread_id,
    int request_id,
    const std::string& characteristic_instance_id,
    uint32_t characteristic_properties) {
  DCHECK(pending_characteristic_requests_.Lookup(request_id)) << request_id;

  BluetoothCharacteristicRequest* request =
      pending_characteristic_requests_.Lookup(request_id);
  request->callbacks->onSuccess(
      base::WrapUnique(new WebBluetoothRemoteGATTCharacteristicInit(
          request->service_instance_id,
          WebString::fromUTF8(characteristic_instance_id),
          request->characteristic_uuid, characteristic_properties)));

  pending_characteristic_requests_.Remove(request_id);
}

void BluetoothDispatcher::OnGetCharacteristicError(int thread_id,
                                                   int request_id,
                                                   WebBluetoothError error) {
  DCHECK(pending_characteristic_requests_.Lookup(request_id)) << request_id;

  pending_characteristic_requests_.Lookup(request_id)
      ->callbacks->onError(WebBluetoothError(error));

  pending_characteristic_requests_.Remove(request_id);
}

void BluetoothDispatcher::OnGetCharacteristicsSuccess(
    int thread_id,
    int request_id,
    const std::vector<std::string>& characteristics_instance_ids,
    const std::vector<std::string>& characteristics_uuids,
    const std::vector<uint32_t>& characteristics_properties) {
  DCHECK(pending_characteristics_requests_.Lookup(request_id)) << request_id;

  BluetoothCharacteristicsRequest* request =
      pending_characteristics_requests_.Lookup(request_id);

  // TODO(dcheng): This WebVector should use smart pointers.
  std::unique_ptr<WebVector<blink::WebBluetoothRemoteGATTCharacteristicInit*>>
  characteristics(new WebVector<WebBluetoothRemoteGATTCharacteristicInit*>(
      characteristics_instance_ids.size()));

  for (size_t i = 0; i < characteristics_instance_ids.size(); i++) {
    (*characteristics)[i] = new WebBluetoothRemoteGATTCharacteristicInit(
        request->service_instance_id,
        WebString::fromUTF8(characteristics_instance_ids[i]),
        WebString::fromUTF8(characteristics_uuids[i]),
        characteristics_properties[i]);
  }

  request->callbacks->onSuccess(std::move(characteristics));

  pending_characteristics_requests_.Remove(request_id);
}

void BluetoothDispatcher::OnGetCharacteristicsError(int thread_id,
                                                    int request_id,
                                                    WebBluetoothError error) {
  DCHECK(pending_characteristics_requests_.Lookup(request_id)) << request_id;

  pending_characteristics_requests_.Lookup(request_id)
      ->callbacks->onError(WebBluetoothError(error));

  pending_characteristics_requests_.Remove(request_id);
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

}  // namespace content
