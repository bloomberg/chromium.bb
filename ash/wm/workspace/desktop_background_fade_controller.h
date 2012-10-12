// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WORKSPACE_DESKTOP_BACKGROUND_FADE_CONTROLLER_H_
#define ASH_WM_WORKSPACE_DESKTOP_BACKGROUND_FADE_CONTROLLER_H_

#include "ash/ash_export.h"
#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "ui/compositor/layer_animation_observer.h"

namespace aura {
class Window;
}

namespace base {
class TimeDelta;
}

namespace ash {
namespace internal {

class ColoredWindowController;

// DesktopBackgroundFadeController handles fading in or out the desktop. It is
// used when maximizing or restoring a window. It is implemented as a colored
// layer whose opacity varies. This results in fading in or out all the windows
// the DesktopBackgroundFadeController is placed on top of. This is used
// instead of varying the opacity for two reasons:
// . The window showing background and the desktop workspace do not have a
//   common parent that can be animated. This could be fixed, but wouldn't
//   address the following.
// . When restoring the window is moved back to the desktop workspace. If we
//   animated the opacity of the desktop workspace the cross fade would be
//   effected.
class ASH_EXPORT DesktopBackgroundFadeController
    : public ui::ImplicitAnimationObserver {
 public:
  // Direction to fade.
  enum Direction {
    FADE_IN,
    FADE_OUT,
  };

  // Creates a new DesktopBackgroundFadeController. |parent| is the Window to
//   parent the newly created window to. The newly created window is stacked
//   directly on top of |position_above|. The window animating the fade is
//   destroyed as soon as the animation completes.
  DesktopBackgroundFadeController(aura::Window* parent,
                                  aura::Window* position_above,
                                  base::TimeDelta duration,
                                  Direction direction);
  virtual ~DesktopBackgroundFadeController();

 private:
  // ImplicitAnimationObserver overrides:
  virtual void OnImplicitAnimationsCompleted() OVERRIDE;

  scoped_ptr<ColoredWindowController> window_controller_;

  DISALLOW_COPY_AND_ASSIGN(DesktopBackgroundFadeController);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_WORKSPACE_DESKTOP_BACKGROUND_FADE_CONTROLLER_H_
