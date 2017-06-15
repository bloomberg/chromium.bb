// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tracing/trace_message_filter.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "components/tracing/common/tracing_messages.h"
#include "content/browser/tracing/background_tracing_manager_impl.h"
#include "content/browser/tracing/tracing_controller_impl.h"
#include "content/common/child_process_host_impl.h"
#include "content/public/browser/browser_thread.h"

namespace content {

TraceMessageFilter::TraceMessageFilter(int child_process_id)
    : BrowserMessageFilter(TracingMsgStart),
      has_child_(false),
      tracing_process_id_(
          ChildProcessHostImpl::ChildProcessUniqueIdToTracingProcessId(
              child_process_id)),
      is_awaiting_end_ack_(false),
      is_awaiting_buffer_percent_full_ack_(false) {
}

TraceMessageFilter::~TraceMessageFilter() {}

void TraceMessageFilter::OnChannelClosing() {
  if (has_child_) {
    if (is_awaiting_end_ack_)
      OnEndTracingAck(std::vector<std::string>());

    if (is_awaiting_buffer_percent_full_ack_)
      OnTraceLogStatusReply(base::trace_event::TraceLogStatus());

    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&TraceMessageFilter::Unregister, base::RetainedRef(this)));
  }
}

bool TraceMessageFilter::OnMessageReceived(const IPC::Message& message) {
  // Always on IO thread (BrowserMessageFilter guarantee).
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(TraceMessageFilter, message)
    IPC_MESSAGE_HANDLER(TracingHostMsg_ChildSupportsTracing,
                        OnChildSupportsTracing)
    IPC_MESSAGE_HANDLER(TracingHostMsg_EndTracingAck, OnEndTracingAck)
    IPC_MESSAGE_HANDLER(TracingHostMsg_TraceDataCollected,
                        OnTraceDataCollected)
    IPC_MESSAGE_HANDLER(TracingHostMsg_TraceLogStatusReply,
                        OnTraceLogStatusReply)
    IPC_MESSAGE_HANDLER(TracingHostMsg_TriggerBackgroundTrace,
                        OnTriggerBackgroundTrace)
    IPC_MESSAGE_HANDLER(TracingHostMsg_AbortBackgroundTrace,
                        OnAbortBackgroundTrace)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void TraceMessageFilter::SendBeginTracing(
      const base::trace_event::TraceConfig& trace_config) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  Send(new TracingMsg_BeginTracing(
      trace_config.ToString(), base::TimeTicks::Now(), tracing_process_id_));
}

void TraceMessageFilter::SendEndTracing() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!is_awaiting_end_ack_);
  is_awaiting_end_ack_ = true;
  Send(new TracingMsg_EndTracing);
}

void TraceMessageFilter::SendCancelTracing() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!is_awaiting_end_ack_);
  is_awaiting_end_ack_ = true;
  Send(new TracingMsg_CancelTracing);
}

void TraceMessageFilter::SendGetTraceLogStatus() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!is_awaiting_buffer_percent_full_ack_);
  is_awaiting_buffer_percent_full_ack_ = true;
  Send(new TracingMsg_GetTraceLogStatus);
}

void TraceMessageFilter::OnChildSupportsTracing() {
  has_child_ = true;
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&TraceMessageFilter::Register, base::RetainedRef(this)));
}

void TraceMessageFilter::OnEndTracingAck(
    const std::vector<std::string>& known_categories) {
  // is_awaiting_end_ack_ should always be true here, but check in case the
  // child process is compromised.
  if (is_awaiting_end_ack_) {
    is_awaiting_end_ack_ = false;
    TracingControllerImpl::GetInstance()->OnStopTracingAcked(
        this, known_categories);
  } else {
    NOTREACHED();
  }
}

void TraceMessageFilter::Register() {
  BackgroundTracingManagerImpl::GetInstance()->AddTraceMessageFilter(this);
  TracingControllerImpl::GetInstance()->AddTraceMessageFilter(this);
}

void TraceMessageFilter::Unregister() {
  BackgroundTracingManagerImpl::GetInstance()->RemoveTraceMessageFilter(this);
  TracingControllerImpl::GetInstance()->RemoveTraceMessageFilter(this);
}

void TraceMessageFilter::OnTraceDataCollected(const std::string& data) {
  scoped_refptr<base::RefCountedString> data_ptr(new base::RefCountedString());
  data_ptr->data() = data;
  TracingControllerImpl::GetInstance()->OnTraceDataCollected(data_ptr);
}

void TraceMessageFilter::OnTraceLogStatusReply(
    const base::trace_event::TraceLogStatus& status) {
  if (is_awaiting_buffer_percent_full_ack_) {
    is_awaiting_buffer_percent_full_ack_ = false;
    TracingControllerImpl::GetInstance()->OnTraceLogStatusReply(this, status);
  } else {
    NOTREACHED();
  }
}

void TraceMessageFilter::OnTriggerBackgroundTrace(const std::string& name) {
  BackgroundTracingManagerImpl::GetInstance()->OnHistogramTrigger(name);
}

void TraceMessageFilter::OnAbortBackgroundTrace() {
  BackgroundTracingManagerImpl::GetInstance()->AbortScenario();
}

}  // namespace content
