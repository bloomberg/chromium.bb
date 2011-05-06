// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/trace_controller.h"

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/task.h"
#include "content/browser/browser_message_filter.h"
#include "content/browser/trace_message_filter.h"
#include "content/common/child_process_messages.h"


TraceController::TraceController() :
    subscriber_(NULL),
    pending_end_ack_count_(0),
    pending_bpf_ack_count_(0),
    maximum_bpf_(0.0f),
    is_tracing_(false) {
  base::debug::TraceLog::GetInstance()->SetOutputCallback(
      base::Bind(&TraceController::OnTraceDataCollected,
                 base::Unretained(this)));
}

TraceController::~TraceController() {
  if (base::debug::TraceLog* trace_log = base::debug::TraceLog::GetInstance())
    trace_log->SetOutputCallback(base::debug::TraceLog::OutputCallback());
}

//static
TraceController* TraceController::GetInstance() {
  return Singleton<TraceController>::get();
}

bool TraceController::BeginTracing(TraceSubscriber* subscriber) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!can_begin_tracing() ||
      (subscriber_ != NULL && subscriber != subscriber_))
    return false;

  subscriber_ = subscriber;

  // Enable tracing
  is_tracing_ = true;
  base::debug::TraceLog::GetInstance()->SetEnabled(true);

  // Notify all child processes.
  for (FilterMap::iterator it = filters_.begin(); it != filters_.end(); ++it) {
    it->get()->SendBeginTracing();
  }

  return true;
}

bool TraceController::EndTracingAsync(TraceSubscriber* subscriber) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!can_end_tracing() || subscriber != subscriber_)
    return false;

  // There could be a case where there are no child processes and filters_
  // is empty. In that case we can immediately tell the subscriber that tracing
  // has ended. To avoid recursive calls back to the subscriber, we will just
  // use the existing asynchronous OnEndTracingAck code.
  // Count myself (local trace) in pending_end_ack_count_, acked below.
  pending_end_ack_count_ = filters_.size() + 1;

  // Handle special case of zero child processes.
  if (pending_end_ack_count_ == 1) {
    // Ack asynchronously now, because we don't have any children to wait for.
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        NewRunnableMethod(this, &TraceController::OnEndTracingAck));
  }

  // Notify all child processes.
  for (FilterMap::iterator it = filters_.begin(); it != filters_.end(); ++it) {
    it->get()->SendEndTracing();
  }

  return true;
}

bool TraceController::GetTraceBufferPercentFullAsync(
    TraceSubscriber* subscriber) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!can_get_buffer_percent_full() || subscriber != subscriber_)
    return false;

  maximum_bpf_ = 0.0f;
  pending_bpf_ack_count_ = filters_.size() + 1;

  // Handle special case of zero child processes.
  if (pending_bpf_ack_count_ == 1) {
    // Ack asynchronously now, because we don't have any children to wait for.
    float bpf = base::debug::TraceLog::GetInstance()->GetBufferPercentFull();
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        NewRunnableMethod(this,
                          &TraceController::OnTraceBufferPercentFullReply,
                          bpf));
  }

  // Message all child processes.
  for (FilterMap::iterator it = filters_.begin(); it != filters_.end(); ++it) {
    it->get()->SendGetTraceBufferPercentFull();
  }

  return true;
}

void TraceController::CancelSubscriber(TraceSubscriber* subscriber) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (subscriber == subscriber_) {
    subscriber_ = NULL;
    // End tracing if necessary.
    if (is_tracing_ && pending_end_ack_count_ == 0)
      EndTracingAsync(NULL);
  }
}

void TraceController::AddFilter(TraceMessageFilter* filter) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        NewRunnableMethod(this, &TraceController::AddFilter,
                          make_scoped_refptr(filter)));
    return;
  }

  filters_.insert(filter);
  if (is_tracing_enabled()) {
    filter->SendBeginTracing();
  }
}

void TraceController::RemoveFilter(TraceMessageFilter* filter) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        NewRunnableMethod(this, &TraceController::RemoveFilter,
                          make_scoped_refptr(filter)));
    return;
  }

  filters_.erase(filter);
}

void TraceController::OnEndTracingAck() {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        NewRunnableMethod(this, &TraceController::OnEndTracingAck));
    return;
  }

  if (pending_end_ack_count_ == 0)
    return;

  if (--pending_end_ack_count_ == 0) {
    // All acks have been received.
    is_tracing_ = false;

    // Disable local trace. During this call, our OnTraceDataCollected will be
    // called with the last of the local trace data. Since we are on the UI
    // thread, the call to OnTraceDataCollected will be synchronous, so we can
    // immediately call OnEndTracingComplete below.
    base::debug::TraceLog::GetInstance()->SetEnabled(false);

    // Trigger callback if one is set.
    if (subscriber_) {
      subscriber_->OnEndTracingComplete();
      // Clear subscriber so that others can use TraceController.
      subscriber_ = NULL;
    }
  }

  if (pending_end_ack_count_ == 1) {
    // The last ack represents local trace, so we need to ack it now. Note that
    // this code only executes if there were child processes.
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        NewRunnableMethod(this, &TraceController::OnEndTracingAck));
  }
}

void TraceController::OnTraceDataCollected(
    const scoped_refptr<base::debug::TraceLog::RefCountedString>&
        json_events_str_ptr) {
  // OnTraceDataCollected may be called from any browser thread, either by the
  // local event trace system or from child processes via TraceMessageFilter.
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        NewRunnableMethod(this,
                          &TraceController::OnTraceDataCollected,
                          json_events_str_ptr));
    return;
  }

  if (subscriber_)
    subscriber_->OnTraceDataCollected(json_events_str_ptr->data);
}

void TraceController::OnTraceBufferFull() {
  // OnTraceBufferFull may be called from any browser thread, either by the
  // local event trace system or from child processes via TraceMessageFilter.
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        NewRunnableMethod(this, &TraceController::OnTraceBufferFull));
    return;
  }

  // EndTracingAsync may return false if tracing is already in the process of
  // being ended. That is ok.
  EndTracingAsync(subscriber_);
}

void TraceController::OnTraceBufferPercentFullReply(float percent_full) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        NewRunnableMethod(this,
                          &TraceController::OnTraceBufferPercentFullReply,
                          percent_full));
    return;
  }

  if (pending_bpf_ack_count_ == 0)
    return;

  maximum_bpf_ = (maximum_bpf_ > percent_full)? maximum_bpf_ : percent_full;

  if (--pending_bpf_ack_count_ == 0) {
    // Trigger callback if one is set.
    if (subscriber_)
      subscriber_->OnTraceBufferPercentFullReply(maximum_bpf_);
  }

  if (pending_bpf_ack_count_ == 1) {
    // The last ack represents local trace, so we need to ack it now. Note that
    // this code only executes if there were child processes.
    float bpf = base::debug::TraceLog::GetInstance()->GetBufferPercentFull();
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        NewRunnableMethod(this,
                          &TraceController::OnTraceBufferPercentFullReply,
                          bpf));
  }
}

