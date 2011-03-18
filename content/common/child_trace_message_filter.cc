// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/child_trace_message_filter.h"

#include "content/common/child_process_messages.h"
#include "gpu/common/gpu_trace_event.h"


ChildTraceMessageFilter::ChildTraceMessageFilter() : channel_(NULL) {
  gpu::TraceLog::GetInstance()->SetOutputCallback(
      NewCallback(this, &ChildTraceMessageFilter::OnTraceDataCollected));
}

ChildTraceMessageFilter::~ChildTraceMessageFilter() {
  gpu::TraceLog::GetInstance()->SetOutputCallback(NULL);
}

void ChildTraceMessageFilter::OnFilterAdded(IPC::Channel* channel) {
  channel_ = channel;
}

bool ChildTraceMessageFilter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ChildTraceMessageFilter, message)
    IPC_MESSAGE_HANDLER(ChildProcessMsg_BeginTracing, OnBeginTracing)
    IPC_MESSAGE_HANDLER(ChildProcessMsg_EndTracing, OnEndTracing)
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
  gpu::TraceLog::GetInstance()->SetEnabled(false);

  channel_->Send(new ChildProcessHostMsg_EndTracingAck);
}

void ChildTraceMessageFilter::OnTraceDataCollected(const std::string& data) {
  // May be called before channel_ is set if other code is enabling the
  // trace subsystem earlier.
  channel_->Send(new ChildProcessHostMsg_TraceDataCollected(data));
}

