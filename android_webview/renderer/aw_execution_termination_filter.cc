// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/renderer/aw_execution_termination_filter.h"

#include <v8.h>

#include "android_webview/common/render_view_messages.h"
#include "base/message_loop/message_loop_proxy.h"

namespace {
const int kTerminationTimeoutInSeconds = 3;
}  // namespace

namespace android_webview {

AwExecutionTerminationFilter::AwExecutionTerminationFilter(
    const scoped_refptr<base::MessageLoopProxy>& io_message_loop,
    const scoped_refptr<base::MessageLoopProxy>& main_message_loop)
    : io_message_loop_(io_message_loop),
      main_message_loop_(main_message_loop),
      render_thread_isolate_(NULL) {
}

AwExecutionTerminationFilter::~AwExecutionTerminationFilter() {
}

void AwExecutionTerminationFilter::SetRenderThreadIsolate(
    v8::Isolate* isolate) {
  render_thread_isolate_ = isolate;
}

bool AwExecutionTerminationFilter::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(AwExecutionTerminationFilter, message)
    IPC_MESSAGE_HANDLER(AwViewMsg_CheckRenderThreadResponsiveness,
                        OnCheckRenderThreadResponsiveness)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void AwExecutionTerminationFilter::OnCheckRenderThreadResponsiveness() {
  termination_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromSeconds(kTerminationTimeoutInSeconds),
      base::Bind(&AwExecutionTerminationFilter::TerminateExecution, this));
  // Post a request to stop the timer via render thread's message loop
  // to ensure that render thread is responsive.
  main_message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&AwExecutionTerminationFilter::StopTimerOnMainThread,
                 this));
}

void AwExecutionTerminationFilter::StopTimerOnMainThread() {
  io_message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&AwExecutionTerminationFilter::StopTimer, this));
}

void AwExecutionTerminationFilter::StopTimer() {
  termination_timer_.Stop();
}

void AwExecutionTerminationFilter::TerminateExecution() {
  if (render_thread_isolate_) {
    LOG(WARNING) << "Trying to terminate JavaScript execution because "
        "renderer is unresponsive";
    v8::V8::TerminateExecution(render_thread_isolate_);
  }
}

}  // namespace android_webview
