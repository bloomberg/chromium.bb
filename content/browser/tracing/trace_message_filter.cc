// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tracing/trace_message_filter.h"

#include "components/tracing/tracing_messages.h"
#include "content/browser/tracing/trace_controller_impl.h"

namespace content {

TraceMessageFilter::TraceMessageFilter() :
    has_child_(false),
    is_awaiting_end_ack_(false),
    is_awaiting_buffer_percent_full_ack_(false) {
}

void TraceMessageFilter::OnFilterAdded(IPC::Channel* channel) {
  // Always on IO thread (BrowserMessageFilter guarantee).
  BrowserMessageFilter::OnFilterAdded(channel);
}

void TraceMessageFilter::OnChannelClosing() {
  // Always on IO thread (BrowserMessageFilter guarantee).
  BrowserMessageFilter::OnChannelClosing();

  if (has_child_) {
    if (is_awaiting_end_ack_)
      OnEndTracingAck(std::vector<std::string>());

    if (is_awaiting_buffer_percent_full_ack_)
      OnTraceBufferPercentFullReply(0.0f);

    TraceControllerImpl::GetInstance()->RemoveFilter(this);
  }
}

bool TraceMessageFilter::OnMessageReceived(const IPC::Message& message,
                                           bool* message_was_ok) {
  // Always on IO thread (BrowserMessageFilter guarantee).
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(TraceMessageFilter, message, *message_was_ok)
    IPC_MESSAGE_HANDLER(TracingHostMsg_ChildSupportsTracing,
                        OnChildSupportsTracing)
    IPC_MESSAGE_HANDLER(TracingHostMsg_EndTracingAck, OnEndTracingAck)
    IPC_MESSAGE_HANDLER(TracingHostMsg_TraceDataCollected,
                        OnTraceDataCollected)
    IPC_MESSAGE_HANDLER(TracingHostMsg_TraceNotification,
                        OnTraceNotification)
    IPC_MESSAGE_HANDLER(TracingHostMsg_TraceBufferPercentFullReply,
                        OnTraceBufferPercentFullReply)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()
  return handled;
}

void TraceMessageFilter::SendBeginTracing(
    const std::vector<std::string>& included_categories,
    const std::vector<std::string>& excluded_categories,
    base::debug::TraceLog::Options options) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  Send(new TracingMsg_BeginTracing(included_categories, excluded_categories,
                                   base::TimeTicks::NowFromSystemTraceTime(),
                                   options));
}

void TraceMessageFilter::SendEndTracing() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!is_awaiting_end_ack_);
  is_awaiting_end_ack_ = true;
  Send(new TracingMsg_EndTracing);
}

void TraceMessageFilter::SendGetTraceBufferPercentFull() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!is_awaiting_buffer_percent_full_ack_);
  is_awaiting_buffer_percent_full_ack_ = true;
  Send(new TracingMsg_GetTraceBufferPercentFull);
}

void TraceMessageFilter::SendSetWatchEvent(const std::string& category_name,
                                           const std::string& event_name) {
  Send(new TracingMsg_SetWatchEvent(category_name, event_name));
}

void TraceMessageFilter::SendCancelWatchEvent() {
  Send(new TracingMsg_CancelWatchEvent);
}

TraceMessageFilter::~TraceMessageFilter() {}

void TraceMessageFilter::OnChildSupportsTracing() {
  has_child_ = true;
  TraceControllerImpl::GetInstance()->AddFilter(this);
}

void TraceMessageFilter::OnEndTracingAck(
    const std::vector<std::string>& known_categories) {
  // is_awaiting_end_ack_ should always be true here, but check in case the
  // child process is compromised.
  if (is_awaiting_end_ack_) {
    is_awaiting_end_ack_ = false;
    TraceControllerImpl::GetInstance()->OnEndTracingAck(known_categories);
  } else {
    NOTREACHED();
  }
}

void TraceMessageFilter::OnTraceDataCollected(const std::string& data) {
  scoped_refptr<base::RefCountedString> data_ptr(new base::RefCountedString());
  data_ptr->data() = data;
  TraceControllerImpl::GetInstance()->OnTraceDataCollected(data_ptr);
}

void TraceMessageFilter::OnTraceNotification(int notification) {
  TraceControllerImpl::GetInstance()->OnTraceNotification(notification);
}

void TraceMessageFilter::OnTraceBufferPercentFullReply(float percent_full) {
  if (is_awaiting_buffer_percent_full_ack_) {
    is_awaiting_buffer_percent_full_ack_ = false;
    TraceControllerImpl::GetInstance()->OnTraceBufferPercentFullReply(
        percent_full);
  } else {
    NOTREACHED();
  }
}

}  // namespace content
