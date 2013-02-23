// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tracing/child_trace_message_filter.h"

#include "base/debug/trace_event.h"
#include "base/message_loop_proxy.h"
#include "components/tracing/tracing_messages.h"

using base::debug::TraceLog;

namespace components {

ChildTraceMessageFilter::ChildTraceMessageFilter(
    base::MessageLoopProxy* ipc_message_loop)
    : channel_(NULL),
      ipc_message_loop_(ipc_message_loop) {}

void ChildTraceMessageFilter::OnFilterAdded(IPC::Channel* channel) {
  channel_ = channel;
  TraceLog::GetInstance()->SetNotificationCallback(
      base::Bind(&ChildTraceMessageFilter::OnTraceNotification, this));
  channel_->Send(new TracingHostMsg_ChildSupportsTracing());
}

void ChildTraceMessageFilter::OnFilterRemoved() {
  TraceLog::GetInstance()->SetNotificationCallback(
      TraceLog::NotificationCallback());
}

bool ChildTraceMessageFilter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ChildTraceMessageFilter, message)
    IPC_MESSAGE_HANDLER(TracingMsg_BeginTracing, OnBeginTracing)
    IPC_MESSAGE_HANDLER(TracingMsg_EndTracing, OnEndTracing)
    IPC_MESSAGE_HANDLER(TracingMsg_GetTraceBufferPercentFull,
                        OnGetTraceBufferPercentFull)
    IPC_MESSAGE_HANDLER(TracingMsg_SetWatchEvent, OnSetWatchEvent)
    IPC_MESSAGE_HANDLER(TracingMsg_CancelWatchEvent, OnCancelWatchEvent)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

ChildTraceMessageFilter::~ChildTraceMessageFilter() {}

void ChildTraceMessageFilter::OnBeginTracing(
    const std::vector<std::string>& included_categories,
    const std::vector<std::string>& excluded_categories,
    base::TimeTicks browser_time,
    int options) {
#if defined(__native_client__)
  // NaCl and system times are offset by a bit, so subtract some time from
  // the captured timestamps. The value might be off by a bit due to messaging
  // latency.
  base::TimeDelta time_offset = base::TimeTicks::NowFromSystemTraceTime() -
      browser_time;
  TraceLog::GetInstance()->SetTimeOffset(time_offset);
#endif
  TraceLog::GetInstance()->SetEnabled(
      included_categories,
      excluded_categories,
      static_cast<base::debug::TraceLog::Options>(options));
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
  channel_->Send(new TracingHostMsg_EndTracingAck(categories));
}

void ChildTraceMessageFilter::OnGetTraceBufferPercentFull() {
  float bpf = TraceLog::GetInstance()->GetBufferPercentFull();

  channel_->Send(new TracingHostMsg_TraceBufferPercentFullReply(bpf));
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
  if (!ipc_message_loop_->BelongsToCurrentThread()) {
    ipc_message_loop_->PostTask(FROM_HERE,
        base::Bind(&ChildTraceMessageFilter::OnTraceDataCollected, this,
                   events_str_ptr));
    return;
  }
  channel_->Send(new TracingHostMsg_TraceDataCollected(
      events_str_ptr->data()));
}

void ChildTraceMessageFilter::OnTraceNotification(int notification) {
  if (!ipc_message_loop_->BelongsToCurrentThread()) {
    ipc_message_loop_->PostTask(FROM_HERE,
        base::Bind(&ChildTraceMessageFilter::OnTraceNotification, this,
                   notification));
    return;
  }
  channel_->Send(new TracingHostMsg_TraceNotification(notification));
}

}  // namespace components
