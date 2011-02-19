// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_FULLSCREEN_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_FULLSCREEN_HOST_H_

#include "content/browser/renderer_host/render_widget_host.h"

class RenderWidgetFullscreenHost : public RenderWidgetHost {
 public:
  RenderWidgetFullscreenHost(RenderProcessHost* process, int routing_id);
};

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_FULLSCREEN_HOST_H_
