// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_UI_FOCUS_RING_LAYER_H_
#define CHROME_BROWSER_CHROMEOS_UI_FOCUS_RING_LAYER_H_

#include <memory>

#include "base/macros.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/compositor/compositor_animation_observer.h"
#include "ui/compositor/layer_delegate.h"
#include "ui/gfx/geometry/rect.h"

namespace aura {
class Window;
}

namespace ui {
class Compositor;
class Layer;
}

namespace chromeos {

// A delegate interface implemented by the object that owns a FocusRingLayer.
class FocusRingLayerDelegate {
 public:
  virtual void OnDeviceScaleFactorChanged() = 0;
  virtual void OnAnimationStep(base::TimeTicks timestamp) = 0;

 protected:
  virtual ~FocusRingLayerDelegate();
};

// FocusRingLayer draws a focus ring at a given global rectangle.
class FocusRingLayer : public ui::LayerDelegate,
                       public ui::CompositorAnimationObserver {
 public:
  explicit FocusRingLayer(FocusRingLayerDelegate* delegate);
  ~FocusRingLayer() override;

  // Move the focus ring layer to the given bounds in the coordinates of
  // the given root window.
  void Set(aura::Window* root_window, const gfx::Rect& bounds);

  // Returns true if this layer is in a composited window with an
  // animation observer.
  bool CanAnimate() const;

  // Set the layer's opacity.
  void SetOpacity(float opacity);

  // Set a custom color, or reset to the default.
  void SetColor(SkColor color);
  void ResetColor();

  ui::Layer* layer() { return layer_.get(); }
  aura::Window* root_window() { return root_window_; }

 protected:
  // Updates |root_window_| and creates |layer_| if it doesn't exist,
  // or if the root window has changed. Moves the layer to the top if
  // it wasn't there already.
  void CreateOrUpdateLayer(aura::Window* root_window,
                           const char* layer_name,
                           const gfx::Rect& bounds);

  bool has_custom_color() { return custom_color_.has_value(); }
  SkColor custom_color() { return *custom_color_; }

 private:
  // ui::LayerDelegate overrides:
  void OnPaintLayer(const ui::PaintContext& context) override;
  void OnDelegatedFrameDamage(const gfx::Rect& damage_rect_in_dip) override;
  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override;

  // CompositorAnimationObserver overrides:
  void OnAnimationStep(base::TimeTicks timestamp) override;
  void OnCompositingShuttingDown(ui::Compositor* compositor) override;

  // The object that owns this layer.
  FocusRingLayerDelegate* delegate_;

  // The current root window containing the focused object.
  aura::Window* root_window_ = nullptr;

  // The current layer.
  std::unique_ptr<ui::Layer> layer_;

  // The bounding rectangle of the focused object, in |root_window_|
  // coordinates.
  gfx::Rect focus_ring_;

  base::Optional<SkColor> custom_color_;

  // The compositor associated with this layer.
  ui::Compositor* compositor_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(FocusRingLayer);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_UI_FOCUS_RING_LAYER_H_
