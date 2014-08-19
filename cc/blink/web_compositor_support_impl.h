// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BLINK_WEB_COMPOSITOR_SUPPORT_IMPL_H_
#define CC_BLINK_WEB_COMPOSITOR_SUPPORT_IMPL_H_

#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop_proxy.h"
#include "cc/blink/cc_blink_export.h"
#include "third_party/WebKit/public/platform/WebCompositorAnimationCurve.h"
#include "third_party/WebKit/public/platform/WebCompositorSupport.h"
#include "third_party/WebKit/public/platform/WebLayer.h"
#include "third_party/WebKit/public/platform/WebTransformOperations.h"

namespace blink {
class WebGraphicsContext3D;
}

namespace cc_blink {

class CC_BLINK_EXPORT WebCompositorSupportImpl
    : public NON_EXPORTED_BASE(blink::WebCompositorSupport) {
 public:
  WebCompositorSupportImpl();
  virtual ~WebCompositorSupportImpl();

  virtual blink::WebLayer* createLayer();
  virtual blink::WebContentLayer* createContentLayer(
      blink::WebContentLayerClient* client);
  virtual blink::WebExternalTextureLayer* createExternalTextureLayer(
      blink::WebExternalTextureLayerClient* client);
  virtual blink::WebImageLayer* createImageLayer();
  virtual blink::WebNinePatchLayer* createNinePatchLayer();
  virtual blink::WebScrollbarLayer* createScrollbarLayer(
      blink::WebScrollbar* scrollbar,
      blink::WebScrollbarThemePainter painter,
      blink::WebScrollbarThemeGeometry*);
  virtual blink::WebScrollbarLayer* createSolidColorScrollbarLayer(
      blink::WebScrollbar::Orientation orientation,
      int thumb_thickness,
      int track_start,
      bool is_left_side_vertical_scrollbar);
  virtual blink::WebCompositorAnimation* createAnimation(
      const blink::WebCompositorAnimationCurve& curve,
      blink::WebCompositorAnimation::TargetProperty target,
      int animation_id);
  virtual blink::WebFilterAnimationCurve* createFilterAnimationCurve();
  virtual blink::WebFloatAnimationCurve* createFloatAnimationCurve();
  virtual blink::WebScrollOffsetAnimationCurve*
      createScrollOffsetAnimationCurve(
          blink::WebFloatPoint target_value,
          blink::WebCompositorAnimationCurve::TimingFunctionType
              timing_function);
  virtual blink::WebTransformAnimationCurve* createTransformAnimationCurve();
  virtual blink::WebTransformOperations* createTransformOperations();
  virtual blink::WebFilterOperations* createFilterOperations();

 private:
  DISALLOW_COPY_AND_ASSIGN(WebCompositorSupportImpl);
};

}  // namespace cc_blink

#endif  // CC_BLINK_WEB_COMPOSITOR_SUPPORT_IMPL_H_
