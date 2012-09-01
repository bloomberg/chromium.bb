// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/child_trace_message_filter.h"

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/message_loop.h"
#include "content/common/child_process.h"
#include "content/common/child_process_messages.h"

using base::debug::TraceLog;

ChildTraceMessageFilter::ChildTraceMessageFilter() : channel_(NULL) {}

void ChildTraceMessageFilter::OnFilterAdded(IPC::Channel* channel) {
  channel_ = channel;
  TraceLog::GetInstance()->SetNotificationCallback(
      base::Bind(&ChildTraceMessageFilter::OnTraceNotification, this));
  channel_->Send(new ChildProcessHostMsg_ChildSupportsTracing());
}

void ChildTraceMessageFilter::OnFilterRemoved() {
  TraceLog::GetInstance()->SetNotificationCallback(
      TraceLog::NotificationCallback());
}

bool ChildTraceMessageFilter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ChildTraceMessageFilter, message)
    IPC_MESSAGE_HANDLER(ChildProcessMsg_BeginTracing, OnBeginTracing)
    IPC_MESSAGE_HANDLER(ChildProcessMsg_EndTracing, OnEndTracing)
    IPC_MESSAGE_HANDLER(ChildProcessMsg_GetTraceBufferPercentFull,
                        OnGetTraceBufferPercentFull)
    IPC_MESSAGE_HANDLER(ChildProcessMsg_SetWatchEvent, OnSetWatchEvent)
    IPC_MESSAGE_HANDLER(ChildProcessMsg_CancelWatchEvent, OnCancelWatchEvent)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

ChildTraceMessageFilter::~ChildTraceMessageFilter() {}

void ChildTraceMessageFilter::OnBeginTracing(
    const std::vector<std::string>& included_categories,
    const std::vector<std::string>& excluded_categories) {
  TraceLog::GetInstance()->SetEnabled(included_categories,
                                      excluded_categories);
}

void ChildTraceMessageFilter::OnEndTracing() {
  TraceLog::GetInstance()->SetDisabled();

  // Flush will generate one or more callbacks to OnTraceDataCollected. It's
  // important that the last OnTraceDataCollected gets called before
  // EndTracingAck below. We are already on the IO thread, so the
  // OnTraceDataCollected calls will not be deferred.
  TraceLog::GetInstance()->Flush(
      base::Bind(&ChildTraceMessageFilter::OnTraceDataCollected, this));

  std::vector<std::string> categories;
  TraceLog::GetInstance()->GetKnownCategories(&categories);
  channel_->Send(new ChildProcessHostMsg_EndTracingAck(categories));
}

void ChildTraceMessageFilter::OnGetTraceBufferPercentFull() {
  float bpf = TraceLog::GetInstance()->GetBufferPercentFull();

  channel_->Send(new ChildProcessHostMsg_TraceBufferPercentFullReply(bpf));
}

void ChildTraceMessageFilter::OnSetWatchEvent(const std::string& category_name,
                                              const std::string& event_name) {
  TraceLog::GetInstance()->SetWatchEvent(category_name.c_str(),
                                         event_name.c_str());
}

void ChildTraceMessageFilter::OnCancelWatchEvent() {
  TraceLog::GetInstance()->CancelWatchEvent();
}

void ChildTraceMessageFilter::OnTraceDataCollected(
    const scoped_refptr<base::RefCountedString>& events_str_ptr) {
  if (MessageLoop::current() != ChildProcess::current()->io_message_loop()) {
    ChildProcess::current()->io_message_loop()->PostTask(FROM_HERE,
        base::Bind(&ChildTraceMessageFilter::OnTraceDataCollected, this,
                   events_str_ptr));
    return;
  }

  channel_->Send(new ChildProcessHostMsg_TraceDataCollected(
      events_str_ptr->data()));
}

void ChildTraceMessageFilter::OnTraceNotification(int notification) {
  if (MessageLoop::current() != ChildProcess::current()->io_message_loop()) {
    ChildProcess::current()->io_message_loop()->PostTask(FROM_HERE,
        base::Bind(&ChildTraceMessageFilter::OnTraceNotification, this,
                   notification));
    return;
  }

  channel_->Send(new ChildProcessHostMsg_TraceNotification(notification));
}
