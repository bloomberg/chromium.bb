// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TOUCH_TOUCH_TRANSFORMER_CONTROLLER_H_
#define ASH_TOUCH_TOUCH_TRANSFORMER_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/display/display_controller.h"
#include "ui/gfx/transform.h"

namespace ui {
struct TouchscreenDevice;
}  // namespace ui

namespace ash {

// TouchTransformerController listens to display configuration change
// and updates the touch transformation for touch displays.
class ASH_EXPORT TouchTransformerController
    : public DisplayController::Observer {
 public:
  TouchTransformerController();
  ~TouchTransformerController() override;

  // Updates the TouchTransformer for touch device and pushes the new
  // TouchTransformer into device manager.
  void UpdateTouchTransformer() const;

  // DisplayController::Observer:
  void OnDisplaysInitialized() override;
  void OnDisplayConfigurationChanged() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(TouchTransformerControllerTest,
                           MirrorModeLetterboxing);
  FRIEND_TEST_ALL_PREFIXES(TouchTransformerControllerTest,
                           MirrorModePillarboxing);
  FRIEND_TEST_ALL_PREFIXES(TouchTransformerControllerTest, SoftwareMirrorMode);
  FRIEND_TEST_ALL_PREFIXES(TouchTransformerControllerTest, ExtendedMode);
  FRIEND_TEST_ALL_PREFIXES(TouchTransformerControllerTest, TouchRadiusScale);

  // Returns a transform that will be used to change an event's location from
  // the touchscreen's coordinate system into |display|'s coordinate system.
  // The transform is also responsible for properly scaling the display if the
  // display supports panel fitting.
  //
  // On X11 events are reported in framebuffer coordinate space, so the
  // |framebuffer_size| is used for scaling.
  // On Ozone events are reported in the touchscreen's resolution, so
  // |touch_display| is used to determine the size and scale the event.
  gfx::Transform GetTouchTransform(const DisplayInfo& display,
                                   const DisplayInfo& touch_display,
                                   const ui::TouchscreenDevice& touchscreen,
                                   const gfx::Size& framebuffer_size) const;

  // Returns the scaling factor for the touch radius such that it scales the
  // radius from |touch_device|'s coordinate system to the |touch_display|'s
  // coordinate system.
  double GetTouchResolutionScale(
      const DisplayInfo& touch_display,
      const ui::TouchscreenDevice& touch_device) const;

  DISALLOW_COPY_AND_ASSIGN(TouchTransformerController);
};

}  // namespace ash

#endif  // ASH_TOUCH_TOUCH_TRANSFORMER_CONTROLLER_H_
