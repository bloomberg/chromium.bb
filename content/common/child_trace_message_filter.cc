// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/child_trace_message_filter.h"

#include "base/message_loop.h"
#include "content/common/child_process.h"
#include "content/common/child_process_messages.h"
#include "gpu/common/gpu_trace_event.h"


ChildTraceMessageFilter::ChildTraceMessageFilter() : channel_(NULL) {
}

ChildTraceMessageFilter::~ChildTraceMessageFilter() {
  gpu::TraceLog::GetInstance()->SetOutputCallback(NULL);
  gpu::TraceLog::GetInstance()->SetBufferFullCallback(NULL);
}

void ChildTraceMessageFilter::OnFilterAdded(IPC::Channel* channel) {
  channel_ = channel;
  gpu::TraceLog::GetInstance()->SetOutputCallback(
      NewCallback(this, &ChildTraceMessageFilter::OnTraceDataCollected));
  gpu::TraceLog::GetInstance()->SetBufferFullCallback(
      NewCallback(this, &ChildTraceMessageFilter::OnTraceBufferFull));
}

bool ChildTraceMessageFilter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ChildTraceMessageFilter, message)
    IPC_MESSAGE_HANDLER(ChildProcessMsg_BeginTracing, OnBeginTracing)
    IPC_MESSAGE_HANDLER(ChildProcessMsg_EndTracing, OnEndTracing)
    IPC_MESSAGE_HANDLER(ChildProcessMsg_GetTraceBufferPercentFull,
                        OnGetTraceBufferPercentFull)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ChildTraceMessageFilter::OnBeginTracing() {
  gpu::TraceLog::GetInstance()->SetEnabled(true);
}

void ChildTraceMessageFilter::OnEndTracing() {
  // SetEnabled(false) may generate a callback to OnTraceDataCollected.
  // It's important that the last OnTraceDataCollected gets called before
  // EndTracingAck below.
  // We are already on the IO thread, so it is guaranteed that
  // OnTraceDataCollected is not deferred.
  gpu::TraceLog::GetInstance()->SetEnabled(false);

  channel_->Send(new ChildProcessHostMsg_EndTracingAck);
}

void ChildTraceMessageFilter::OnGetTraceBufferPercentFull() {
  float bpf = gpu::TraceLog::GetInstance()->GetBufferPercentFull();

  channel_->Send(new ChildProcessHostMsg_TraceBufferPercentFullReply(bpf));
}

void ChildTraceMessageFilter::OnTraceDataCollected(const std::string& data) {
  if (MessageLoop::current() != ChildProcess::current()->io_message_loop()) {
    ChildProcess::current()->io_message_loop()->PostTask(FROM_HERE,
        NewRunnableMethod(this, &ChildTraceMessageFilter::OnTraceDataCollected,
                          data));
    return;
  }

  channel_->Send(new ChildProcessHostMsg_TraceDataCollected(data));
}

void ChildTraceMessageFilter::OnTraceBufferFull() {
  if (MessageLoop::current() != ChildProcess::current()->io_message_loop()) {
    ChildProcess::current()->io_message_loop()->PostTask(FROM_HERE,
        NewRunnableMethod(this, &ChildTraceMessageFilter::OnTraceBufferFull));
    return;
  }

  channel_->Send(new ChildProcessHostMsg_TraceBufferFull());
}

