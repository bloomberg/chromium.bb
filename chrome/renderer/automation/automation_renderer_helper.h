// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_AUTOMATION_AUTOMATION_RENDERER_HELPER_H_
#define CHROME_RENDERER_AUTOMATION_AUTOMATION_RENDERER_HELPER_H_
#pragma once

#include "content/public/renderer/render_view_observer.h"

namespace WebKit {
class WebFrame;
class WebURL;
}

// Filters automation/testing messages sent to a |RenderView| and sends
// automation/testing messages to the browser.
class AutomationRendererHelper : public content::RenderViewObserver {
 public:
  explicit AutomationRendererHelper(RenderView* render_view);
  virtual ~AutomationRendererHelper();

 private:
  // RenderViewObserver implementation.
  virtual void WillPerformClientRedirect(
      WebKit::WebFrame* frame, const WebKit::WebURL& from,
      const WebKit::WebURL& to, double interval, double fire_time);
  virtual void DidCancelClientRedirect(WebKit::WebFrame* frame);
  virtual void DidCompleteClientRedirect(WebKit::WebFrame* frame,
                                         const WebKit::WebURL& from);

  DISALLOW_COPY_AND_ASSIGN(AutomationRendererHelper);
};

#endif  // CHROME_RENDERER_AUTOMATION_AUTOMATION_RENDERER_HELPER_H_
