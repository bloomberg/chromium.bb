// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WORKSPACE_PHANTOM_WINDOW_CONTROLLER_H_
#define ASH_WM_WORKSPACE_PHANTOM_WINDOW_CONTROLLER_H_

#include "ash/ash_export.h"
#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "ui/base/animation/animation_delegate.h"
#include "ui/gfx/rect.h"

namespace aura {
class Window;
}

namespace ui {
class SlideAnimation;
}

namespace views {
class Widget;
}

namespace ash {
namespace internal {

// PhantomWindowController is responsible for showing a phantom representation
// of a window. It's used used during dragging a window to show a snap location.
class ASH_EXPORT PhantomWindowController : public ui::AnimationDelegate {
 public:
  explicit PhantomWindowController(aura::Window* window);
  virtual ~PhantomWindowController();

  // Bounds last passed to Show().
  const gfx::Rect& bounds() const { return bounds_; }

  // Shows the phantom window at the specified location (coordinates of the
  // parent). This does not immediately show the window.
  void Show(const gfx::Rect& bounds);

  // This is used to set bounds for the phantom window immediately. This should
  // be called only when the phantom window is already visible.
  void SetBounds(const gfx::Rect& bounds);

  // Hides the phantom.
  void Hide();

  // Returns true if the phantom is showing.
  bool IsShowing() const;

  // ui::AnimationDelegate overrides:
  virtual void AnimationProgressed(const ui::Animation* animation) OVERRIDE;

 private:
  // Creates and shows the |phantom_widget_| at |bounds|.
  void CreatePhantomWidget(const gfx::Rect& bounds);

  // Window the phantom is placed beneath.
  aura::Window* window_;

  // Initially the bounds of |window_|. Each time Show() is invoked
  // |start_bounds_| is then reset to the bounds of |phantom_widget_| and
  // |bounds_| is set to the value passed into Show(). The animation animates
  // between these two values.
  gfx::Rect start_bounds_;
  gfx::Rect bounds_;

  scoped_ptr<views::Widget> phantom_widget_;

  // Used to transition the bounds.
  scoped_ptr<ui::SlideAnimation> animation_;

  DISALLOW_COPY_AND_ASSIGN(PhantomWindowController);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_WORKSPACE_PHANTOM_WINDOW_CONTROLLER_H_
