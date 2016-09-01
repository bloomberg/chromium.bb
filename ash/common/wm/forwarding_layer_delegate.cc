// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/wm/forwarding_layer_delegate.h"

#include "ash/common/wm_window.h"
#include "base/callback.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_owner.h"

namespace ash {
namespace wm {

ForwardingLayerDelegate::ForwardingLayerDelegate(ui::Layer* new_layer,
                                                 ui::Layer* original_layer)
    : client_layer_(new_layer),
      original_layer_(original_layer),
      scoped_observer_(this) {
  scoped_observer_.Add(original_layer);
}

ForwardingLayerDelegate::~ForwardingLayerDelegate() {}

void ForwardingLayerDelegate::OnPaintLayer(const ui::PaintContext& context) {
  if (original_layer_ && original_layer_->delegate())
    original_layer_->delegate()->OnPaintLayer(context);
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

void ForwardingLayerDelegate::DidPaintLayer(ui::Layer* layer,
                                            const gfx::Rect& rect) {
  client_layer_->SchedulePaint(rect);
}

void ForwardingLayerDelegate::SurfaceChanged(ui::Layer* layer) {
  // This will delete the old layer and any descendants.
  ui::LayerOwner old_client;
  old_client.SetLayer(client_layer_);

  ui::LayerOwner* owner = layer->owner();
  // The layer recreation step isn't recursive, but layers with surfaces don't
  // tend to have children anyway. We may end up missing some children, but we
  // can also reach that state if layers are ever added or removed.
  // TODO(estade): address this if it ever becomes a practical issue.
  std::unique_ptr<ui::Layer> recreated = owner->RecreateLayer();
  client_layer_ = recreated.get();
  old_client.layer()->parent()->Add(recreated.release());
  old_client.layer()->parent()->Remove(old_client.layer());

  scoped_observer_.Remove(original_layer_);
  original_layer_ = owner->layer();
  scoped_observer_.Add(original_layer_);
}

void ForwardingLayerDelegate::LayerDestroyed(ui::Layer* layer) {
  original_layer_ = nullptr;
  scoped_observer_.Remove(layer);
}

}  // namespace wm
}  // namespace ash
