// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LAUNCHER_LAUNCHER_H_
#define ASH_LAUNCHER_LAUNCHER_H_
#pragma once

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "ash/ash_export.h"
#include "ui/base/animation/animation_delegate.h"
#include "ui/base/animation/slide_animation.h"

namespace aura {
class Window;
}

namespace gfx {
class Rect;
}

namespace views {
class Widget;
}

namespace ash {

namespace internal {
class FocusCycler;
class LauncherView;
}

class LauncherDelegate;
class LauncherModel;

class ASH_EXPORT Launcher : public ui::AnimationDelegate {
 public:
  // How the background can be changed.
  enum BackgroundChangeSpeed {
    CHANGE_ANIMATE,
    CHANGE_IMMEDIATE
  };

  explicit Launcher(aura::Window* window_container);
  virtual ~Launcher();

  // Sets the focus cycler.
  void SetFocusCycler(const internal::FocusCycler* focus_cycler);

  // Sets whether the launcher renders a background. Default is false, but is
  // set to true if a window overlaps the shelf.
  void SetRendersBackground(bool value, BackgroundChangeSpeed speed);

  // Sets the width of the status area.
  void SetStatusWidth(int width);
  int GetStatusWidth();

  // Returns the screen bounds of the item for the specified window. If there is
  // no item for the specified window an empty rect is returned.
  gfx::Rect GetScreenBoundsOfItemIconForWindow(aura::Window* window);
  // Only to be called for testing. Retrieves the LauncherView.
  internal::LauncherView* GetLauncherViewForTest();

  LauncherDelegate* delegate() { return delegate_.get(); }

  LauncherModel* model() { return model_.get(); }
  views::Widget* widget() { return widget_.get(); }

  aura::Window* window_container() { return window_container_; }

  // ui::AnimationDelegate overrides:
  virtual void AnimationProgressed(const ui::Animation* animation) OVERRIDE;

 private:
  class DelegateView;

  scoped_ptr<LauncherModel> model_;

  // Widget hosting the view.
  scoped_ptr<views::Widget> widget_;

  aura::Window* window_container_;

  // Contents view of the widget. Houses the LauncherView.
  DelegateView* delegate_view_;

  // LauncherView used to display icons.
  internal::LauncherView* launcher_view_;

  scoped_ptr<LauncherDelegate> delegate_;

  // Used to animate the background.
  ui::SlideAnimation background_animation_;

  // Whether we render a background.
  bool renders_background_;

  // Current alpha value of the background.
  int background_alpha_;

  DISALLOW_COPY_AND_ASSIGN(Launcher);
};

}  // namespace ash

#endif  // ASH_LAUNCHER_LAUNCHER_H_
