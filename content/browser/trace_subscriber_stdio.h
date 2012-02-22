// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_TRACE_SUBSCRIBER_STDIO_H_
#define CONTENT_BROWSER_TRACE_SUBSCRIBER_STDIO_H_
#pragma once

#include <string>

#include "base/file_util.h"
#include "content/browser/trace_controller.h"
#include "content/common/content_export.h"

namespace content {

class TraceSubscriberStdioImpl;

// Stdio implementation of TraceSubscriber. Use this to write traces to a file.
class CONTENT_EXPORT TraceSubscriberStdio : public TraceSubscriber {
 public:
  // Creates or overwrites the specified file. Check IsValid() for success.
  explicit TraceSubscriberStdio(const FilePath& path);
  virtual ~TraceSubscriberStdio();

  // Implementation of TraceSubscriber
  virtual void OnEndTracingComplete() OVERRIDE;
  virtual void OnTraceDataCollected(
      const scoped_refptr<base::RefCountedString>& data_ptr) OVERRIDE;

 private:
  scoped_refptr<TraceSubscriberStdioImpl> impl_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_TRACE_SUBSCRIBER_STDIO_H_
