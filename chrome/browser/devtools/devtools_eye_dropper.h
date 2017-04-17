// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVTOOLS_EYE_DROPPER_H_
#define CHROME_BROWSER_DEVTOOLS_DEVTOOLS_EYE_DROPPER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "content/public/browser/readback_types.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace blink {
class WebMouseEvent;
}

class DevToolsEyeDropper : public content::WebContentsObserver {
 public:
  typedef base::Callback<void(int, int, int, int)> EyeDropperCallback;

  DevToolsEyeDropper(content::WebContents* web_contents,
                     EyeDropperCallback callback);
  ~DevToolsEyeDropper() override;

 private:
  void AttachToHost(content::RenderWidgetHost* host);
  void DetachFromHost();

  // content::WebContentsObserver.
  void DidReceiveCompositorFrame() override;
  void RenderViewCreated(content::RenderViewHost* host) override;
  void RenderViewDeleted(content::RenderViewHost* host) override;
  void RenderViewHostChanged(content::RenderViewHost* old_host,
                             content::RenderViewHost* new_host) override;

  void UpdateFrame();
  void ResetFrame();
  void FrameUpdated(const SkBitmap&, content::ReadbackResponse);
  bool HandleMouseEvent(const blink::WebMouseEvent& event);
  void UpdateCursor();

  EyeDropperCallback callback_;
  SkBitmap frame_;
  int last_cursor_x_;
  int last_cursor_y_;
  content::RenderWidgetHost::MouseEventCallback mouse_event_callback_;
  content::RenderWidgetHost* host_;
  base::WeakPtrFactory<DevToolsEyeDropper> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsEyeDropper);
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVTOOLS_EYE_DROPPER_H_
