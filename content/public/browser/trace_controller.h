// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_TRACE_CONTROLLER_H_
#define CONTENT_PUBLIC_BROWSER_TRACE_CONTROLLER_H_

#include "content/common/content_export.h"

namespace content {

class TraceSubscriber;

// TraceController is used on the browser processes to enable/disable
// trace status and collect trace data. Only the browser UI thread is allowed
// to interact with the TraceController object. All calls on the TraceSubscriber
// happen on the UI thread.
class TraceController {
 public:
  CONTENT_EXPORT static TraceController* GetInstance();

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
  virtual bool BeginTracing(TraceSubscriber* subscriber) = 0;

  // |categories| is a comma-delimited list of category wildcards.
  // A category can have an optional '-' prefix to make it an excluded category.
  // All the same rules apply above, so for example, having both included and
  // excluded categories in the same list would not be supported.
  //
  // Example: BeginTracing("test_MyTest*");
  // Example: BeginTracing("test_MyTest*,test_OtherStuff");
  // Example: BeginTracing("-excluded_category1,-excluded_category2");
  virtual bool BeginTracing(TraceSubscriber* subscriber,
                            const std::string& categories) = 0;

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
  virtual bool EndTracingAsync(TraceSubscriber* subscriber) = 0;

  // Get the maximum across processes of trace buffer percent full state.
  // When the TraceBufferPercentFull value is determined,
  // subscriber->OnTraceBufferPercentFullReply is called.
  // When any child process reaches 100% full, the TraceController will end
  // tracing, and call TraceSubscriber::OnEndTracingComplete.
  // GetTraceBufferPercentFullAsync fails in the following conditions:
  //   trace is ending or disabled;
  //   a previous call to GetTraceBufferPercentFullAsync is pending; or
  //   the caller is not the current subscriber.
  virtual bool GetTraceBufferPercentFullAsync(TraceSubscriber* subscriber) = 0;

  // Cancel the subscriber so that it will not be called when EndTracingAsync is
  // acked by all child processes. This will also call EndTracingAsync
  // internally if necessary.
  // Safe to call even if caller is not the current subscriber.
  virtual void CancelSubscriber(TraceSubscriber* subscriber) = 0;

 protected:
  virtual ~TraceController() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_TRACE_CONTROLLER_H_

