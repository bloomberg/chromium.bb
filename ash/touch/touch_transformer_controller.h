// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TOUCH_TOUCH_TRANSFORMER_CONTROLLER_H_
#define ASH_TOUCH_TOUCH_TRANSFORMER_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/display/display_controller.h"
#include "ui/gfx/transform.h"

namespace ash {

// TouchTransformerController listens to display configuration change
// and updates the touch transformation for touch displays.
class ASH_EXPORT TouchTransformerController
    : public DisplayController::Observer {
 public:
  TouchTransformerController();
  virtual ~TouchTransformerController();

  // Updates the TouchTransformer for touch device and pushes the new
  // TouchTransformer into device manager.
  void UpdateTouchTransformer() const;

  // DisplayController::Observer:
  virtual void OnDisplaysInitialized() OVERRIDE;
  virtual void OnDisplayConfigurationChanged() OVERRIDE;

 private:
  FRIEND_TEST_ALL_PREFIXES(TouchTransformerControllerTest,
                           TouchTransformerMirrorModeLetterboxing);
  FRIEND_TEST_ALL_PREFIXES(TouchTransformerControllerTest,
                           TouchTransformerMirrorModePillarboxing);
  FRIEND_TEST_ALL_PREFIXES(TouchTransformerControllerTest,
                           TouchTransformerExtendedMode);

  bool ShouldComputeMirrorModeTouchTransformer(
      const DisplayInfo& touch_display) const ;

  gfx::Transform GetMirrorModeTouchTransformer(
      const DisplayInfo& touch_display) const;

  gfx::Transform GetExtendedModeTouchTransformer(
      const DisplayInfo& touch_display, const gfx::Size& fb_size) const;

  // For unittests only.
  bool force_compute_mirror_mode_touch_transformer_;

  DISALLOW_COPY_AND_ASSIGN(TouchTransformerController);
};

}  // namespace ash

#endif  // ASH_TOUCH_TOUCH_TRANSFORMER_CONTROLLER_H_
