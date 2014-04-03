// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TOUCH_TOUCH_HUD_PROJECTION_H_
#define ASH_TOUCH_TOUCH_HUD_PROJECTION_H_

#include <map>

#include "ash/touch/touch_observer_hud.h"

namespace ash {
class TouchPointView;

// A heads-up display to show active touch points on the screen. As a derivative
// of TouchObserverHUD, objects of this class manage their own lifetime.
class TouchHudProjection : public TouchObserverHUD {
 public:
  explicit TouchHudProjection(aura::Window* initial_root);

  // Overriden from TouchObserverHUD.
  virtual void Clear() OVERRIDE;

 private:
  friend class TouchHudProjectionTest;

  virtual ~TouchHudProjection();

  // Overriden from TouchObserverHUD.
  virtual void OnTouchEvent(ui::TouchEvent* event) OVERRIDE;
  virtual void SetHudForRootWindowController(
      RootWindowController* controller) OVERRIDE;
  virtual void UnsetHudForRootWindowController(
      RootWindowController* controller) OVERRIDE;

  std::map<int, TouchPointView*> points_;

  DISALLOW_COPY_AND_ASSIGN(TouchHudProjection);
};

}  // namespace ash

#endif  // ASH_TOUCH_TOUCH_HUD_PROJECTION_H_
