// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DISPLAY_OVERSCAN_CALIBRATOR_H_
#define CHROME_BROWSER_CHROMEOS_DISPLAY_OVERSCAN_CALIBRATOR_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "ui/compositor/layer_delegate.h"
#include "ui/gfx/display.h"
#include "ui/gfx/insets.h"
#include "ui/gfx/rect.h"

namespace ui {
class Layer;
}

namespace chromeos {

// This is used to show the visible feedback to the user's operations for
// calibrating display overscan settings.
class OverscanCalibrator : public ui::LayerDelegate {
 public:
  OverscanCalibrator(const gfx::Display& target_display,
                     const gfx::Insets& initial_insets);
  virtual ~OverscanCalibrator();

  // Commits the current insets data to the system.
  void Commit();

  // Reset the overscan insets to default value.  If the display has
  // overscan, the default value is the display's default overscan
  // value. Otherwise, the default value is the old |initial_insets_|.
  void Reset();

  // Updates the insets and redraw the visual feedback.
  void UpdateInsets(const gfx::Insets& insets);

  const gfx::Insets& insets() const { return insets_; }

 private:
  // ui::LayerDelegate overrides:
  virtual void OnPaintLayer(gfx::Canvas* canvas) OVERRIDE;
  virtual void OnDelegatedFrameDamage(
      const gfx::Rect& damage_rect_in_dip) OVERRIDE;
  virtual void OnDeviceScaleFactorChanged(float device_scale_factor) OVERRIDE;
  virtual base::Closure PrepareForLayerBoundsChange() OVERRIDE;

  // The target display.
  const gfx::Display display_;

  // The current insets.
  gfx::Insets insets_;

  // The insets initially given. Stored so we can undo the insets later.
  gfx::Insets initial_insets_;

  // Whether the current insets are committed to the system or not.
  bool committed_;

  // The visualization layer for the current calibration region.
  scoped_ptr<ui::Layer> calibration_layer_;

  DISALLOW_COPY_AND_ASSIGN(OverscanCalibrator);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_DISPLAY_OVERSCAN_CALIBRATOR_H_
