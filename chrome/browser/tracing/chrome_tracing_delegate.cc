// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tracing/chrome_tracing_delegate.h"
#include "chrome/browser/tracing/crash_service_uploader.h"

scoped_ptr<content::TraceUploader> ChromeTracingDelegate::GetTraceUploader(
    net::URLRequestContextGetter* request_context) {
  return scoped_ptr<content::TraceUploader>(
      new TraceCrashServiceUploader(request_context));
}
