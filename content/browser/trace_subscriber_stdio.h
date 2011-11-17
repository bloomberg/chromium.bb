// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_TRACE_SUBSCRIBER_STDIO_H_
#define CONTENT_BROWSER_TRACE_SUBSCRIBER_STDIO_H_
#pragma once

#include <string>

#include "base/file_util.h"
#include "content/browser/trace_controller.h"
#include "content/common/content_export.h"

// Stdio implementation of TraceSubscriber. Use this to write traces to a file.
class CONTENT_EXPORT TraceSubscriberStdio : public TraceSubscriber {
 public:
  TraceSubscriberStdio();
  // Creates or overwrites the specified file. Check IsValid() for success.
  explicit TraceSubscriberStdio(const FilePath& path);

  // Creates or overwrites the specified file. Returns true on success.
  bool OpenFile(const FilePath& path);
  // Finishes json output and closes file.
  void CloseFile();

  // Returns TRUE if we're currently writing data to a file.
  bool IsValid();

  // Implementation of TraceSubscriber
  virtual void OnEndTracingComplete() OVERRIDE;
  virtual void OnTraceDataCollected(const std::string& trace_fragment) OVERRIDE;

  virtual ~TraceSubscriberStdio();

 private:
  void Write(const std::string& output_str);

  FILE* file_;
  base::debug::TraceResultBuffer trace_buffer_;
};

#endif  // CONTENT_BROWSER_TRACE_SUBSCRIBER_STDIO_H_
