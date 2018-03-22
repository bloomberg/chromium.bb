// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "content/browser/renderer_host/render_widget_host_ns_view_bridge.h"

#import "content/browser/renderer_host/render_widget_host_view_cocoa.h"
#include "content/browser/renderer_host/render_widget_host_view_mac.h"

namespace content {

namespace {

// Bridge to a locally-hosted NSView -- this is always instantiated in the same
// process as the NSView. The caller of this interface may exist in another
// process.
class RenderWidgetHostViewNSViewBridgeLocal
    : public RenderWidgetHostNSViewBridge {
 public:
  explicit RenderWidgetHostViewNSViewBridgeLocal(
      std::unique_ptr<RenderWidgetHostNSViewClient> client) {
    // Since we autorelease |cocoa_view|, our caller must put |GetNativeView()|
    // into the view hierarchy right after calling us.
    cocoa_view_ = [[[RenderWidgetHostViewCocoa alloc]
        initWithClient:std::move(client)] autorelease];
  }
  ~RenderWidgetHostViewNSViewBridgeLocal() override {}

  RenderWidgetHostViewCocoa* GetRenderWidgetHostViewCocoa() override {
    return cocoa_view_;
  }

 private:
  // Weak, this is owned by |cocoa_view_|'s |client_|, and |cocoa_view_| owns
  // its |client_|.
  RenderWidgetHostViewCocoa* cocoa_view_ = nil;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostViewNSViewBridgeLocal);
};

}  // namespace

// static
std::unique_ptr<RenderWidgetHostNSViewBridge>
RenderWidgetHostNSViewBridge::Create(
    std::unique_ptr<RenderWidgetHostNSViewClient> client) {
  return std::make_unique<RenderWidgetHostViewNSViewBridgeLocal>(
      std::move(client));
}

}  // namespace content
