// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TRACING_CHROME_TRACING_DELEGATE_H_
#define CHROME_BROWSER_TRACING_CHROME_TRACING_DELEGATE_H_

#include "content/public/browser/tracing_delegate.h"

class ChromeTracingDelegate : public content::TracingDelegate {
 public:
  scoped_ptr<content::TraceUploader> GetTraceUploader(
      net::URLRequestContextGetter* request_context) override;
};

#endif  // CHROME_BROWSER_TRACING_CHROME_TRACING_DELEGATE_H_
