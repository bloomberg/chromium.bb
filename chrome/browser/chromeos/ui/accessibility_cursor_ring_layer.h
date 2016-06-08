// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_UI_ACCESSIBILITY_CURSOR_RING_LAYER_H_
#define CHROME_BROWSER_CHROMEOS_UI_ACCESSIBILITY_CURSOR_RING_LAYER_H_

#include "chrome/browser/chromeos/ui/accessibility_focus_ring.h"
#include "chrome/browser/chromeos/ui/focus_ring_layer.h"

namespace chromeos {

// A subclass of FocusRingLayer that highlights the mouse cursor while it's
// moving, to make it easier to find visually.
class AccessibilityCursorRingLayer : public FocusRingLayer {
 public:
  AccessibilityCursorRingLayer(FocusRingLayerDelegate* delegate,
                               int red,
                               int green,
                               int blue);
  ~AccessibilityCursorRingLayer() override;

  // Create the layer and update its bounds and position in the hierarchy.
  void Set(const gfx::Point& location);

 private:
  // ui::LayerDelegate overrides:
  void OnPaintLayer(const ui::PaintContext& context) override;

  // The current location.
  gfx::Point location_;

  int red_;
  int green_;
  int blue_;

  DISALLOW_COPY_AND_ASSIGN(AccessibilityCursorRingLayer);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_UI_ACCESSIBILITY_CURSOR_RING_LAYER_H_
