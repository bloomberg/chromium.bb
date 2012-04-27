// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_AUTOMATION_AUTOMATION_RENDERER_HELPER_H_
#define CHROME_RENDERER_AUTOMATION_AUTOMATION_RENDERER_HELPER_H_
#pragma once

#include <string>
#include <vector>

#include "content/public/renderer/render_view_observer.h"
#include "content/public/renderer/render_view_observer_tracker.h"

namespace WebKit {
class WebFrame;
class WebURL;
class WebView;
}

// Filters automation/testing messages sent to a |RenderView| and sends
// automation/testing messages to the browser.
class AutomationRendererHelper : public content::RenderViewObserver {
 public:
  explicit AutomationRendererHelper(content::RenderView* render_view);
  virtual ~AutomationRendererHelper();

  // Takes a snapshot of the entire page without changing layout size.
  bool SnapshotEntirePage(WebKit::WebView* view,
                          std::vector<unsigned char>* png_data,
                          std::string* error_msg);

 private:
  void OnSnapshotEntirePage();
#if !defined(NO_TCMALLOC) && (defined(OS_LINUX) || defined(OS_CHROMEOS))
  void OnHeapProfilerDump(const std::string& reason);
#endif

  // RenderViewObserver implementation.
  virtual bool OnMessageReceived(const IPC::Message& message);
  virtual void WillPerformClientRedirect(
      WebKit::WebFrame* frame, const WebKit::WebURL& from,
      const WebKit::WebURL& to, double interval, double fire_time);
  virtual void DidCancelClientRedirect(WebKit::WebFrame* frame);
  virtual void DidCompleteClientRedirect(WebKit::WebFrame* frame,
                                         const WebKit::WebURL& from);

  DISALLOW_COPY_AND_ASSIGN(AutomationRendererHelper);
};

#endif  // CHROME_RENDERER_AUTOMATION_AUTOMATION_RENDERER_HELPER_H_
