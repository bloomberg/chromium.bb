// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_NS_VIEW_CLIENT_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_NS_VIEW_CLIENT_H_

#include "base/macros.h"

namespace content {

class RenderWidgetHostViewMac;

// The interface through which the NSView for a RenderWidgetHostViewMac is to
// communicate with the RenderWidgetHostViewMac (potentially in another
// process).
class RenderWidgetHostNSViewClient {
 public:
  RenderWidgetHostNSViewClient() {}
  virtual ~RenderWidgetHostNSViewClient() {}

  // TODO(ccameron): As with the GetRenderWidgetHostViewCocoa method on
  // RenderWidgetHostNSViewBridge, this method is to be removed.
  virtual RenderWidgetHostViewMac* GetRenderWidgetHostViewMac() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostNSViewClient);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_NS_VIEW_CLIENT_H_
