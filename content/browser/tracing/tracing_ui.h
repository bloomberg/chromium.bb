// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_TRACING_TRACING_UI_H_
#define CONTENT_BROWSER_TRACING_TRACING_UI_H_

#include <map>
#include <string>

#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_ui_controller.h"

namespace content {

class TraceUploader;
class TracingDelegate;

// The C++ back-end for the chrome://tracing webui page.
class CONTENT_EXPORT TracingUI : public WebUIController {
 public:
  explicit TracingUI(WebUI* web_ui);
  ~TracingUI() override;
  void OnMonitoringStateChanged(bool is_monitoring);
  void DoUpload(const base::ListValue* args);
  void OnTraceUploadProgress(int64 current, int64 total);
  void OnTraceUploadComplete(bool success, const std::string& feedback);

 private:
  scoped_ptr<TracingDelegate> delegate_;
  scoped_ptr<TraceUploader> trace_uploader_;
  base::WeakPtrFactory<TracingUI> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(TracingUI);
};

}  // namespace content

#endif  // CONTENT_BROWSER_TRACING_TRACING_UI_H_
