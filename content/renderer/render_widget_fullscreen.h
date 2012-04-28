// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDER_WIDGET_FULLSCREEN_H_
#define CONTENT_RENDERER_RENDER_WIDGET_FULLSCREEN_H_

#include "content/renderer/render_widget.h"

#include "third_party/WebKit/Source/WebKit/chromium/public/WebWidget.h"

// TODO(boliu): Override non-supported methods with no-op? eg setWindowRect().
class RenderWidgetFullscreen : public RenderWidget {
 public:
  // Creates a new RenderWidget.  The opener_id is the routing ID of the
  // RenderView that this widget lives inside.
  static RenderWidgetFullscreen* Create(int32 opener_id);

  virtual void show(WebKit::WebNavigationPolicy);

 protected:
  RenderWidgetFullscreen();
  virtual ~RenderWidgetFullscreen();

  virtual WebKit::WebWidget* CreateWebWidget();

  void Init(int32 opener_id);
};

#endif  // CONTENT_RENDERER_RENDER_WIDGET_FULLSCREEN_H_
