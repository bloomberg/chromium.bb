// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_RENDERER_AW_EXECUTION_TERMINATION_FILTER_H_
#define ANDROID_WEBVIEW_RENDERER_AW_EXECUTION_TERMINATION_FILTER_H_

#include "base/memory/scoped_ptr.h"
#include "base/timer/timer.h"
#include "ipc/message_filter.h"

namespace base {
class MessageLoopProxy;
}

namespace v8 {
class Isolate;
}

namespace android_webview {

// The purpose of AwExecutionTerminationFilter is to attempt to terminate
// any JavaScript code that is stuck in a loop before doing a navigation
// originating from a Andoird WebView URL loading functions.
//
// This is how it works. AwExecutionTerminationFilter is created on render
// thread. It listens on IO thread for navigation requests coming from
// AwContents.loadUrl calls. On each such a request, it posts a delayed
// cancellable task on the IO thread's message loop and, at the same time, posts
// a cancellation task on the render thread's message loop. If render thread
// is not stuck, the cancellation task runs and cancels the delayed task.
// Otherwise, the delayed task runs and terminates execution of JS code
// from the IO thread.
class AwExecutionTerminationFilter : public IPC::MessageFilter {
 public:
  AwExecutionTerminationFilter(
      const scoped_refptr<base::MessageLoopProxy>& io_message_loop,
      const scoped_refptr<base::MessageLoopProxy>& main_message_loop);

  void SetRenderThreadIsolate(v8::Isolate* isolate);

 private:
  virtual ~AwExecutionTerminationFilter();

  // IPC::MessageFilter methods.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  void OnCheckRenderThreadResponsiveness();
  void StopTimerOnMainThread();
  void StopTimer();
  void TerminateExecution();

  const scoped_refptr<base::MessageLoopProxy> io_message_loop_;
  const scoped_refptr<base::MessageLoopProxy> main_message_loop_;

  v8::Isolate* render_thread_isolate_;
  base::OneShotTimer<AwExecutionTerminationFilter> termination_timer_;

  DISALLOW_COPY_AND_ASSIGN(AwExecutionTerminationFilter);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_RENDERER_AW_EXECUTION_TERMINATION_FILTER_H_
