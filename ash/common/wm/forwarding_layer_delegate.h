// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_WM_FORWARDING_LAYER_DELEGATE_H_
#define ASH_COMMON_WM_FORWARDING_LAYER_DELEGATE_H_

#include "base/macros.h"
#include "ui/compositor/layer_delegate.h"

namespace ui {
class Layer;
}

namespace ash {

class WmWindow;

namespace wm {

// A layer delegate to paint the content of a recreated layer by delegating
// the paint request to the original delegate. It checks if the original
// delegate is still valid by traversing the original layers.
class ForwardingLayerDelegate : public ui::LayerDelegate {
 public:
  ForwardingLayerDelegate(WmWindow* original_window,
                          ui::LayerDelegate* delegate);
  ~ForwardingLayerDelegate() override;

 private:
  bool IsDelegateValid(ui::Layer* layer) const;

  // ui:LayerDelegate:
  void OnPaintLayer(const ui::PaintContext& context) override;
  void OnDelegatedFrameDamage(const gfx::Rect& damage_rect_in_dip) override;
  void OnDeviceScaleFactorChanged(float device_scale_factor) override;
  base::Closure PrepareForLayerBoundsChange() override;

  WmWindow* original_window_;
  ui::LayerDelegate* original_delegate_;

  DISALLOW_COPY_AND_ASSIGN(ForwardingLayerDelegate);
};

}  // namespace wm
}  // namespace ash

#endif  // ASH_COMMON_WM_FORWARDING_LAYER_DELEGATE_H_
