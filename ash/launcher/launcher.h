// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LAUNCHER_LAUNCHER_H_
#define ASH_LAUNCHER_LAUNCHER_H_

#include "ash/ash_export.h"
#include "ash/launcher/background_animator.h"
#include "ash/launcher/launcher_types.h"
#include "ash/shelf_types.h"
#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "ui/gfx/size.h"
#include "ui/views/widget/widget_observer.h"

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
class ShelfLayoutManager;
}

class LauncherIconObserver;
class LauncherDelegate;
class LauncherModel;

class ASH_EXPORT Launcher: public views::WidgetObserver  {
 public:
  Launcher(LauncherModel* launcher_model,
           LauncherDelegate* launcher_delegate,
           aura::Window* window_container,
           internal::ShelfLayoutManager* shelf_layout_manager);
  virtual ~Launcher();

  // Return the launcher for the primary display. NULL if no user is
  // logged in yet.
  static Launcher* ForPrimaryDisplay();

  // Return the launcher for the display that |window| is currently on,
  // or a launcher on primary display if the launcher per display feature
  // is disabled. NULL if no user is logged in yet.
  static Launcher* ForWindow(aura::Window* window);

  // Sets the focus cycler.  Also adds the launcher to the cycle.
  void SetFocusCycler(internal::FocusCycler* focus_cycler);
  internal::FocusCycler* GetFocusCycler();

  void SetAlignment(ShelfAlignment alignment);
  ShelfAlignment alignment() const { return alignment_; }

  // Sets whether the launcher paints a background. Default is false, but is set
  // to true if a window overlaps the shelf.
  void SetPaintsBackground(
      bool value,
      internal::BackgroundAnimator::ChangeType change_type);
  bool paints_background() const {
    return background_animator_.paints_background();
  }

  // Causes shelf items to be slightly dimmed.
  void SetDimsShelf(bool value);
  bool GetDimsShelf() const;

  // Sets the size of the status area.
  void SetStatusSize(const gfx::Size& size);
  const gfx::Size& status_size() const { return status_size_; }

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

  // Show the context menu for the Launcher.
  void ShowContextMenu(const gfx::Point& location);

  bool IsShowingOverflowBubble() const;

  void SetVisible(bool visible) const;

  views::View* GetAppListButtonView() const;

  // Sets the bounds of the launcher widget, and the dimmer if visible.
  void SetWidgetBounds(const gfx::Rect bounds);

  // Switches to a 0-indexed (in order of creation) window.
  // A negative index switches to the last window in the list.
  void SwitchToWindow(int window_index);

  // Only to be called for testing. Retrieves the LauncherView.
  // TODO(sky): remove this!
  internal::LauncherView* GetLauncherViewForTest();

  LauncherDelegate* delegate() { return delegate_; }

  views::Widget* widget() { return widget_.get(); }

  views::Widget* GetDimmerWidgetForTest() { return dimmer_.get(); }

  aura::Window* window_container() { return window_container_; }

  // Called by the activation delegate, before the launcher is activated
  // when no other windows are visible.
  void WillActivateAsFallback() { activating_as_fallback_ = true; }

  // Overridden from views::WidgetObserver:
  virtual void OnWidgetActivationChanged(
      views::Widget* widget, bool active) OVERRIDE;

 private:
  class DelegateView;
  class DimmerView;

  // Widget hosting the view.
  scoped_ptr<views::Widget> widget_;
  scoped_ptr<views::Widget> dimmer_;

  aura::Window* window_container_;

  // Contents view of the widget. Houses the LauncherView.
  DelegateView* delegate_view_;

  // LauncherView used to display icons.
  internal::LauncherView* launcher_view_;

  ShelfAlignment alignment_;

  LauncherDelegate* delegate_;

  // Size reserved for the status area.
  gfx::Size status_size_;

  // Used to animate the background.
  internal::BackgroundAnimator background_animator_;

  // Used then activation is forced from the activation delegate.
  bool activating_as_fallback_;

  DISALLOW_COPY_AND_ASSIGN(Launcher);
};

}  // namespace ash

#endif  // ASH_LAUNCHER_LAUNCHER_H_
