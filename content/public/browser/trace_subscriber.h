// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_TRACE_SUBSCRIBER_H_
#define CONTENT_PUBLIC_BROWSER_TRACE_SUBSCRIBER_H_

#include <set>

#include "base/memory/ref_counted_memory.h"

namespace content {

// Objects interested in receiving trace data derive from TraceSubscriber. All
// callbacks occur on the UI thread.
// See also: trace_message_filter.h
// See also: child_trace_message_filter.h
class TraceSubscriber {
 public:
  // Called once after TraceController::EndTracingAsync.
  virtual void OnEndTracingComplete() = 0;

  // Called 0 or more times between TraceController::BeginTracing and
  // OnEndTracingComplete. Use base::debug::TraceResultBuffer to convert one or
  // more trace fragments to JSON.
  virtual void OnTraceDataCollected(
      const scoped_refptr<base::RefCountedString>& trace_fragment) = 0;

  // Called once after TraceController::GetKnownCategoryGroupsAsync.
  virtual void OnKnownCategoriesCollected(
      const std::set<std::string>& known_categories) {}

  virtual void OnTraceBufferPercentFullReply(float percent_full) {}

  virtual void OnEventWatchNotification() {}

 protected:
  virtual ~TraceSubscriber() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_TRACE_SUBSCRIBER_H_
