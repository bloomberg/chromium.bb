// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_WM_IMMERSIVE_WM_IMMERSIVE_FULLSCREEN_CONTROLLER_H_
#define ASH_COMMON_WM_IMMERSIVE_WM_IMMERSIVE_FULLSCREEN_CONTROLLER_H_

#include "ash/ash_export.h"

namespace views {
class View;
class Widget;
}

namespace ash {

class WmImmersiveFullscreenControllerDelegate;

// Controller for immersive mode. See ImmersiveFullscreenController for details.
class ASH_EXPORT WmImmersiveFullscreenController {
 public:
  // The enum is used for an enumerated histogram. New items should be only
  // added to the end.
  enum WindowType {
    WINDOW_TYPE_OTHER,
    WINDOW_TYPE_BROWSER,
    WINDOW_TYPE_HOSTED_APP,
    WINDOW_TYPE_PACKAGED_APP,
    WINDOW_TYPE_COUNT
  };

  virtual ~WmImmersiveFullscreenController() {}

  // Initializes the controller. Must be called prior to enabling immersive
  // fullscreen via SetEnabled(). |top_container| is used to keep the
  // top-of-window views revealed when a child of |top_container| has focus.
  // |top_container| does not affect which mouse and touch events keep the
  // top-of-window views revealed. |widget| is the widget to make fullscreen.
  virtual void Init(WmImmersiveFullscreenControllerDelegate* delegate,
                    views::Widget* widget,
                    views::View* top_container) = 0;

  // Enables or disables immersive fullscreen.
  // |window_type| is the type of window which is put in immersive fullscreen.
  // It is only used for histogramming.
  virtual void SetEnabled(WindowType window_type, bool enable) = 0;

  // Returns true if in immersive fullscreen.
  virtual bool IsEnabled() const = 0;

  // Returns true if in immersive fullscreen and the top-of-window views are
  // fully or partially visible.
  virtual bool IsRevealed() const = 0;
};

}  // namespace ash

#endif  // ASH_COMMON_WM_IMMERSIVE_WM_IMMERSIVE_FULLSCREEN_CONTROLLER_H_
