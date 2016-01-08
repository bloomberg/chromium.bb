// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BLINK_WEB_COMPOSITOR_MUTABLE_STATE_IMPL_H_
#define CC_BLINK_WEB_COMPOSITOR_MUTABLE_STATE_IMPL_H_

#include "cc/blink/cc_blink_export.h"

#include "third_party/WebKit/public/platform/WebCompositorMutableState.h"

namespace cc {
class LayerImpl;
class LayerTreeMutation;
}

namespace cc_blink {

class WebCompositorMutableStateImpl : public blink::WebCompositorMutableState {
 public:
  WebCompositorMutableStateImpl(cc::LayerTreeMutation* mutation,
                                cc::LayerImpl* main_layer,
                                cc::LayerImpl* scroll_layer);
  ~WebCompositorMutableStateImpl() override;

  double opacity() const override;
  void setOpacity(double opacity) override;

  const SkMatrix44& transform() const override;
  void setTransform(const SkMatrix44& transform) override;

  double scrollLeft() const override;
  void setScrollLeft(double scroll_left) override;

  double scrollTop() const override;
  void setScrollTop(double scroll_top) override;

 private:
  cc::LayerTreeMutation* mutation_;
  cc::LayerImpl* main_layer_;
  cc::LayerImpl* scroll_layer_;
};

}  // namespace cc_blink

#endif  // CC_BLINK_WEB_COMPOSITOR_MUTABLE_STATE_IMPL_H_
