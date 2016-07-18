// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/wm/forwarding_layer_delegate.h"

#include "ash/common/wm_window.h"
#include "base/callback.h"
#include "ui/compositor/layer.h"

namespace ash {
namespace wm {

ForwardingLayerDelegate::ForwardingLayerDelegate(WmWindow* original_window,
                                                 ui::LayerDelegate* delegate)
    : original_window_(original_window), original_delegate_(delegate) {}

ForwardingLayerDelegate::~ForwardingLayerDelegate() {}

void ForwardingLayerDelegate::OnPaintLayer(const ui::PaintContext& context) {
  if (!original_delegate_)
    return;
  // |original_delegate_| may have already been deleted or
  // disconnected by this time. Check if |original_delegate_| is still
  // used by the original_window tree, or skip otherwise.
  if (IsDelegateValid(original_window_->GetLayer()))
    original_delegate_->OnPaintLayer(context);
  else
    original_delegate_ = nullptr;
}

void ForwardingLayerDelegate::OnDelegatedFrameDamage(
    const gfx::Rect& damage_rect_in_dip) {}

void ForwardingLayerDelegate::OnDeviceScaleFactorChanged(
    float device_scale_factor) {
  // Don't tell the original delegate about device scale factor change
  // on cloned layer because the original layer is still on the same display.
}

base::Closure ForwardingLayerDelegate::PrepareForLayerBoundsChange() {
  return base::Closure();
}

bool ForwardingLayerDelegate::IsDelegateValid(ui::Layer* layer) const {
  if (layer->delegate() == original_delegate_)
    return true;
  for (auto* child : layer->children()) {
    if (IsDelegateValid(child))
      return true;
  }
  return false;
}

}  // namespace wm
}  // namespace ash
