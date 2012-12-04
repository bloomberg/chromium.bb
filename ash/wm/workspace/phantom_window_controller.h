// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WORKSPACE_PHANTOM_WINDOW_CONTROLLER_H_
#define ASH_WM_WORKSPACE_PHANTOM_WINDOW_CONTROLLER_H_

#include "ash/ash_export.h"
#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
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
  // parent). If |layer| is non-NULL, it is shown on top of the phantom window.
  // |layer| is owned by the caller.
  // This does not immediately show the window.
  void Show(const gfx::Rect& bounds);

  // Hides the phantom.
  void Hide();

  // Returns true if the phantom is showing.
  bool IsShowing() const;

  // If set, the phantom window is stacked below this window, otherwise it
  // is stacked above the window passed to the constructor.
  void set_phantom_below_window(aura::Window* phantom_below_window) {
    phantom_below_window_ = phantom_below_window;
  }

  // ui::AnimationDelegate overrides:
  virtual void AnimationProgressed(const ui::Animation* animation) OVERRIDE;

 private:
  FRIEND_TEST_ALL_PREFIXES(WorkspaceWindowResizerTest, PhantomStyle);

  // Creates and shows the |phantom_widget_| at |bounds|.
  // |layer| is shown on top of the phantom window if it is non-NULL.
  // |layer| is not owned by this object.
  void CreatePhantomWidget(const gfx::Rect& bounds);

  // Window the phantom is placed beneath.
  aura::Window* window_;

  // If set, the phantom window should get stacked below this window.
  aura::Window* phantom_below_window_;

  // Initially the bounds of |window_|. Each time Show() is invoked
  // |start_bounds_| is then reset to the bounds of |phantom_widget_| and
  // |bounds_| is set to the value passed into Show(). The animation animates
  // between these two values.
  gfx::Rect start_bounds_;
  gfx::Rect bounds_;

  views::Widget* phantom_widget_;

  // Used to transition the bounds.
  scoped_ptr<ui::SlideAnimation> animation_;

  DISALLOW_COPY_AND_ASSIGN(PhantomWindowController);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_WORKSPACE_PHANTOM_WINDOW_CONTROLLER_H_
