// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_TRACE_CONTROLLER_H_
#define CONTENT_BROWSER_TRACE_CONTROLLER_H_

#include <set>
#include <string>
#include <vector>

#include "base/debug/trace_event.h"
#include "base/memory/ref_counted.h"
#include "base/memory/singleton.h"
#include "base/task.h"
#include "content/common/content_export.h"

class TraceMessageFilter;

// Objects interested in receiving trace data derive from TraceSubscriber.
// See also: trace_message_filter.h
// See also: child_trace_message_filter.h
class TraceSubscriber {
 public:
  // Called once after TraceController::EndTracingAsync.
  virtual void OnEndTracingComplete() = 0;
  // Called 0 or more times between TraceController::BeginTracing and
  // OnEndTracingComplete.
  virtual void OnTraceDataCollected(const std::string& json_events) = 0;
  // Called once after TraceController::GetKnownCategoriesAsync.
  virtual void OnKnownCategoriesCollected(
      const std::set<std::string>& known_categories) {}
  virtual void OnTraceBufferPercentFullReply(float percent_full) {}
};

// TraceController is used on the browser processes to enable/disable
// trace status and collect trace data. Only the browser UI thread is allowed
// to interact with the TraceController object. All calls on the TraceSubscriber
// happen on the UI thread.
class CONTENT_EXPORT TraceController {
 public:
  static TraceController* GetInstance();

  // Get set of known categories. This can change as new code paths are reached.
  // If true is returned, subscriber->OnKnownCategoriesCollected will be called
  // when once the categories are retrieved from child processes.
  bool GetKnownCategoriesAsync(TraceSubscriber* subscriber);

  // Called by browser process to start tracing events on all processes.
  //
  // Currently only one subscriber is allowed at a time.
  // Tracing begins immediately locally, and asynchronously on child processes
  // as soon as they receive the BeginTracing request.
  // By default, all categories are traced except those matching "test_*".
  //
  // If BeginTracing was already called previously,
  //   or if an EndTracingAsync is pending,
  //   or if another subscriber is tracing,
  //   BeginTracing will return false meaning it failed.
  bool BeginTracing(TraceSubscriber* subscriber);

  // Same as above, but specifies which categories to trace.
  // If both included_categories and excluded_categories are empty,
  //   all categories are traced.
  // Else if included_categories is non-empty, only those are traced.
  // Else if excluded_categories is non-empty, everything but those are traced.
  bool BeginTracing(TraceSubscriber* subscriber,
                    const std::vector<std::string>& included_categories,
                    const std::vector<std::string>& excluded_categories);

  // Called by browser process to stop tracing events on all processes.
  //
  // Child processes typically are caching trace data and only rarely flush
  // and send trace data back to the browser process. That is because it may be
  // an expensive operation to send the trace data over IPC, and we would like
  // to avoid much runtime overhead of tracing. So, to end tracing, we must
  // asynchronously ask all child processes to flush any pending trace data.
  //
  // Once all child processes have acked the EndTracing request,
  // TraceSubscriber will be called with OnEndTracingComplete.
  //
  // If a previous call to EndTracingAsync is already pending,
  //   or if another subscriber is tracing,
  //   EndTracingAsync will return false meaning it failed.
  bool EndTracingAsync(TraceSubscriber* subscriber);

  // Get the maximum across processes of trace buffer percent full state.
  // When the TraceBufferPercentFull value is determined,
  // subscriber->OnTraceBufferPercentFullReply is called.
  // When any child process reaches 100% full, the TraceController will end
  // tracing, and call TraceSubscriber::OnEndTracingComplete.
  // GetTraceBufferPercentFullAsync fails in the following conditions:
  //   trace is ending or disabled;
  //   a previous call to GetTraceBufferPercentFullAsync is pending; or
  //   the caller is not the current subscriber.
  bool GetTraceBufferPercentFullAsync(TraceSubscriber* subscriber);

  // Cancel the subscriber so that it will not be called when EndTracingAsync is
  // acked by all child processes. This will also call EndTracingAsync
  // internally if necessary.
  // Safe to call even if caller is not the current subscriber.
  void CancelSubscriber(TraceSubscriber* subscriber);

 private:
  typedef std::set<scoped_refptr<TraceMessageFilter> > FilterMap;

  friend struct DefaultSingletonTraits<TraceController>;
  friend class TraceMessageFilter;

  TraceController();
  ~TraceController();

  bool is_tracing_enabled() const {
    return can_end_tracing();
  }

  bool can_end_tracing() const {
    return is_tracing_ && pending_end_ack_count_ == 0;
  }

  // Can get Buffer Percent Full
  bool can_get_buffer_percent_full() const {
    return is_tracing_ &&
        pending_end_ack_count_ == 0 &&
        pending_bpf_ack_count_ == 0;
  }

  bool can_begin_tracing() const { return !is_tracing_; }

  // Methods for use by TraceMessageFilter.

  // Passing as scoped_refptr so that the method can be run asynchronously as
  // a task safely (otherwise the TraceMessageFilter could be destructed).
  void AddFilter(TraceMessageFilter* filter);
  void RemoveFilter(TraceMessageFilter* filter);
  void OnEndTracingAck(const std::vector<std::string>& known_categories);
  void OnTraceDataCollected(
      const scoped_refptr<base::debug::TraceLog::RefCountedString>&
          json_events_str_ptr);
  void OnTraceBufferFull();
  void OnTraceBufferPercentFullReply(float percent_full);

  FilterMap filters_;
  TraceSubscriber* subscriber_;
  // Pending acks for EndTracingAsync:
  int pending_end_ack_count_;
  // Pending acks for GetTraceBufferPercentFullAsync:
  int pending_bpf_ack_count_;
  float maximum_bpf_;
  bool is_tracing_;
  bool is_get_categories_;
  std::set<std::string> known_categories_;
  std::vector<std::string> included_categories_;
  std::vector<std::string> excluded_categories_;

  DISALLOW_COPY_AND_ASSIGN(TraceController);
};

DISABLE_RUNNABLE_METHOD_REFCOUNT(TraceController);

#endif  // CONTENT_BROWSER_TRACE_CONTROLLER_H_

