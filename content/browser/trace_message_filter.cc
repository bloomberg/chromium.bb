// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/trace_message_filter.h"

#include "content/browser/trace_controller.h"
#include "content/common/child_process_messages.h"


TraceMessageFilter::TraceMessageFilter() :
    is_awaiting_end_ack_(false),
    is_awaiting_bpf_ack_(false) {
}

TraceMessageFilter::~TraceMessageFilter() {
}

void TraceMessageFilter::OnFilterAdded(IPC::Channel* channel) {
  // Always on IO thread (BrowserMessageFilter guarantee).
  BrowserMessageFilter::OnFilterAdded(channel);

  TraceController::GetInstance()->AddFilter(this);
}

void TraceMessageFilter::OnChannelClosing() {
  // Always on IO thread (BrowserMessageFilter guarantee).
  BrowserMessageFilter::OnChannelClosing();

  if (is_awaiting_bpf_ack_)
    OnEndTracingAck();

  if (is_awaiting_end_ack_)
    OnTraceBufferPercentFullReply(0.0f);

  TraceController::GetInstance()->RemoveFilter(this);
}

bool TraceMessageFilter::OnMessageReceived(const IPC::Message& message,
                                           bool* message_was_ok) {
  // Always on IO thread (BrowserMessageFilter guarantee).
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(TraceMessageFilter, message, *message_was_ok)
    IPC_MESSAGE_HANDLER(ChildProcessHostMsg_EndTracingAck, OnEndTracingAck)
    IPC_MESSAGE_HANDLER(ChildProcessHostMsg_TraceDataCollected,
                        OnTraceDataCollected)
    IPC_MESSAGE_HANDLER(ChildProcessHostMsg_TraceBufferFull,
                        OnTraceBufferFull)
    IPC_MESSAGE_HANDLER(ChildProcessHostMsg_TraceBufferPercentFullReply,
                        OnTraceBufferPercentFullReply)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()
  return handled;
}

void TraceMessageFilter::SendBeginTracing() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  Send(new ChildProcessMsg_BeginTracing);
}

void TraceMessageFilter::SendEndTracing() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!is_awaiting_end_ack_);
  is_awaiting_end_ack_ = true;
  Send(new ChildProcessMsg_EndTracing);
}

void TraceMessageFilter::SendGetTraceBufferPercentFull() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!is_awaiting_bpf_ack_);
  is_awaiting_bpf_ack_ = true;
  Send(new ChildProcessMsg_GetTraceBufferPercentFull);
}

void TraceMessageFilter::OnEndTracingAck() {
  // is_awaiting_end_ack_ should always be true here, but check in case the
  // child process is compromised.
  if (is_awaiting_end_ack_) {
    is_awaiting_end_ack_ = false;
    TraceController::GetInstance()->OnEndTracingAck();
  }
}

void TraceMessageFilter::OnTraceDataCollected(const std::string& data) {
  TraceController::GetInstance()->OnTraceDataCollected(
      make_scoped_refptr(new base::debug::TraceLog::RefCountedString(data)));
}

void TraceMessageFilter::OnTraceBufferFull() {
  TraceController::GetInstance()->OnTraceBufferFull();
}

void TraceMessageFilter::OnTraceBufferPercentFullReply(float percent_full) {
  if (is_awaiting_bpf_ack_) {
    is_awaiting_bpf_ack_ = false;
    TraceController::GetInstance()->OnTraceBufferPercentFullReply(
        percent_full);
  }
}

