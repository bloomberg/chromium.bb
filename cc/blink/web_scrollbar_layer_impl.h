// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BLINK_WEB_SCROLLBAR_LAYER_IMPL_H_
#define CC_BLINK_WEB_SCROLLBAR_LAYER_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "cc/blink/cc_blink_export.h"
#include "third_party/blink/public/platform/web_scrollbar.h"
#include "third_party/blink/public/platform/web_scrollbar_layer.h"

namespace blink {
class WebScrollbarThemeGeometry;
class WebScrollbarThemePainter;
}

namespace cc_blink {

class WebLayerImpl;

class WebScrollbarLayerImpl : public blink::WebScrollbarLayer {
 public:
  CC_BLINK_EXPORT WebScrollbarLayerImpl(
      std::unique_ptr<blink::WebScrollbar> scrollbar,
      blink::WebScrollbarThemePainter painter,
      std::unique_ptr<blink::WebScrollbarThemeGeometry> geometry,
      bool is_overlay);
  CC_BLINK_EXPORT WebScrollbarLayerImpl(
      blink::WebScrollbar::Orientation orientation,
      int thumb_thickness,
      int track_start,
      bool is_left_side_vertical_scrollbar);
  ~WebScrollbarLayerImpl() override;

  // blink::WebScrollbarLayer implementation.
  blink::WebLayer* Layer() override;
  void SetScrollLayer(blink::WebLayer* layer) override;

  void SetElementId(const cc::ElementId&) override;

 private:
  std::unique_ptr<WebLayerImpl> layer_;

  DISALLOW_COPY_AND_ASSIGN(WebScrollbarLayerImpl);
};

}  // namespace cc_blink

#endif  // CC_BLINK_WEB_SCROLLBAR_LAYER_IMPL_H_
