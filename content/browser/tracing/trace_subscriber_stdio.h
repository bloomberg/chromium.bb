// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_TRACING_TRACE_SUBSCRIBER_STDIO_H_
#define CONTENT_BROWSER_TRACING_TRACE_SUBSCRIBER_STDIO_H_

#include <string>

#include "base/compiler_specific.h"
#include "content/public/browser/trace_subscriber.h"
#include "content/common/content_export.h"

namespace base {
class FilePath;
}

namespace content {

// Stdio implementation of TraceSubscriber. Use this to write traces to a file.
class CONTENT_EXPORT TraceSubscriberStdio
    : NON_EXPORTED_BASE(public TraceSubscriber) {
 public:
  enum FileType {
    // Output file as array, representing trace events:
    // [event1, event2, ...]
    FILE_TYPE_ARRAY,
    // Output file as property list with one or two items:
    // {traceEvents: [event1, event2, ...],
    //  systemTraceEvents: "event1\nevent2\n..." // optional}
    FILE_TYPE_PROPERTY_LIST
  };

  // has_system_trace indicates whether system trace events are expected.
  TraceSubscriberStdio(const base::FilePath& path,
                       FileType file_type,
                       bool has_system_trace);
  virtual ~TraceSubscriberStdio();

  // Implementation of TraceSubscriber
  virtual void OnEndTracingComplete() OVERRIDE;
  virtual void OnTraceDataCollected(
      const scoped_refptr<base::RefCountedString>& data_ptr) OVERRIDE;

  // To be used as callback to DebugDaemonClient::RequestStopSystemTracing().
  virtual void OnEndSystemTracing(
      const scoped_refptr<base::RefCountedString>& events_str_ptr);

 private:
  class TraceSubscriberStdioWorker;
  scoped_refptr<TraceSubscriberStdioWorker> worker_;
  DISALLOW_COPY_AND_ASSIGN(TraceSubscriberStdio);
};

}  // namespace content

#endif  // CONTENT_BROWSER_TRACING_TRACE_SUBSCRIBER_STDIO_H_
