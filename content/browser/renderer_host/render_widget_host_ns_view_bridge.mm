// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "content/browser/renderer_host/render_widget_host_ns_view_bridge.h"

#import <Cocoa/Cocoa.h>

#import "base/mac/scoped_nsobject.h"
#import "content/browser/renderer_host/render_widget_host_view_cocoa.h"
#include "content/browser/renderer_host/render_widget_host_view_mac.h"
#import "skia/ext/skia_utils_mac.h"
#import "ui/base/cocoa/animation_utils.h"

namespace content {

namespace {

// Bridge to a locally-hosted NSView -- this is always instantiated in the same
// process as the NSView. The caller of this interface may exist in another
// process.
class RenderWidgetHostViewNSViewBridgeLocal
    : public RenderWidgetHostNSViewBridge {
 public:
  explicit RenderWidgetHostViewNSViewBridgeLocal(
      std::unique_ptr<RenderWidgetHostNSViewClient> client);
  ~RenderWidgetHostViewNSViewBridgeLocal() override;
  RenderWidgetHostViewCocoa* GetRenderWidgetHostViewCocoa() override;

  void SetBackgroundColor(SkColor color) override;
  void SetVisible(bool visible) override;

 private:
  // Weak, this is owned by |cocoa_view_|'s |client_|, and |cocoa_view_| owns
  // its |client_|.
  RenderWidgetHostViewCocoa* cocoa_view_ = nil;

  // The background CoreAnimation layer which is hosted by |cocoa_view_|.
  base::scoped_nsobject<CALayer> background_layer_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostViewNSViewBridgeLocal);
};

RenderWidgetHostViewNSViewBridgeLocal::RenderWidgetHostViewNSViewBridgeLocal(
    std::unique_ptr<RenderWidgetHostNSViewClient> client) {
  // Since we autorelease |cocoa_view|, our caller must put |GetNativeView()|
  // into the view hierarchy right after calling us.
  cocoa_view_ = [[[RenderWidgetHostViewCocoa alloc]
      initWithClient:std::move(client)] autorelease];

  background_layer_.reset([[CALayer alloc] init]);
  [cocoa_view_ setLayer:background_layer_];
  [cocoa_view_ setWantsLayer:YES];
}

RenderWidgetHostViewNSViewBridgeLocal::
    ~RenderWidgetHostViewNSViewBridgeLocal() {}

RenderWidgetHostViewCocoa*
RenderWidgetHostViewNSViewBridgeLocal::GetRenderWidgetHostViewCocoa() {
  return cocoa_view_;
}

void RenderWidgetHostViewNSViewBridgeLocal::SetBackgroundColor(SkColor color) {
  ScopedCAActionDisabler disabler;
  base::ScopedCFTypeRef<CGColorRef> cg_color(
      skia::CGColorCreateFromSkColor(color));
  [background_layer_ setBackgroundColor:cg_color];
}

void RenderWidgetHostViewNSViewBridgeLocal::SetVisible(bool visible) {
  ScopedCAActionDisabler disabler;
  [cocoa_view_ setHidden:!visible];
}

}  // namespace

// static
std::unique_ptr<RenderWidgetHostNSViewBridge>
RenderWidgetHostNSViewBridge::Create(
    std::unique_ptr<RenderWidgetHostNSViewClient> client) {
  return std::make_unique<RenderWidgetHostViewNSViewBridgeLocal>(
      std::move(client));
}

}  // namespace content
