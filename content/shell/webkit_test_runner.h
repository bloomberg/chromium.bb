// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_WEBKIT_TEST_RUNNER_H_
#define CONTENT_SHELL_WEBKIT_TEST_RUNNER_H_

#include "base/memory/scoped_ptr.h"
#include "content/public/renderer/render_view_observer.h"
#include "content/public/renderer/render_view_observer_tracker.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebRect.h"

namespace skia {
class PlatformCanvas;
}

namespace content {

// This is the renderer side of the webkit test runner.
class WebKitTestRunner : public RenderViewObserver,
                         public RenderViewObserverTracker<WebKitTestRunner> {
 public:
  explicit WebKitTestRunner(RenderView* render_view);
  virtual ~WebKitTestRunner();

  // RenderViewObserver implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void DidClearWindowObject(WebKit::WebFrame* frame) OVERRIDE;
  virtual void DidFinishLoad(WebKit::WebFrame* frame) OVERRIDE;
  virtual void DidInvalidateRect(const WebKit::WebRect& rect) OVERRIDE;
  virtual void DidScrollRect(int dx,
                             int dy,
                             const WebKit::WebRect& rect) OVERRIDE;
  virtual void DidRequestScheduleComposite() OVERRIDE;
  virtual void DidRequestScheduleAnimation() OVERRIDE;

  void Display();

 private:
  // Message handlers.
  void OnCaptureTextDump(bool as_text, bool printing, bool recursive);
  void OnCaptureImageDump(const std::string& expected_pixel_hash);

  skia::PlatformCanvas* GetCanvas();
  void UpdatePaintRect(const WebKit::WebRect& rect);
  void PaintRect(const WebKit::WebRect& rect);
  void PaintInvalidatedRegion();
  void DisplayRepaintMask();

  scoped_ptr<skia::PlatformCanvas> canvas_;
  WebKit::WebRect paint_rect_;

  DISALLOW_COPY_AND_ASSIGN(WebKitTestRunner);
};

}  // namespace content

#endif  // CONTENT_SHELL_WEBKIT_TEST_RUNNER_H_
