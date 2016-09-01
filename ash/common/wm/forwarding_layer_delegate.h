// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_WM_FORWARDING_LAYER_DELEGATE_H_
#define ASH_COMMON_WM_FORWARDING_LAYER_DELEGATE_H_

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "ui/compositor/layer_delegate.h"
#include "ui/compositor/layer_observer.h"

namespace ui {
class Layer;
}

namespace ash {

class WmWindow;

namespace wm {

// A layer delegate to paint the content of a recreated layer by delegating
// the paint request to the original delegate. It is a delegate of the recreated
// layer and an observer of the original layer.
class ForwardingLayerDelegate : public ui::LayerDelegate,
                                public ui::LayerObserver {
 public:
  ForwardingLayerDelegate(ui::Layer* new_layer, ui::Layer* original_layer);
  ~ForwardingLayerDelegate() override;

  // ui:LayerDelegate:
  void OnPaintLayer(const ui::PaintContext& context) override;
  void OnDelegatedFrameDamage(const gfx::Rect& damage_rect_in_dip) override;
  void OnDeviceScaleFactorChanged(float device_scale_factor) override;
  base::Closure PrepareForLayerBoundsChange() override;

  // ui::LayerObserver:
  void DidPaintLayer(ui::Layer* layer, const gfx::Rect& rect) override;
  void SurfaceChanged(ui::Layer* layer) override;
  void LayerDestroyed(ui::Layer* layer) override;

 private:
  // The layer for which |this| is the delegate.
  ui::Layer* client_layer_;

  // The layer that was recreated to replace |new_layer_|.
  ui::Layer* original_layer_;

  ScopedObserver<ui::Layer, ui::LayerObserver> scoped_observer_;

  DISALLOW_COPY_AND_ASSIGN(ForwardingLayerDelegate);
};

}  // namespace wm
}  // namespace ash

#endif  // ASH_COMMON_WM_FORWARDING_LAYER_DELEGATE_H_
