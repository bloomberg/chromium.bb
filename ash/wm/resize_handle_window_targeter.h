// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_RESIZE_HANDLE_WINDOW_TARGETER_H_
#define ASH_WM_RESIZE_HANDLE_WINDOW_TARGETER_H_

#include "ash/wm/common/window_state_observer.h"
#include "base/macros.h"
#include "ui/aura/window_observer.h"
#include "ui/aura/window_targeter.h"
#include "ui/gfx/geometry/insets.h"

namespace ash {

class ImmersiveFullscreenController;

// To allow easy resize, the resize handles should slightly overlap the content
// area of non-maximized and non-fullscreen windows. For immersive fullscreen
// windows, this targeter makes sure that touch-events towards the top of the
// screen are targeted to the window itself (instead of a child window that may
// otherwise have been targeted) when the top-of-window views are not revealed.
class ResizeHandleWindowTargeter : public wm::WindowStateObserver,
                                   public aura::WindowObserver,
                                   public aura::WindowTargeter {
 public:
  ResizeHandleWindowTargeter(aura::Window* window,
                             ImmersiveFullscreenController* immersive);
  ~ResizeHandleWindowTargeter() override;

 private:
  // wm::WindowStateObserver:
  void OnPostWindowStateTypeChange(wm::WindowState* window_state,
                                   wm::WindowStateType old_type) override;
  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;

  // aura::WindowTargeter:
  aura::Window* FindTargetForLocatedEvent(aura::Window* window,
                                          ui::LocatedEvent* event) override;
  bool SubtreeShouldBeExploredForEvent(aura::Window* window,
                                       const ui::LocatedEvent& event) override;

  // The targeter does not take ownership of |window_| or
  // |immersive_controller_|.
  aura::Window* window_;
  gfx::Insets frame_border_inset_;
  ImmersiveFullscreenController* immersive_controller_;

  DISALLOW_COPY_AND_ASSIGN(ResizeHandleWindowTargeter);
};

}  // namespace ash

#endif  // ASH_WM_RESIZE_HANDLE_WINDOW_TARGETER_H_
