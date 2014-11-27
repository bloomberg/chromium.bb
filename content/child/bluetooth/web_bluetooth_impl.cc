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

void WebBluetoothImpl::SetBluetoothMockDataSetForTesting(
    const std::string& name) {
  GetDispatcher()->SetBluetoothMockDataSetForTesting(name);
}

BluetoothDispatcher* WebBluetoothImpl::GetDispatcher() {
  return BluetoothDispatcher::GetOrCreateThreadSpecificInstance(
      thread_safe_sender_.get());
}

}  // namespace content
