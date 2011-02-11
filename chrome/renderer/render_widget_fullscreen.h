// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_RENDER_WIDGET_FULLSCREEN_H_
#define CHROME_RENDERER_RENDER_WIDGET_FULLSCREEN_H_

#include "chrome/renderer/render_widget.h"

#include "third_party/WebKit/Source/WebKit/chromium/public/WebWidget.h"

// TODO(boliu): Override non-supported methods with no-op? eg setWindowRect().
class RenderWidgetFullscreen : public RenderWidget {
 public:
  // Creates a new RenderWidget.  The opener_id is the routing ID of the
  // RenderView that this widget lives inside. The render_thread is any
  // RenderThreadBase implementation, mostly commonly RenderThread::current().
  static RenderWidgetFullscreen* Create(int32 opener_id,
                                        RenderThreadBase* render_thread);

  virtual void show(WebKit::WebNavigationPolicy);

 protected:
  virtual WebKit::WebWidget* CreateWebWidget();
  RenderWidgetFullscreen(RenderThreadBase* render_thread);

  void Init(int32 opener_id);
};

#endif  // CHROME_RENDERER_RENDER_WIDGET_FULLSCREEN_H_
