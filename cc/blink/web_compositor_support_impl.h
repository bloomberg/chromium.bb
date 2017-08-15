// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BLINK_WEB_COMPOSITOR_SUPPORT_IMPL_H_
#define CC_BLINK_WEB_COMPOSITOR_SUPPORT_IMPL_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "cc/blink/cc_blink_export.h"
#include "third_party/WebKit/public/platform/WebCompositorSupport.h"
#include "third_party/WebKit/public/platform/WebContentLayerClient.h"
#include "third_party/WebKit/public/platform/WebLayer.h"

namespace cc_blink {

class CC_BLINK_EXPORT WebCompositorSupportImpl
    : public blink::WebCompositorSupport {
 public:
  WebCompositorSupportImpl();
  ~WebCompositorSupportImpl() override;

  std::unique_ptr<blink::WebLayer> CreateLayer() override;
  std::unique_ptr<blink::WebLayer> CreateLayerFromCCLayer(cc::Layer*) override;
  std::unique_ptr<blink::WebContentLayer> CreateContentLayer(
      blink::WebContentLayerClient* client) override;
  std::unique_ptr<blink::WebExternalTextureLayer> CreateExternalTextureLayer(
      cc::TextureLayerClient* client) override;
  std::unique_ptr<blink::WebImageLayer> CreateImageLayer() override;
  std::unique_ptr<blink::WebScrollbarLayer> CreateScrollbarLayer(
      std::unique_ptr<blink::WebScrollbar> scrollbar,
      blink::WebScrollbarThemePainter painter,
      std::unique_ptr<blink::WebScrollbarThemeGeometry>) override;
  std::unique_ptr<blink::WebScrollbarLayer> CreateOverlayScrollbarLayer(
      std::unique_ptr<blink::WebScrollbar> scrollbar,
      blink::WebScrollbarThemePainter painter,
      std::unique_ptr<blink::WebScrollbarThemeGeometry>) override;
  std::unique_ptr<blink::WebScrollbarLayer> CreateSolidColorScrollbarLayer(
      blink::WebScrollbar::Orientation orientation,
      int thumb_thickness,
      int track_start,
      bool is_left_side_vertical_scrollbar) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebCompositorSupportImpl);
};

}  // namespace cc_blink

#endif  // CC_BLINK_WEB_COMPOSITOR_SUPPORT_IMPL_H_
