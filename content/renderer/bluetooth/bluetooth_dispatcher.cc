// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/bluetooth/bluetooth_dispatcher.h"

#include <stddef.h>

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
#include "third_party/WebKit/public/platform/modules/bluetooth/WebBluetoothGATTCharacteristicInit.h"
#include "third_party/WebKit/public/platform/modules/bluetooth/WebBluetoothGATTRemoteServer.h"
#include "third_party/WebKit/public/platform/modules/bluetooth/WebBluetoothGATTService.h"
#include "third_party/WebKit/public/platform/modules/bluetooth/WebRequestDeviceOptions.h"

using blink::WebBluetoothConnectGATTCallbacks;
using blink::WebBluetoothDevice;
using blink::WebBluetoothError;
using blink::WebBluetoothGATTCharacteristicInit;
using blink::WebBluetoothGATTRemoteServer;
using blink::WebBluetoothGATTService;
using blink::WebBluetoothReadValueCallbacks;
using blink::WebBluetoothRequestDeviceCallbacks;
using blink::WebBluetoothScanFilter;
using blink::WebRequestDeviceOptions;
using blink::WebString;
using blink::WebVector;
using NotificationsRequestType =
    content::BluetoothDispatcher::NotificationsRequestType;

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

// Struct that holds a pending Start/StopNotifications request.
struct BluetoothNotificationsRequest {
  BluetoothNotificationsRequest(
      int frame_routing_id,
      const std::string characteristic_instance_id,
      blink::WebBluetoothGATTCharacteristic* characteristic,
      blink::WebBluetoothNotificationsCallbacks* callbacks,
      NotificationsRequestType type)
      : frame_routing_id(frame_routing_id),
        characteristic_instance_id(characteristic_instance_id),
        characteristic(characteristic),
        callbacks(callbacks),
        type(type) {}
  ~BluetoothNotificationsRequest() {}

  const int frame_routing_id;
  const std::string characteristic_instance_id;
  // The characteristic object is owned by the execution context on
  // the blink side which can destroy the object at any point. Since the
  // object implements ActiveDOMObject, the object calls Stop when is getting
  // destroyed, which in turn calls characteristicObjectRemoved.
  // characteristicObjectRemoved will null any pointers to the object
  // and queue a stop notifications request if necessary.
  blink::WebBluetoothGATTCharacteristic* characteristic;
  scoped_ptr<blink::WebBluetoothNotificationsCallbacks> callbacks;
  NotificationsRequestType type;
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
  IPC_MESSAGE_HANDLER(BluetoothMsg_StartNotificationsSuccess,
                      OnStartNotificationsSuccess)
  IPC_MESSAGE_HANDLER(BluetoothMsg_StartNotificationsError,
                      OnStartNotificationsError)
  IPC_MESSAGE_HANDLER(BluetoothMsg_StopNotificationsSuccess,
                      OnStopNotificationsSuccess)
  IPC_MESSAGE_HANDLER(BluetoothMsg_CharacteristicValueChanged,
                      OnCharacteristicValueChanged)
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

void BluetoothDispatcher::connectGATT(
    int frame_routing_id,
    const blink::WebString& device_id,
    blink::WebBluetoothConnectGATTCallbacks* callbacks) {
  int request_id = pending_connect_requests_.Add(callbacks);
  Send(new BluetoothHostMsg_ConnectGATT(CurrentWorkerId(), request_id,
                                        frame_routing_id, device_id.utf8()));
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

void BluetoothDispatcher::readValue(
    int frame_routing_id,
    const blink::WebString& characteristic_instance_id,
    blink::WebBluetoothReadValueCallbacks* callbacks) {
  int request_id = pending_read_value_requests_.Add(callbacks);
  Send(new BluetoothHostMsg_ReadValue(CurrentWorkerId(), request_id,
                                      frame_routing_id,
                                      characteristic_instance_id.utf8()));
}

void BluetoothDispatcher::writeValue(
    int frame_routing_id,
    const blink::WebString& characteristic_instance_id,
    const blink::WebVector<uint8_t>& value,
    blink::WebBluetoothWriteValueCallbacks* callbacks) {
  int request_id = pending_write_value_requests_.Add(callbacks);

  Send(new BluetoothHostMsg_WriteValue(
      CurrentWorkerId(), request_id, frame_routing_id,
      characteristic_instance_id.utf8(),
      std::vector<uint8_t>(value.begin(), value.end())));
}

void BluetoothDispatcher::startNotifications(
    int frame_routing_id,
    const blink::WebString& characteristic_instance_id,
    blink::WebBluetoothGATTCharacteristic* characteristic,
    blink::WebBluetoothNotificationsCallbacks* callbacks) {
  int request_id = QueueNotificationRequest(
      frame_routing_id, characteristic_instance_id.utf8(), characteristic,
      callbacks, NotificationsRequestType::START);
  // The Notification subscription's state can change after a request
  // finishes. To avoid resolving with a soon-to-be-invalid state we queue
  // requests.
  if (HasNotificationRequestResponsePending(
          characteristic_instance_id.utf8())) {
    return;
  }

  ResolveOrSendStartNotificationRequest(request_id);
}

void BluetoothDispatcher::stopNotifications(
    int frame_routing_id,
    const blink::WebString& characteristic_instance_id,
    blink::WebBluetoothGATTCharacteristic* characteristic,
    blink::WebBluetoothNotificationsCallbacks* callbacks) {
  int request_id = QueueNotificationRequest(
      frame_routing_id, characteristic_instance_id.utf8(), characteristic,
      callbacks, NotificationsRequestType::STOP);
  if (HasNotificationRequestResponsePending(
          characteristic_instance_id.utf8())) {
    return;
  }

  ResolveOrSendStopNotificationsRequest(request_id);
}

void BluetoothDispatcher::characteristicObjectRemoved(
    int frame_routing_id,
    const blink::WebString& characteristic_instance_id,
    blink::WebBluetoothGATTCharacteristic* characteristic) {
  // We need to remove references to the object from the following:
  //  1) The set of active characteristics
  //  2) The queue waiting for a response
  //  3) The set of active notifications

  // 1
  // TODO(ortuno): We should only unregister a characteristic once
  // there are no characteristic objects that have listeners attached.
  // https://crbug.com/541388
  UnregisterCharacteristicObject(frame_routing_id, characteristic_instance_id);

  // 2
  // If the object is in the queue we null the characteristic. If this is the
  // first object waiting for a response OnStartNotificationsSuccess will make
  // sure not to add the characteristic to the map and it will queue a Stop
  // request. Otherwise ResolveOrSendStartNotificationRequest will make sure not
  // to add it to active notification subscriptions.
  bool found = false;
  for (IDMap<BluetoothNotificationsRequest, IDMapOwnPointer>::iterator iter(
           &pending_notifications_requests_);
       !iter.IsAtEnd(); iter.Advance()) {
    if (iter.GetCurrentValue()->characteristic == characteristic) {
      found = true;
      iter.GetCurrentValue()->characteristic = nullptr;
    }
  }

  if (found) {
    // A characteristic will never be in the set of active notifications
    // and in the queue at the same time.
    auto subscriptions_iter = active_notification_subscriptions_.find(
        characteristic_instance_id.utf8());
    if (subscriptions_iter != active_notification_subscriptions_.end()) {
      DCHECK(!ContainsKey(subscriptions_iter->second, characteristic));
    }
    return;
  }

  // 3
  // If the object is not in the queue then:
  //   1. The subscription was inactive already: this characteristic
  //      object didn't subscribe to notifications.
  //   2. The subscription will become inactive: the characteristic
  //      object that subscribed to notifications is getting destroyed.
  //   3. The subscription will still be active: there are other
  //      characteristic objects subscribed to notifications.

  if (!HasActiveNotificationSubscription(characteristic_instance_id.utf8())) {
    return;
  }

  // For 2 and 3 calling ResolveOrSendStopNotificationsRequest ensures the
  // notification subscription is released.
  // We pass in the characteristic so that ResolveOrSendStopNotificationsRequest
  // can remove the characteristic from ActiveNotificationSubscriptions.
  ResolveOrSendStopNotificationsRequest(QueueNotificationRequest(
      frame_routing_id, characteristic_instance_id.utf8(), characteristic,
      nullptr /* callbacks */, NotificationsRequestType::STOP));
}

void BluetoothDispatcher::registerCharacteristicObject(
    int frame_routing_id,
    const blink::WebString& characteristic_instance_id,
    blink::WebBluetoothGATTCharacteristic* characteristic) {
  // TODO(ortuno): After the Object manager is implemented, there will
  // only be one object per characteristic. But for now we remove
  // the previous object.
  // https://crbug.com/495270
  active_characteristics_.erase(characteristic_instance_id.utf8());

  active_characteristics_.insert(
      std::make_pair(characteristic_instance_id.utf8(), characteristic));

  Send(new BluetoothHostMsg_RegisterCharacteristic(
      CurrentWorkerId(), frame_routing_id, characteristic_instance_id.utf8()));
}

void BluetoothDispatcher::WillStopCurrentWorkerThread() {
  delete this;
}

int BluetoothDispatcher::QueueNotificationRequest(
    int frame_routing_id,
    const std::string& characteristic_instance_id,
    blink::WebBluetoothGATTCharacteristic* characteristic,
    blink::WebBluetoothNotificationsCallbacks* callbacks,
    NotificationsRequestType type) {
  int request_id =
      pending_notifications_requests_.Add(new BluetoothNotificationsRequest(
          frame_routing_id, characteristic_instance_id, characteristic,
          callbacks, type));
  notification_requests_queues_[characteristic_instance_id].push(request_id);

  return request_id;
}

void BluetoothDispatcher::PopNotificationRequestQueueAndProcessNext(
    int request_id) {
  BluetoothNotificationsRequest* old_request =
      pending_notifications_requests_.Lookup(request_id);

  auto queue_iter = notification_requests_queues_.find(
      old_request->characteristic_instance_id);

  CHECK(queue_iter != notification_requests_queues_.end());

  std::queue<int>& request_queue = queue_iter->second;

  DCHECK(request_queue.front() == request_id);

  // Remove old request and clean map if necessary.
  request_queue.pop();
  pending_notifications_requests_.Remove(request_id);
  if (request_queue.empty()) {
    notification_requests_queues_.erase(queue_iter);
    return;
  }

  int next_request_id = request_queue.front();
  BluetoothNotificationsRequest* next_request =
      pending_notifications_requests_.Lookup(next_request_id);

  switch (next_request->type) {
    case NotificationsRequestType::START:
      ResolveOrSendStartNotificationRequest(next_request_id);
      return;
    case NotificationsRequestType::STOP:
      ResolveOrSendStopNotificationsRequest(next_request_id);
      return;
  }
}

bool BluetoothDispatcher::HasNotificationRequestResponsePending(
    const std::string& characteristic_instance_id) {
  return ContainsKey(notification_requests_queues_,
                     characteristic_instance_id) &&
         (notification_requests_queues_[characteristic_instance_id].size() > 1);
}

bool BluetoothDispatcher::HasActiveNotificationSubscription(
    const std::string& characteristic_instance_id) {
  return ContainsKey(active_notification_subscriptions_,
                     characteristic_instance_id);
}

void BluetoothDispatcher::AddToActiveNotificationSubscriptions(
    const std::string& characteristic_instance_id,
    blink::WebBluetoothGATTCharacteristic* characteristic) {
  active_notification_subscriptions_[characteristic_instance_id].insert(
      characteristic);
}

bool BluetoothDispatcher::RemoveFromActiveNotificationSubscriptions(
    const std::string& characteristic_instance_id,
    blink::WebBluetoothGATTCharacteristic* characteristic) {
  auto active_map =
      active_notification_subscriptions_.find(characteristic_instance_id);

  if (active_map == active_notification_subscriptions_.end()) {
    return false;
  }

  active_map->second.erase(characteristic);

  if (active_map->second.empty()) {
    active_notification_subscriptions_.erase(active_map);
    DCHECK(!HasActiveNotificationSubscription(characteristic_instance_id));
    return true;
  }
  return false;
}

void BluetoothDispatcher::ResolveOrSendStartNotificationRequest(
    int request_id) {
  BluetoothNotificationsRequest* request =
      pending_notifications_requests_.Lookup(request_id);
  const int frame_routing_id = request->frame_routing_id;
  const std::string& characteristic_instance_id =
      request->characteristic_instance_id;
  blink::WebBluetoothGATTCharacteristic* characteristic =
      request->characteristic;
  blink::WebBluetoothNotificationsCallbacks* callbacks =
      request->callbacks.get();

  // If an object is already subscribed to notifications from the characteristic
  // no need to send an IPC again.
  if (HasActiveNotificationSubscription(characteristic_instance_id)) {
    // The object could have been destroyed while we waited.
    if (characteristic != nullptr) {
      AddToActiveNotificationSubscriptions(characteristic_instance_id,
                                           characteristic);
    }
    callbacks->onSuccess();

    PopNotificationRequestQueueAndProcessNext(request_id);
    return;
  }

  Send(new BluetoothHostMsg_StartNotifications(CurrentWorkerId(), request_id,
                                               frame_routing_id,
                                               characteristic_instance_id));
}

void BluetoothDispatcher::ResolveOrSendStopNotificationsRequest(
    int request_id) {
  // The Notification subscription's state can change after a request
  // finishes. To avoid resolving with a soon-to-be-invalid state we queue
  // requests.
  BluetoothNotificationsRequest* request =
      pending_notifications_requests_.Lookup(request_id);
  const int frame_routing_id = request->frame_routing_id;
  const std::string& characteristic_instance_id =
      request->characteristic_instance_id;
  blink::WebBluetoothGATTCharacteristic* characteristic =
      request->characteristic;
  blink::WebBluetoothNotificationsCallbacks* callbacks =
      request->callbacks.get();

  // If removing turns the subscription inactive then stop.
  if (RemoveFromActiveNotificationSubscriptions(characteristic_instance_id,
                                                characteristic)) {
    Send(new BluetoothHostMsg_StopNotifications(CurrentWorkerId(), request_id,
                                                frame_routing_id,
                                                characteristic_instance_id));
    return;
  }

  // We could be processing a request with nullptr callbacks due to:
  //   1) OnStartNotificationSuccess queues a Stop request because the object
  //      got destroyed in characteristicObjectRemoved.
  //   2) The last characteristic object that held this subscription got
  //      destroyed in characteristicObjectRemoved.
  if (callbacks != nullptr) {
    callbacks->onSuccess();
  }
  PopNotificationRequestQueueAndProcessNext(request_id);
}

void BluetoothDispatcher::UnregisterCharacteristicObject(
    int frame_routing_id,
    const blink::WebString& characteristic_instance_id) {
  int removed =
      active_characteristics_.erase(characteristic_instance_id.utf8());
  if (removed != 0) {
    Send(new BluetoothHostMsg_UnregisterCharacteristic(
        CurrentWorkerId(), frame_routing_id,
        characteristic_instance_id.utf8()));
  }
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

void BluetoothDispatcher::OnConnectGATTSuccess(int thread_id,
                                               int request_id,
                                               const std::string& device_id) {
  DCHECK(pending_connect_requests_.Lookup(request_id)) << request_id;
  pending_connect_requests_.Lookup(request_id)
      ->onSuccess(blink::adoptWebPtr(new WebBluetoothGATTRemoteServer(
          WebString::fromUTF8(device_id), true /* connected */)));
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
      blink::adoptWebPtr(new WebBluetoothGATTCharacteristicInit(
          WebString::fromUTF8(characteristic_instance_id),
          request->service_instance_id, request->characteristic_uuid,
          characteristic_properties)));

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

void BluetoothDispatcher::OnStartNotificationsSuccess(int thread_id,
                                                      int request_id) {
  DCHECK(pending_notifications_requests_.Lookup(request_id)) << request_id;

  BluetoothNotificationsRequest* request =
      pending_notifications_requests_.Lookup(request_id);

  DCHECK(notification_requests_queues_[request->characteristic_instance_id]
             .front() == request_id);

  // We only send an IPC for inactive notifications.
  DCHECK(
      !HasActiveNotificationSubscription(request->characteristic_instance_id));

  AddToActiveNotificationSubscriptions(request->characteristic_instance_id,
                                       request->characteristic);

  // The object requesting the notification could have been destroyed
  // while waiting for the subscription. characteristicRemoved
  // nulls the characteristic when the corresponding js object gets destroyed.
  // A Stop request must be issued as the subscription is no longer needed.
  // The Stop requets is added to the end of the queue in case another
  // Start request exists in the queue from another characteristic object,
  // which would result in the subscription continuing.
  if (request->characteristic == nullptr) {
    QueueNotificationRequest(
        request->frame_routing_id, request->characteristic_instance_id,
        nullptr /* characteristic */, nullptr /* callbacks */,
        NotificationsRequestType::STOP);
  }

  request->callbacks->onSuccess();

  PopNotificationRequestQueueAndProcessNext(request_id);
}

void BluetoothDispatcher::OnStartNotificationsError(int thread_id,
                                                    int request_id,
                                                    WebBluetoothError error) {
  DCHECK(pending_notifications_requests_.Lookup(request_id)) << request_id;

  BluetoothNotificationsRequest* request =
      pending_notifications_requests_.Lookup(request_id);

  DCHECK(notification_requests_queues_[request->characteristic_instance_id]
             .front() == request_id);

  // We only send an IPC for inactive notifications.
  DCHECK(
      !HasActiveNotificationSubscription(request->characteristic_instance_id));

  request->callbacks->onError(error);

  PopNotificationRequestQueueAndProcessNext(request_id);
}

void BluetoothDispatcher::OnStopNotificationsSuccess(int thread_id,
                                                     int request_id) {
  DCHECK(pending_notifications_requests_.Lookup(request_id)) << request_id;

  BluetoothNotificationsRequest* request =
      pending_notifications_requests_.Lookup(request_id);

  DCHECK(notification_requests_queues_[request->characteristic_instance_id]
             .front() == request_id);

  // We only send an IPC for inactive notifications.
  DCHECK(
      !HasActiveNotificationSubscription(request->characteristic_instance_id));

  if (request->callbacks != nullptr) {
    request->callbacks->onSuccess();
  }

  PopNotificationRequestQueueAndProcessNext(request_id);
}

void BluetoothDispatcher::OnCharacteristicValueChanged(
    int thread_id,
    const std::string& characteristic_instance_id,
    const std::vector<uint8_t> new_value) {
  auto active_iter = active_characteristics_.find(characteristic_instance_id);
  if (active_iter != active_characteristics_.end()) {
    active_iter->second->dispatchCharacteristicValueChanged(new_value);
  }
}

}  // namespace content
