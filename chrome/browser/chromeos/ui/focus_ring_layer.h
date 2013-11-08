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

namespace views {
class View;
}

namespace chromeos {

// FocusRingLayer draws a focus ring for a given view.
class FocusRingLayer : public ui::LayerDelegate {
 public:
  FocusRingLayer();
  virtual ~FocusRingLayer();

  // Create the layer and update its bounds and position in the hierarchy.
  void Update();

  // Updates the focus ring layer for the view or clears it if |view| is NULL.
  void SetForView(views::View* view);

 private:
  // ui::LayerDelegate overrides:
  virtual void OnPaintLayer(gfx::Canvas* canvas) OVERRIDE;
  virtual void OnDeviceScaleFactorChanged(float device_scale_factor) OVERRIDE;
  virtual base::Closure PrepareForLayerBoundsChange() OVERRIDE;

  // The window containing focus.
  aura::Window* window_;

  // The current root window containing the focused object.
  aura::Window* root_window_;

  // The bounding rectangle of the focused object, in |window_| coordinates.
  gfx::Rect focus_ring_;

  // The current layer.
  scoped_ptr<ui::Layer> layer_;

  DISALLOW_COPY_AND_ASSIGN(FocusRingLayer);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_UI_FOCUS_RING_LAYER_H_
