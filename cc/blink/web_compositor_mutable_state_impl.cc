// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/blink/web_compositor_mutable_state_impl.h"

#include "cc/animation/layer_tree_mutation.h"
#include "cc/layers/layer_impl.h"
#include "cc/trees/layer_tree_impl.h"

namespace cc_blink {

WebCompositorMutableStateImpl::WebCompositorMutableStateImpl(
    cc::LayerTreeMutation* mutation,
    cc::LayerImpl* main_layer,
    cc::LayerImpl* scroll_layer)
    : mutation_(mutation),
      main_layer_(main_layer),
      scroll_layer_(scroll_layer) {}

WebCompositorMutableStateImpl::~WebCompositorMutableStateImpl() {}

double WebCompositorMutableStateImpl::opacity() const {
  return main_layer_->opacity();
}

void WebCompositorMutableStateImpl::setOpacity(double opacity) {
  if (!main_layer_)
    return;
  main_layer_->OnOpacityAnimated(opacity);
  mutation_->SetOpacity(opacity);
}

const SkMatrix44& WebCompositorMutableStateImpl::transform() const {
  static SkMatrix44 identity;
  return main_layer_ ? main_layer_->transform().matrix() : identity;
}

void WebCompositorMutableStateImpl::setTransform(const SkMatrix44& matrix) {
  if (!main_layer_)
    return;
  main_layer_->OnTransformAnimated(gfx::Transform(matrix));
  mutation_->SetTransform(matrix);
}

double WebCompositorMutableStateImpl::scrollLeft() const {
  return scroll_layer_ ? scroll_layer_->CurrentScrollOffset().x() : 0.0;
}

void WebCompositorMutableStateImpl::setScrollLeft(double scroll_left) {
  if (!scroll_layer_)
    return;
  gfx::ScrollOffset offset = scroll_layer_->CurrentScrollOffset();
  offset.set_x(scroll_left);
  scroll_layer_->OnScrollOffsetAnimated(offset);
  mutation_->SetScrollLeft(scroll_left);
}

double WebCompositorMutableStateImpl::scrollTop() const {
  return scroll_layer_ ? scroll_layer_->CurrentScrollOffset().y() : 0.0;
}

void WebCompositorMutableStateImpl::setScrollTop(double scroll_top) {
  if (!scroll_layer_)
    return;
  gfx::ScrollOffset offset = scroll_layer_->CurrentScrollOffset();
  offset.set_y(scroll_top);
  scroll_layer_->OnScrollOffsetAnimated(offset);
  mutation_->SetScrollTop(scroll_top);
}

}  // namespace cc_blink
