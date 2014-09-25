// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_UI_FOCUS_RING_LAYER_H_
#define CHROME_BROWSER_CHROMEOS_UI_FOCUS_RING_LAYER_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "ui/compositor/layer_delegate.h"
#include "ui/gfx/rect.h"

namespace aura {
class Window;
}

namespace ui {
class Layer;
}

namespace chromeos {

// A delegate interface implemented by the object that owns a FocusRingLayer.
class FocusRingLayerDelegate {
 public:
  virtual void OnDeviceScaleFactorChanged() = 0;

 protected:
  virtual ~FocusRingLayerDelegate();
};

// FocusRingLayer draws a focus ring at a given global rectangle.
class FocusRingLayer : public ui::LayerDelegate {
 public:
  explicit FocusRingLayer(FocusRingLayerDelegate* delegate);
  virtual ~FocusRingLayer();

  // Move the focus ring layer to the given bounds in the coordinates of
  // the given root window.
  void Set(aura::Window* root_window, const gfx::Rect& bounds);

  ui::Layer* layer() { return layer_.get(); }
  aura::Window* root_window() { return root_window_; }

 protected:
  // Updates |root_window_| and creates |layer_| if it doesn't exist,
  // or if the root window has changed. Moves the layer to the top if
  // it wasn't there already.
  void CreateOrUpdateLayer(aura::Window* root_window, const char* layer_name);

 private:
  // ui::LayerDelegate overrides:
  virtual void OnPaintLayer(gfx::Canvas* canvas) OVERRIDE;
  virtual void OnDelegatedFrameDamage(
      const gfx::Rect& damage_rect_in_dip) OVERRIDE;
  virtual void OnDeviceScaleFactorChanged(float device_scale_factor) OVERRIDE;
  virtual base::Closure PrepareForLayerBoundsChange() OVERRIDE;

  // The object that owns this layer.
  FocusRingLayerDelegate* delegate_;

  // The current root window containing the focused object.
  aura::Window* root_window_;

  // The current layer.
  scoped_ptr<ui::Layer> layer_;

  // The bounding rectangle of the focused object, in |root_window_|
  // coordinates.
  gfx::Rect focus_ring_;

  DISALLOW_COPY_AND_ASSIGN(FocusRingLayer);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_UI_FOCUS_RING_LAYER_H_
