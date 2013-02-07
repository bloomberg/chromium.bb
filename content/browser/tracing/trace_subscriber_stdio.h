// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_TRACING_TRACE_SUBSCRIBER_STDIO_H_
#define CONTENT_BROWSER_TRACING_TRACE_SUBSCRIBER_STDIO_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/file_util.h"
#include "content/public/browser/trace_subscriber.h"
#include "content/common/content_export.h"

namespace content {

class TraceSubscriberStdioImpl;

// Stdio implementation of TraceSubscriber. Use this to write traces to a file.
class CONTENT_EXPORT TraceSubscriberStdio
    : NON_EXPORTED_BASE(public TraceSubscriber) {
 public:
  // Creates or overwrites the specified file. Check IsValid() for success.
  explicit TraceSubscriberStdio(const base::FilePath& path);
  virtual ~TraceSubscriberStdio();

  // Implementation of TraceSubscriber
  virtual void OnEndTracingComplete() OVERRIDE;
  virtual void OnTraceDataCollected(
      const scoped_refptr<base::RefCountedString>& data_ptr) OVERRIDE;

 private:
  scoped_refptr<TraceSubscriberStdioImpl> impl_;
  DISALLOW_COPY_AND_ASSIGN(TraceSubscriberStdio);
};

}  // namespace content

#endif  // CONTENT_BROWSER_TRACING_TRACE_SUBSCRIBER_STDIO_H_
