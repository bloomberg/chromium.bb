// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_UI_FOCUS_RING_LAYER_H_
#define CHROME_BROWSER_CHROMEOS_UI_FOCUS_RING_LAYER_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "ui/compositor/layer_delegate.h"

namespace ui {
class Layer;
}

namespace views {
class Painter;
class View;
}

namespace chromeos {

// FocusRingLayer draws a focus ring for a given view.
class FocusRingLayer : public ui::LayerDelegate {
 public:
  FocusRingLayer();
  virtual ~FocusRingLayer();

  // Updates the focus ring layer for the view or clears it if |view| is NULL.
  void SetForView(views::View* view);

 private:
  // ui::LayerDelegate overrides:
  virtual void OnPaintLayer(gfx::Canvas* canvas) OVERRIDE;
  virtual void OnDeviceScaleFactorChanged(float device_scale_factor) OVERRIDE;
  virtual base::Closure PrepareForLayerBoundsChange() OVERRIDE;

  scoped_ptr<ui::Layer> layer_;
  scoped_ptr<views::Painter> ring_painter_;

  DISALLOW_COPY_AND_ASSIGN(FocusRingLayer);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_UI_FOCUS_RING_LAYER_H_
