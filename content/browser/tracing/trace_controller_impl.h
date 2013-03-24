// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_TRACING_TRACE_CONTROLLER_IMPL_H_
#define CONTENT_BROWSER_TRACING_TRACE_CONTROLLER_IMPL_H_

#include <set>
#include <string>
#include <vector>

#include "base/debug/trace_event.h"
#include "base/lazy_instance.h"
#include "content/public/browser/trace_controller.h"

class CommandLine;

namespace content {
class TraceMessageFilter;

class TraceControllerImpl : public TraceController {
 public:
  static TraceControllerImpl* GetInstance();

  // Called on the main thread of the browser process to initialize
  // startup tracing.
  void InitStartupTracing(const CommandLine& command_line);

  // Same as above, but specifies which categories to trace.
  // If both included_categories and excluded_categories are empty,
  //   all categories are traced.
  // Else if included_categories is non-empty, only those are traced.
  // Else if excluded_categories is non-empty, everything but those are traced.
  bool BeginTracing(TraceSubscriber* subscriber,
                    const std::vector<std::string>& included_categories,
                    const std::vector<std::string>& excluded_categories,
                    base::debug::TraceLog::Options options);

  // TraceController implementation:
  virtual bool BeginTracing(TraceSubscriber* subscriber,
                            const std::string& categories,
                            base::debug::TraceLog::Options options) OVERRIDE;
  virtual  bool EndTracingAsync(TraceSubscriber* subscriber) OVERRIDE;
  virtual bool GetTraceBufferPercentFullAsync(
      TraceSubscriber* subscriber) OVERRIDE;
  virtual bool SetWatchEvent(TraceSubscriber* subscriber,
                             const std::string& category_name,
                             const std::string& event_name) OVERRIDE;
  virtual bool CancelWatchEvent(TraceSubscriber* subscriber) OVERRIDE;
  virtual void CancelSubscriber(TraceSubscriber* subscriber) OVERRIDE;
  virtual bool GetKnownCategoriesAsync(TraceSubscriber* subscriber) OVERRIDE;

 private:
  typedef std::set<scoped_refptr<TraceMessageFilter> > FilterMap;

  friend struct base::DefaultLazyInstanceTraits<TraceControllerImpl>;
  friend class TraceMessageFilter;

  TraceControllerImpl();
  virtual ~TraceControllerImpl();

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

  bool can_begin_tracing(TraceSubscriber* subscriber) const {
    return !is_tracing_ &&
        (subscriber_ == NULL || subscriber == subscriber_);
  }

  // Methods for use by TraceMessageFilter.

  void AddFilter(TraceMessageFilter* filter);
  void RemoveFilter(TraceMessageFilter* filter);
  void OnTracingBegan(TraceSubscriber* subscriber);
  void OnEndTracingAck(const std::vector<std::string>& known_categories);
  void OnTraceDataCollected(
      const scoped_refptr<base::RefCountedString>& events_str_ptr);
  void OnTraceNotification(int notification);
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
  std::string watch_category_;
  std::string watch_name_;
  base::debug::TraceLog::Options trace_options_;

  DISALLOW_COPY_AND_ASSIGN(TraceControllerImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_TRACING_TRACE_CONTROLLER_IMPL_H_
