// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_BLUETOOTH_BLUETOOTH_MESSAGE_FILTER_H_
#define CONTENT_CHILD_BLUETOOTH_BLUETOOTH_MESSAGE_FILTER_H_

#include "content/child/child_message_filter.h"

namespace base {
class MessageLoopProxy;
}

namespace content {

class ThreadSafeSender;

class BluetoothMessageFilter : public ChildMessageFilter {
 public:
  explicit BluetoothMessageFilter(ThreadSafeSender* thread_safe_sender);

 private:
  ~BluetoothMessageFilter() override;

  // ChildMessageFilter implementation:
  base::TaskRunner* OverrideTaskRunnerForMessage(
      const IPC::Message& msg) override;
  bool OnMessageReceived(const IPC::Message& msg) override;

  scoped_refptr<base::MessageLoopProxy> main_thread_loop_proxy_;
  scoped_refptr<ThreadSafeSender> thread_safe_sender_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothMessageFilter);
};

}  // namespace content

#endif  // CONTENT_CHILD_BLUETOOTH_BLUETOOTH_MESSAGE_FILTER_H_
