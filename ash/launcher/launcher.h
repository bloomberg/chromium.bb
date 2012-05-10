// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LAUNCHER_LAUNCHER_H_
#define ASH_LAUNCHER_LAUNCHER_H_
#pragma once

#include "ash/ash_export.h"
#include "ash/launcher/background_animator.h"
#include "ash/launcher/launcher_types.h"
#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"

namespace aura {
class Window;
}

namespace gfx {
class Rect;
}

namespace views {
class View;
class Widget;
}

namespace ash {

namespace internal {
class FocusCycler;
class LauncherView;
}

class LauncherIconObserver;
class LauncherDelegate;
class LauncherModel;

class ASH_EXPORT Launcher : public internal::BackgroundAnimatorDelegate {
 public:
  explicit Launcher(aura::Window* window_container);
  virtual ~Launcher();

  // Sets the focus cycler.  Also adds the launcher to the cycle.
  void SetFocusCycler(internal::FocusCycler* focus_cycler);
  internal::FocusCycler* GetFocusCycler();

  // Sets whether the launcher paints a background. Default is false, but is set
  // to true if a window overlaps the shelf.
  void SetPaintsBackground(
      bool value,
      internal::BackgroundAnimator::ChangeType change_type);

  // Sets the width of the status area.
  void SetStatusWidth(int width);
  int GetStatusWidth();

  // Returns the screen bounds of the item for the specified window. If there is
  // no item for the specified window an empty rect is returned.
  gfx::Rect GetScreenBoundsOfItemIconForWindow(aura::Window* window);

  // Activates the the launcher item specified by the index in the list
  // of launcher items.
  void ActivateLauncherItem(int index);

  // Cycles the window focus linearly over the current launcher items.
  void CycleWindowLinear(CycleDirection direction);

  void AddIconObserver(LauncherIconObserver* observer);
  void RemoveIconObserver(LauncherIconObserver* observer);

  // Returns true if the Launcher is showing a context menu.
  bool IsShowingMenu() const;

  views::View* GetAppListButtonView() const;

  // Only to be called for testing. Retrieves the LauncherView.
  // TODO(sky): remove this!
  internal::LauncherView* GetLauncherViewForTest();

  LauncherDelegate* delegate() { return delegate_.get(); }

  LauncherModel* model() { return model_.get(); }
  views::Widget* widget() { return widget_.get(); }

  aura::Window* window_container() { return window_container_; }

  // BackgroundAnimatorDelegate overrides:
  virtual void UpdateBackground(int alpha) OVERRIDE;

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
  internal::BackgroundAnimator background_animator_;

  DISALLOW_COPY_AND_ASSIGN(Launcher);
};

}  // namespace ash

#endif  // ASH_LAUNCHER_LAUNCHER_H_
