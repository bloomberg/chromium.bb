// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCESSIBILITY_FOCUS_RING_LAYER_H_
#define ASH_ACCESSIBILITY_FOCUS_RING_LAYER_H_

#include <memory>

#include "ash/accessibility/accessibility_layer.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/compositor/compositor_animation_observer.h"
#include "ui/compositor/layer_delegate.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {

// FocusRingLayer draws a focus ring at a given global rectangle.
class FocusRingLayer : public AccessibilityLayer {
 public:
  explicit FocusRingLayer(AccessibilityLayerDelegate* delegate);
  ~FocusRingLayer() override;

  // AccessibilityLayer overrides:
  bool CanAnimate() const override;
  bool NeedToAnimate() const override;
  int GetInset() const override;

  // Set a custom color, or reset to the default.
  void SetColor(SkColor color);
  void ResetColor();

 protected:
  bool has_custom_color() { return custom_color_.has_value(); }
  SkColor custom_color() { return *custom_color_; }

 private:
  // ui::LayerDelegate overrides:
  void OnPaintLayer(const ui::PaintContext& context) override;

  base::Optional<SkColor> custom_color_;

  DISALLOW_COPY_AND_ASSIGN(FocusRingLayer);
};

}  // namespace ash

#endif  // ASH_ACCESSIBILITY_FOCUS_RING_LAYER_H_
