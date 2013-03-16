// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LAUNCHER_LAUNCHER_H_
#define ASH_LAUNCHER_LAUNCHER_H_

#include "ash/ash_export.h"
#include "ash/launcher/launcher_types.h"
#include "ash/shelf/shelf_types.h"
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
class ShelfWidget;

class ASH_EXPORT Launcher {
 public:
  Launcher(LauncherModel* launcher_model,
           LauncherDelegate* launcher_delegate,
           ShelfWidget* shelf_widget);
  virtual ~Launcher();

  // Return the launcher for the primary display. NULL if no user is
  // logged in yet.
  static Launcher* ForPrimaryDisplay();

  // Return the launcher for the display that |window| is currently on,
  // or a launcher on primary display if the launcher per display feature
  // is disabled. NULL if no user is logged in yet.
  static Launcher* ForWindow(aura::Window* window);

  void SetAlignment(ShelfAlignment alignment);
  ShelfAlignment alignment() const { return alignment_; }

  // Returns the screen bounds of the item for the specified window. If there is
  // no item for the specified window an empty rect is returned.
  gfx::Rect GetScreenBoundsOfItemIconForWindow(aura::Window* window);

  // Updates the icon position given the current window bounds. This is used
  // when dragging panels to reposition them with respect to the other panels.
  void UpdateIconPositionForWindow(aura::Window* window);

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
  bool IsVisible() const;

  views::View* GetAppListButtonView() const;

  // Switches to a 0-indexed (in order of creation) window.
  // A negative index switches to the last window in the list.
  void SwitchToWindow(int window_index);

  // Only to be called for testing. Retrieves the LauncherView.
  // TODO(sky): remove this!
  internal::LauncherView* GetLauncherViewForTest();

  LauncherDelegate* delegate() { return delegate_; }

  ShelfWidget* shelf_widget() { return shelf_widget_; }

  // Set the bounds of the launcher view.
  void SetLauncherViewBounds(gfx::Rect bounds);
  gfx::Rect GetLauncherViewBounds() const;

 private:
  // LauncherView used to display icons.
  internal::LauncherView* launcher_view_;

  ShelfAlignment alignment_;

  LauncherDelegate* delegate_;

  ShelfWidget* shelf_widget_;

  DISALLOW_COPY_AND_ASSIGN(Launcher);
};

}  // namespace ash

#endif  // ASH_LAUNCHER_LAUNCHER_H_
