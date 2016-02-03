// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_BLUETOOTH_BLUETOOTH_DISPATCHER_H_
#define CONTENT_CHILD_BLUETOOTH_BLUETOOTH_DISPATCHER_H_

#include <stdint.h>

#include <map>
#include <queue>

#include "base/id_map.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/common/bluetooth/bluetooth_device.h"
#include "content/public/child/worker_thread.h"
#include "third_party/WebKit/public/platform/modules/bluetooth/WebBluetooth.h"
#include "third_party/WebKit/public/platform/modules/bluetooth/WebBluetoothError.h"

namespace base {
class MessageLoop;
class TaskRunner;
}

namespace blink {
class WebBluetoothGATTCharacteristic;
}

namespace IPC {
class Message;
}

struct BluetoothCharacteristicRequest;
struct BluetoothPrimaryServiceRequest;
struct BluetoothWriteValueRequest;
struct BluetoothNotificationsRequest;

namespace content {
class ThreadSafeSender;

// Dispatcher for child process threads which communicates to the browser's
// BluetoothDispatcherHost.
//
// Instances are created for each thread as necessary by WebBluetoothImpl.
//
// Incoming IPC messages are received by the BluetoothMessageFilter and
// directed to the thread specific instance of this class.
// Outgoing messages come from WebBluetoothImpl.
class BluetoothDispatcher : public WorkerThread::Observer {
 public:
  explicit BluetoothDispatcher(ThreadSafeSender* sender);
  ~BluetoothDispatcher() override;

  // Gets or Creates a BluetoothDispatcher for the current thread.
  // |thread_safe_sender| is required when constructing a BluetoothDispatcher.
  static BluetoothDispatcher* GetOrCreateThreadSpecificInstance(
      ThreadSafeSender* thread_safe_sender);

  // IPC Send and Receiving interface, see IPC::Sender and IPC::Listener.
  bool Send(IPC::Message* msg);
  void OnMessageReceived(const IPC::Message& msg);

  // Corresponding to WebBluetoothImpl methods.
  void requestDevice(int frame_routing_id,
                     const blink::WebRequestDeviceOptions& options,
                     blink::WebBluetoothRequestDeviceCallbacks* callbacks);
  void connect(int frame_routing_id,
               const blink::WebString& device_id,
               blink::WebBluetoothGATTServerConnectCallbacks* callbacks);
  void disconnect(int frame_routing_id, const blink::WebString& device_id);
  void getPrimaryService(
      int frame_routing_id,
      const blink::WebString& device_id,
      const blink::WebString& service_uuid,
      blink::WebBluetoothGetPrimaryServiceCallbacks* callbacks);

  void getCharacteristic(
      int frame_routing_id,
      const blink::WebString& service_instance_id,
      const blink::WebString& characteristic_uuid,
      blink::WebBluetoothGetCharacteristicCallbacks* callbacks);
  void readValue(int frame_routing_id,
                 const blink::WebString& characteristic_instance_id,
                 blink::WebBluetoothReadValueCallbacks* callbacks);
  void writeValue(int frame_routing_id,
                  const blink::WebString& characteristic_instance_id,
                  const blink::WebVector<uint8_t>& value,
                  blink::WebBluetoothWriteValueCallbacks*);
  void startNotifications(int frame_routing_id,
                          const blink::WebString& characteristic_instance_id,
                          blink::WebBluetoothGATTCharacteristic* delegate,
                          blink::WebBluetoothNotificationsCallbacks*);
  void stopNotifications(int frame_routing_id,
                         const blink::WebString& characteristic_instance_id,
                         blink::WebBluetoothGATTCharacteristic* delegate,
                         blink::WebBluetoothNotificationsCallbacks*);
  void characteristicObjectRemoved(
      int frame_routing_id,
      const blink::WebString& characteristic_instance_id,
      blink::WebBluetoothGATTCharacteristic* delegate);
  void registerCharacteristicObject(
      int frame_routing_id,
      const blink::WebString& characteristic_instance_id,
      blink::WebBluetoothGATTCharacteristic* characteristic);

  // WorkerThread::Observer implementation.
  void WillStopCurrentWorkerThread() override;

  enum class NotificationsRequestType { START = 0, STOP = 1 };

 private:
  // Notifications Queueing Notes:
  // To avoid races and sending unnecessary IPC messages we implement
  // a queueing system for notification requests. When receiving
  // a notification request, the request is immediately queued. If
  // there are no other pending requests then the request is processed.
  // When a characteristic object gets destroyed BluetoothDispatcher
  // gets notified by characteristicObjectRemoved. When this happens
  // a stop request should be queued if the characteristic was subscribed
  // to notifications.

  // Helper functions for notification requests queue:

  // Creates a notification request and queues it.
  int QueueNotificationRequest(
      int frame_routing_id,
      const std::string& characteristic_instance_id,
      blink::WebBluetoothGATTCharacteristic* characteristic,
      blink::WebBluetoothNotificationsCallbacks* callbacks,
      NotificationsRequestType type);
  // Pops the last requests and runs the next request in the queue.
  void PopNotificationRequestQueueAndProcessNext(int request_id);
  // Checks if there is more than one request in the queue i.e. if there
  // are other requests besides the one being processed.
  bool HasNotificationRequestResponsePending(
      const std::string& characteristic_instance_id);
  // Checks if there are any objects subscribed to the characteristic's
  // notifications.
  bool HasActiveNotificationSubscription(
      const std::string& characteristic_instance_id);
  // Adds the object to the set of subscribed objects to a characteristic's
  // notifications.
  void AddToActiveNotificationSubscriptions(
      const std::string& characteristic_instance_id,
      blink::WebBluetoothGATTCharacteristic* characteristic);
  // Removes the object from the set of subscribed object to the
  // characteristic's notifications. Returns true if the subscription
  // becomes inactive.
  bool RemoveFromActiveNotificationSubscriptions(
      const std::string& characteristic_instance_id,
      blink::WebBluetoothGATTCharacteristic* characteristic);

  // The following functions decide whether to resolve the request immediately
  // or send an IPC to change the subscription state.
  // You should never call these functions if PendingNotificationRequest
  // is true since there is currently another request being processed.
  void ResolveOrSendStartNotificationRequest(int request_id);
  void ResolveOrSendStopNotificationsRequest(int request_id);

  // Tells BluetoothDispatcherHost that we are no longer interested in
  // events for the characteristic.
  //
  // TODO(ortuno): We should unregister a characteristic once there are no
  // characteristic objects that have listeners attached.
  // For now, we call this function when an object gets destroyed. So if there
  // are two frames registered for notifications from the same characteristic
  // and one of the characteristic objects gets destroyed both will stop
  // receiving notifications.
  // https://crbug.com/541388
  void UnregisterCharacteristicObject(
      int frame_routing_id,
      const blink::WebString& characteristic_instance_id);

  // IPC Handlers, see definitions in bluetooth_messages.h.
  void OnRequestDeviceSuccess(int thread_id,
                              int request_id,
                              const BluetoothDevice& device);
  void OnRequestDeviceError(int thread_id,
                            int request_id,
                            blink::WebBluetoothError error);
  void OnGATTServerConnectSuccess(int thread_id, int request_id);
  void OnGATTServerConnectError(int thread_id,
                                int request_id,
                                blink::WebBluetoothError error);
  void OnGetPrimaryServiceSuccess(int thread_id,
                                  int request_id,
                                  const std::string& service_instance_id);
  void OnGetPrimaryServiceError(int thread_id,
                                int request_id,
                                blink::WebBluetoothError error);
  void OnGetCharacteristicSuccess(int thread_id,
                                  int request_id,
                                  const std::string& characteristic_instance_id,
                                  uint32_t characteristic_properties);
  void OnGetCharacteristicError(int thread_id,
                                int request_id,
                                blink::WebBluetoothError error);
  void OnReadValueSuccess(int thread_id,
                          int request_id,
                          const std::vector<uint8_t>& value);
  void OnReadValueError(int thread_id,
                        int request_id,
                        blink::WebBluetoothError error);
  void OnWriteValueSuccess(int thread_id, int request_id);
  void OnWriteValueError(int thread_id,
                         int request_id,
                         blink::WebBluetoothError error);
  void OnStartNotificationsSuccess(int thread_id, int request_id);
  void OnStartNotificationsError(int thread_id,
                                 int request_id,
                                 blink::WebBluetoothError error);
  void OnStopNotificationsSuccess(int thread_id, int request_id);
  void OnCharacteristicValueChanged(
      int thread_id,
      const std::string& characteristic_instance_id,
      const std::vector<uint8_t> value);

  scoped_refptr<ThreadSafeSender> thread_safe_sender_;

  // Map of characteristic_instance_id to a queue of Notification Requests' IDs.
  // See "Notifications Queueing Note" above.
  std::map<std::string, std::queue<int>> notification_requests_queues_;

  // Tracks device requests sent to browser to match replies with callbacks.
  // Owns callback objects.
  IDMap<blink::WebBluetoothRequestDeviceCallbacks, IDMapOwnPointer>
      pending_requests_;
  // Tracks requests to connect to a device.
  // Owns callback objects.
  IDMap<blink::WebBluetoothGATTServerConnectCallbacks, IDMapOwnPointer>
      pending_connect_requests_;
  // Tracks requests to get a primary service from a device.
  // Owns request objects.
  IDMap<BluetoothPrimaryServiceRequest, IDMapOwnPointer>
      pending_primary_service_requests_;
  // Tracks requests to get a characteristic from a service.
  IDMap<BluetoothCharacteristicRequest, IDMapOwnPointer>
      pending_characteristic_requests_;
  // Tracks requests to read from a characteristics.
  IDMap<blink::WebBluetoothReadValueCallbacks, IDMapOwnPointer>
      pending_read_value_requests_;
  IDMap<BluetoothWriteValueRequest, IDMapOwnPointer>
      pending_write_value_requests_;
  IDMap<BluetoothNotificationsRequest, IDMapOwnPointer>
      pending_notifications_requests_;

  // Map of characteristic_instance_id to a set of
  // WebBluetoothGATTCharacteristic pointers. Keeps track of which
  // objects are subscribed to notifications.
  std::map<std::string, std::set<blink::WebBluetoothGATTCharacteristic*>>
      active_notification_subscriptions_;

  // Map of characteristic_instance_ids to WebBluetoothGATTCharacteristics.
  // Keeps track of what characteristics have listeners.
  // TODO(ortuno): We are assuming that there exists a single frame per
  // dispatcher, so there could be at most one characteristic object per
  // characteristic_instance_id. Change to a set when we support multiple
  // frames.
  // http://crbug.com/541388
  std::map<std::string, blink::WebBluetoothGATTCharacteristic*>
      active_characteristics_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothDispatcher);
};

}  // namespace content

#endif  // CONTENT_CHILD_BLUETOOTH_BLUETOOTH_DISPATCHER_H_
