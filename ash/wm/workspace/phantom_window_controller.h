// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WORKSPACE_PHANTOM_WINDOW_CONTROLLER_H_
#define ASH_WM_WORKSPACE_PHANTOM_WINDOW_CONTROLLER_H_

#include "ash/ash_export.h"
#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/rect.h"

namespace aura {
class Window;
}

namespace gfx {
class SlideAnimation;
}

namespace views {
class Widget;
}

namespace ash {
namespace internal {

// PhantomWindowController is responsible for showing a phantom representation
// of a window. It's used used during dragging a window to show a snap location.
class ASH_EXPORT PhantomWindowController : public gfx::AnimationDelegate {
 public:
  explicit PhantomWindowController(aura::Window* window);
  virtual ~PhantomWindowController();

  // Bounds last passed to Show().
  const gfx::Rect& bounds_in_screen() const { return bounds_in_screen_; }

  // Animates the phantom window towards |bounds_in_screen|.
  // Creates two (if start bounds intersect any root window other than the
  // root window that matches the target bounds) or one (otherwise) phantom
  // widgets to display animated rectangle in each root.
  // This does not immediately show the window.
  void Show(const gfx::Rect& bounds_in_screen);

  // Hides the phantom.
  void Hide();

  // Returns true if the phantom is showing.
  bool IsShowing() const;

  // If set, the phantom window is stacked below this window, otherwise it
  // is stacked above the window passed to the constructor.
  void set_phantom_below_window(aura::Window* phantom_below_window) {
    phantom_below_window_ = phantom_below_window;
  }

  // gfx::AnimationDelegate overrides:
  virtual void AnimationProgressed(const gfx::Animation* animation) OVERRIDE;

 private:
  FRIEND_TEST_ALL_PREFIXES(WorkspaceWindowResizerTest, PhantomWindowShow);

  // Creates, shows and returns a phantom widget at |bounds|
  // with kShellWindowId_ShelfContainer in |root_window| as a parent.
  views::Widget* CreatePhantomWidget(aura::Window* root_window,
                                     const gfx::Rect& bounds_in_screen);

  // Window the phantom is placed beneath.
  aura::Window* window_;

  // If set, the phantom window should get stacked below this window.
  aura::Window* phantom_below_window_;

  // Initially the bounds of |window_| (in screen coordinates).
  // Each time Show() is invoked |start_bounds_| is then reset to the bounds of
  // |phantom_widget_| and |bounds_| is set to the value passed into Show().
  // The animation animates between these two values.
  gfx::Rect start_bounds_;

  // Target bounds of the animation in screen coordinates.
  gfx::Rect bounds_in_screen_;

  // The primary phantom representation of the window. It is parented by the
  // root window matching the target bounds.
  views::Widget* phantom_widget_;

  // If the animation starts on another display, this is the secondary phantom
  // representation of the window used on the initial display, otherwise this is
  // NULL. This allows animation to progress from one display into the other.
  views::Widget* phantom_widget_start_;

  // Used to transition the bounds.
  scoped_ptr<gfx::SlideAnimation> animation_;

  DISALLOW_COPY_AND_ASSIGN(PhantomWindowController);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_WORKSPACE_PHANTOM_WINDOW_CONTROLLER_H_
