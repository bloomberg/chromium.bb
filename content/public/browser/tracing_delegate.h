// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_TRACING_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_TRACING_DELEGATE_H_

#include "base/memory/scoped_ptr.h"

namespace net {
class URLRequestContextGetter;
}

namespace content {
class TraceUploader;

// This can be implemented by the embedder to provide functionality for the
// about://tracing WebUI.
class TracingDelegate {
 public:
  virtual ~TracingDelegate() {}

  // Provide trace uploading functionality; see trace_uploader.h.
  virtual scoped_ptr<TraceUploader> GetTraceUploader(
      net::URLRequestContextGetter* request_context) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_TRACING_DELEGATE_H_
