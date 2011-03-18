// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/trace_controller.h"

#include "base/task.h"
#include "content/browser/browser_message_filter.h"
#include "content/browser/trace_message_filter.h"
#include "content/common/child_process_messages.h"
#include "gpu/common/gpu_trace_event.h"


TraceController::TraceController() :
    pending_ack_count_(0),
    is_tracing_(false),
    subscriber_(NULL) {
  gpu::TraceLog::GetInstance()->SetOutputCallback(
      NewCallback(this, &TraceController::OnTraceDataCollected));
}

TraceController::~TraceController() {
  gpu::TraceLog::GetInstance()->SetOutputCallback(NULL);
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
  gpu::TraceLog::GetInstance()->SetEnabled(true);

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
  // Count myself (local trace) in pending_ack_count_, acked below.
  pending_ack_count_ = filters_.size() + 1;

  // Handle special case of zero child processes.
  if (pending_ack_count_ == 1) {
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

void TraceController::CancelSubscriber(TraceSubscriber* subscriber) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (subscriber == subscriber_) {
    subscriber_ = NULL;
    // End tracing if necessary.
    if (is_tracing_ && pending_ack_count_ == 0)
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

  if (--pending_ack_count_ == 0) {
    // All acks have been received.
    is_tracing_ = false;

    // Disable local trace. During this call, our OnTraceDataCollected will be
    // called with the last of the local trace data. Since we are on the UI
    // thread, the call to OnTraceDataCollected will be synchronous, so we can
    // immediately call OnEndTracingComplete below.
    gpu::TraceLog::GetInstance()->SetEnabled(false);

    // Trigger callback if one is set.
    if (subscriber_ != NULL) {
      subscriber_->OnEndTracingComplete();
      // Clear subscriber so that others can use TraceController.
      subscriber_ = NULL;
    }
  }

  if (pending_ack_count_ == 1) {
    // The last ack represents local trace, so we need to ack it now. Note that
    // this code only executes if there were child processes.
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        NewRunnableMethod(this, &TraceController::OnEndTracingAck));
  }
}

void TraceController::OnTraceDataCollected(const std::string& data) {
  // OnTraceDataCollected may be called from any browser thread, either by the
  // local event trace system or from child processes bia TraceMessageFilter.
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        NewRunnableMethod(this, &TraceController::OnTraceDataCollected, data));
    return;
  }

  if (subscriber_) {
    subscriber_->OnTraceDataCollected(data);
  }
}

