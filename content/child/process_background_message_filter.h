// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_PROCESS_BACKGROUND_MESSAGE_FILTER_H_
#define CONTENT_CHILD_PROCESS_BACKGROUND_MESSAGE_FILTER_H_

#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "ipc/message_filter.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace content {

// Receives process background messages from the browser process and processes
// them on the IO thread.
class ProcessBackgroundMessageFilter : public IPC::MessageFilter {
 public:
  ProcessBackgroundMessageFilter();

  // IPC::MessageFilter
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

 private:
  virtual ~ProcessBackgroundMessageFilter();

  void OnProcessBackgrounded(bool background);
  void SetTimerSlack(bool background);

  base::ThreadChecker thread_checker_;
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;
};

}  // namespace content

#endif  // CONTENT_CHILD_PROCESS_BACKGROUND_MESSAGE_FILTER_H_
