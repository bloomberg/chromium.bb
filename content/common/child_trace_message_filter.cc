// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/child_trace_message_filter.h"

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/message_loop.h"
#include "content/common/child_process.h"
#include "content/common/child_process_messages.h"


ChildTraceMessageFilter::ChildTraceMessageFilter() : channel_(NULL) {
}

ChildTraceMessageFilter::~ChildTraceMessageFilter() {
}

void ChildTraceMessageFilter::OnFilterAdded(IPC::Channel* channel) {
  channel_ = channel;
  base::debug::TraceLog::GetInstance()->SetOutputCallback(
      base::Bind(&ChildTraceMessageFilter::OnTraceDataCollected, this));
  base::debug::TraceLog::GetInstance()->SetBufferFullCallback(
      base::Bind(&ChildTraceMessageFilter::OnTraceBufferFull, this));
  channel_->Send(new ChildProcessHostMsg_ChildSupportsTracing());
}

void ChildTraceMessageFilter::OnFilterRemoved() {
  base::debug::TraceLog::GetInstance()->SetOutputCallback(
      base::debug::TraceLog::OutputCallback());
  base::debug::TraceLog::GetInstance()->SetBufferFullCallback(
      base::debug::TraceLog::BufferFullCallback());
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

void ChildTraceMessageFilter::OnBeginTracing(
    const std::vector<std::string>& included_categories,
    const std::vector<std::string>& excluded_categories) {
  base::debug::TraceLog::GetInstance()->SetEnabled(included_categories,
                                                   excluded_categories);
}

void ChildTraceMessageFilter::OnEndTracing() {
  // SetDisabled may generate a callback to OnTraceDataCollected.
  // It's important that the last OnTraceDataCollected gets called before
  // EndTracingAck below.
  // We are already on the IO thread, so it is guaranteed that
  // OnTraceDataCollected is not deferred.
  base::debug::TraceLog::GetInstance()->SetDisabled();

  std::vector<std::string> categories;
  base::debug::TraceLog::GetInstance()->GetKnownCategories(&categories);
  channel_->Send(new ChildProcessHostMsg_EndTracingAck(categories));
}

void ChildTraceMessageFilter::OnGetTraceBufferPercentFull() {
  float bpf = base::debug::TraceLog::GetInstance()->GetBufferPercentFull();

  channel_->Send(new ChildProcessHostMsg_TraceBufferPercentFullReply(bpf));
}

void ChildTraceMessageFilter::OnTraceDataCollected(
    const scoped_refptr<base::debug::TraceLog::RefCountedString>&
        json_events_str_ptr) {
  if (MessageLoop::current() != ChildProcess::current()->io_message_loop()) {
    ChildProcess::current()->io_message_loop()->PostTask(FROM_HERE,
        NewRunnableMethod(this, &ChildTraceMessageFilter::OnTraceDataCollected,
                          json_events_str_ptr));
    return;
  }

  channel_->Send(new ChildProcessHostMsg_TraceDataCollected(
    json_events_str_ptr->data));
}

void ChildTraceMessageFilter::OnTraceBufferFull() {
  if (MessageLoop::current() != ChildProcess::current()->io_message_loop()) {
    ChildProcess::current()->io_message_loop()->PostTask(FROM_HERE,
        NewRunnableMethod(this, &ChildTraceMessageFilter::OnTraceBufferFull));
    return;
  }

  channel_->Send(new ChildProcessHostMsg_TraceBufferFull());
}

