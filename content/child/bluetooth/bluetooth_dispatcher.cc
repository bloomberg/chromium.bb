// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/bluetooth/bluetooth_dispatcher.h"

#include "base/lazy_instance.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/thread_task_runner_handle.h"
#include "content/child/thread_safe_sender.h"
#include "content/common/bluetooth/bluetooth_messages.h"
#include "third_party/WebKit/public/platform/WebBluetoothDevice.h"
#include "third_party/WebKit/public/platform/WebBluetoothError.h"

using blink::WebBluetoothDevice;
using blink::WebBluetoothError;
using blink::WebBluetoothRequestDeviceCallbacks;
using blink::WebString;
using blink::WebVector;

namespace content {

namespace {

base::LazyInstance<base::ThreadLocalPointer<BluetoothDispatcher>>::Leaky
    g_dispatcher_tls = LAZY_INSTANCE_INITIALIZER;

BluetoothDispatcher* const kHasBeenDeleted =
    reinterpret_cast<BluetoothDispatcher*>(0x1);

int CurrentWorkerId() {
  return WorkerTaskRunner::Instance()->CurrentWorkerId();
}

WebBluetoothError::ErrorType WebBluetoothErrorFromBluetoothError(
    BluetoothError error_type) {
  switch (error_type) {
    case BluetoothError::NOT_FOUND:
      return WebBluetoothError::NotFoundError;
    case BluetoothError::SECURITY:
      return WebBluetoothError::SecurityError;
  }
  NOTIMPLEMENTED();
  return WebBluetoothError::NotFoundError;
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
  g_dispatcher_tls.Pointer()->Set(this);
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
    return g_dispatcher_tls.Pointer()->Get();

  BluetoothDispatcher* dispatcher = new BluetoothDispatcher(thread_safe_sender);
  if (WorkerTaskRunner::Instance()->CurrentWorkerId())
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
  IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  DCHECK(handled) << "Unhandled message:" << msg.type();
}

void BluetoothDispatcher::requestDevice(
    blink::WebBluetoothRequestDeviceCallbacks* callbacks) {
  int request_id = pending_requests_.Add(callbacks);
  Send(new BluetoothHostMsg_RequestDevice(CurrentWorkerId(), request_id));
}

void BluetoothDispatcher::SetBluetoothMockDataSetForTesting(
    const std::string& name) {
  Send(new BluetoothHostMsg_SetBluetoothMockDataSetForTesting(name));
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
      ->onSuccess(new WebBluetoothDevice(
          WebString::fromUTF8(device.instance_id), WebString(device.name),
          device.device_class, GetWebVendorIdSource(device.vendor_id_source),
          device.vendor_id, device.product_id, device.product_version,
          device.paired, device.connected, uuids));
  pending_requests_.Remove(request_id);
}

void BluetoothDispatcher::OnRequestDeviceError(int thread_id,
                                               int request_id,
                                               BluetoothError error_type) {
  DCHECK(pending_requests_.Lookup(request_id)) << request_id;
  pending_requests_.Lookup(request_id)
      ->onError(new WebBluetoothError(
          WebBluetoothErrorFromBluetoothError(error_type), ""));
  pending_requests_.Remove(request_id);
}

}  // namespace content
