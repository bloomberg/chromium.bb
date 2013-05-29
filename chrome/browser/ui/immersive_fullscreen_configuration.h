// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_IMMERSIVE_FULLSCREEN_CONFIGURATION_H_
#define CHROME_BROWSER_UI_IMMERSIVE_FULLSCREEN_CONFIGURATION_H_

#include "base/basictypes.h"

class ImmersiveFullscreenConfiguration {
 public:
  // Returns true if immersive mode should be used for fullscreen based on
  // command line flags.
  static bool UseImmersiveFullscreen();

  static void EnableImmersiveFullscreenForTest();

  static int immersive_mode_reveal_delay_ms() {
    return immersive_mode_reveal_delay_ms_;
  }
  static void set_immersive_mode_reveal_delay_ms(int val) {
    immersive_mode_reveal_delay_ms_ = val;
  }

  static int immersive_mode_reveal_x_threshold_pixels() {
    return immersive_mode_reveal_x_threshold_pixels_;
  }
  static void set_immersive_mode_reveal_x_threshold_pixels(int val) {
    immersive_mode_reveal_x_threshold_pixels_ = val;
  }

 private:
  // The time after which the edge trigger fires and top-chrome is revealed in
  // immersive fullscreen. This is after the mouse stops moving.
  static int immersive_mode_reveal_delay_ms_;

  // Threshold for horizontal mouse movement at the top of the screen for the
  // mouse to be considered "moving" in immersive fullscreen. This allows the
  // user to trigger a reveal even if their fingers are not completely still on
  // the trackpad or mouse.
  static int immersive_mode_reveal_x_threshold_pixels_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ImmersiveFullscreenConfiguration);
};

#endif  // CHROME_BROWSER_UI_IMMERSIVE_FULLSCREEN_CONFIGURATION_H_
