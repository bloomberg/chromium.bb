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
#include "third_party/WebKit/public/platform/modules/bluetooth/WebBluetoothRemoteGATTService.h"
#include "third_party/WebKit/public/platform/modules/bluetooth/WebRequestDeviceOptions.h"

using blink::WebBluetoothDevice;
using blink::WebBluetoothError;
using blink::WebBluetoothRemoteGATTServerConnectCallbacks;
using blink::WebBluetoothRemoteGATTService;
using blink::WebBluetoothReadValueCallbacks;
using blink::WebBluetoothRequestDeviceCallbacks;
using blink::WebBluetoothScanFilter;
using blink::WebRequestDeviceOptions;
using blink::WebString;
using blink::WebVector;

namespace content {

namespace {

base::LazyInstance<base::ThreadLocalPointer<void>>::Leaky g_dispatcher_tls =
    LAZY_INSTANCE_INITIALIZER;

void* const kHasBeenDeleted = reinterpret_cast<void*>(0x1);

int CurrentWorkerId() {
  return WorkerThread::GetCurrentId();
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
          WebString::fromUTF8(device.id), WebString(device.name), uuids)));
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

}  // namespace content
