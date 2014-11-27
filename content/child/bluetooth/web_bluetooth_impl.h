// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_BLUETOOTH_WEB_BLUETOOTH_IMPL_H_
#define CONTENT_CHILD_BLUETOOTH_WEB_BLUETOOTH_IMPL_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/WebBluetooth.h"

namespace content {

class BluetoothDispatcher;
class ThreadSafeSender;

// Implementation of blink::WebBluetooth. Passes calls through to the thread
// specific BluetoothDispatcher.
class CONTENT_EXPORT WebBluetoothImpl
    : NON_EXPORTED_BASE(public blink::WebBluetooth) {
 public:
  explicit WebBluetoothImpl(ThreadSafeSender* thread_safe_sender);
  ~WebBluetoothImpl();

  // blink::WebBluetooth interface:
  void requestDevice(
      blink::WebBluetoothRequestDeviceCallbacks* callbacks) override;

  // Testing interface:
  void SetBluetoothMockDataSetForTesting(const std::string& name);

 private:
  BluetoothDispatcher* GetDispatcher();

  scoped_refptr<ThreadSafeSender> thread_safe_sender_;

  DISALLOW_COPY_AND_ASSIGN(WebBluetoothImpl);
};

}  // namespace content

#endif  // CONTENT_CHILD_BLUETOOTH_WEB_BLUETOOTH_IMPL_H_
