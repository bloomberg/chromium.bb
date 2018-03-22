// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_NS_VIEW_BRIDGE_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_NS_VIEW_BRIDGE_H_

@class RenderWidgetHostViewCocoa;

#include <memory>

#include "base/macros.h"

namespace content {

class RenderWidgetHostNSViewClient;

// The interface through which RenderWidgetHostViewMac is to manipulate its
// corresponding NSView (potentially in another process).
class RenderWidgetHostNSViewBridge {
 public:
  RenderWidgetHostNSViewBridge() {}
  virtual ~RenderWidgetHostNSViewBridge() {}

  static std::unique_ptr<RenderWidgetHostNSViewBridge> Create(
      std::unique_ptr<RenderWidgetHostNSViewClient> client);

  // TODO(ccameron): RenderWidgetHostViewMac and other functions currently use
  // this method to communicate directly with RenderWidgetHostViewCocoa. The
  // goal of this class is to eliminate this direct communication (so this
  // method is expected to go away).
  virtual RenderWidgetHostViewCocoa* GetRenderWidgetHostViewCocoa() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostNSViewBridge);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_NS_VIEW_BRIDGE_H_
