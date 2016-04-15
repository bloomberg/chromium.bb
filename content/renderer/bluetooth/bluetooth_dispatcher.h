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
class WebBluetoothRemoteGATTCharacteristic;
}

namespace IPC {
class Message;
}

struct BluetoothCharacteristicRequest;
struct BluetoothCharacteristicsRequest;
struct BluetoothPrimaryServiceRequest;

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
               blink::WebBluetoothRemoteGATTServerConnectCallbacks* callbacks);
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
  void getCharacteristics(
      int frame_routing_id,
      const blink::WebString& service_instance_id,
      const blink::WebString& characteristics_uuid,
      blink::WebBluetoothGetCharacteristicsCallbacks* callbacks);
  void readValue(int frame_routing_id,
                 const blink::WebString& characteristic_instance_id,
                 blink::WebBluetoothReadValueCallbacks* callbacks);

  // WorkerThread::Observer implementation.
  void WillStopCurrentWorkerThread() override;

 private:
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
  void OnGetCharacteristicsSuccess(
      int thread_id,
      int request_id,
      const std::vector<std::string>& characteristics_instance_ids,
      const std::vector<std::string>& characteristics_uuids,
      const std::vector<uint32_t>& characteristic_properties);
  void OnGetCharacteristicsError(int thread_id,
                                 int request_id,
                                 blink::WebBluetoothError error);
  void OnReadValueSuccess(int thread_id,
                          int request_id,
                          const std::vector<uint8_t>& value);
  void OnReadValueError(int thread_id,
                        int request_id,
                        blink::WebBluetoothError error);

  scoped_refptr<ThreadSafeSender> thread_safe_sender_;

  // Tracks device requests sent to browser to match replies with callbacks.
  // Owns callback objects.
  IDMap<blink::WebBluetoothRequestDeviceCallbacks, IDMapOwnPointer>
      pending_requests_;
  // Tracks requests to connect to a device.
  // Owns callback objects.
  IDMap<blink::WebBluetoothRemoteGATTServerConnectCallbacks, IDMapOwnPointer>
      pending_connect_requests_;
  // Tracks requests to get a primary service from a device.
  // Owns request objects.
  IDMap<BluetoothPrimaryServiceRequest, IDMapOwnPointer>
      pending_primary_service_requests_;
  // Tracks requests to get a characteristic from a service.
  IDMap<BluetoothCharacteristicRequest, IDMapOwnPointer>
      pending_characteristic_requests_;
  // Tracks requests to get characteristics from a service.
  IDMap<BluetoothCharacteristicsRequest, IDMapOwnPointer>
      pending_characteristics_requests_;
  // Tracks requests to read from a characteristics.
  IDMap<blink::WebBluetoothReadValueCallbacks, IDMapOwnPointer>
      pending_read_value_requests_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothDispatcher);
};

}  // namespace content

#endif  // CONTENT_CHILD_BLUETOOTH_BLUETOOTH_DISPATCHER_H_
