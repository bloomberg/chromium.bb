// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_H_
#define ASH_SHELF_SHELF_H_

#include "ash/ash_export.h"
#include "ash/shelf/shelf_constants.h"
#include "ash/shelf/shelf_types.h"
#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "ui/gfx/size.h"
#include "ui/views/widget/widget_observer.h"

namespace app_list {
class ApplicationDragAndDropHost;
}

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
class FocusCycler;
class ShelfDelegate;
class ShelfIconObserver;
class ShelfLayoutManager;
class ShelfModel;
class ShelfView;
class ShelfWidget;

namespace test {
class ShelfTestAPI;
}


class ASH_EXPORT Shelf {
 public:
  static const char kNativeViewName[];

  Shelf(ShelfModel* model, ShelfDelegate* delegate, ShelfWidget* widget);
  virtual ~Shelf();

  // Return the shelf for the primary display. NULL if no user is logged in yet.
  static Shelf* ForPrimaryDisplay();

  // Return the shelf for the display that |window| is currently on, or a shelf
  // on primary display if the shelf per display feature  is disabled. NULL if
  // no user is logged in yet.
  static Shelf* ForWindow(aura::Window* window);

  void SetAlignment(ShelfAlignment alignment);
  ShelfAlignment alignment() const { return alignment_; }

  // Returns the screen bounds of the item for the specified window. If there is
  // no item for the specified window an empty rect is returned.
  gfx::Rect GetScreenBoundsOfItemIconForWindow(const aura::Window* window);

  // Updates the icon position given the current window bounds. This is used
  // when dragging panels to reposition them with respect to the other panels.
  void UpdateIconPositionForWindow(aura::Window* window);

  // Activates the the shelf item specified by the index in the list of shelf
  // items.
  void ActivateShelfItem(int index);

  // Cycles the window focus linearly over the current shelf items.
  void CycleWindowLinear(CycleDirection direction);

  void AddIconObserver(ShelfIconObserver* observer);
  void RemoveIconObserver(ShelfIconObserver* observer);

  // Returns true if the shelf is showing a context menu.
  bool IsShowingMenu() const;

  bool IsShowingOverflowBubble() const;

  void SetVisible(bool visible) const;
  bool IsVisible() const;

  void SchedulePaint();

  views::View* GetAppListButtonView() const;

  // Launch a 0-indexed shelf item in the shelf.
  // A negative index launches the last shelf item in the shelf.
  void LaunchAppIndexAt(int item_index);

  ShelfWidget* shelf_widget() { return shelf_widget_; }

  // Set the bounds of the shelf view.
  void SetShelfViewBounds(gfx::Rect bounds);
  gfx::Rect GetShelfViewBounds() const;

  // Returns rectangle bounding all visible shelf items. Used screen coordinate
  // system.
  gfx::Rect GetVisibleItemsBoundsInScreen() const;

  // Returns ApplicationDragAndDropHost for this shelf.
  app_list::ApplicationDragAndDropHost* GetDragAndDropHostForAppList();

 private:
  friend class test::ShelfTestAPI;

  // ShelfView used to display icons.
  ShelfView* shelf_view_;

  ShelfAlignment alignment_;

  ShelfDelegate* delegate_;

  ShelfWidget* shelf_widget_;

  DISALLOW_COPY_AND_ASSIGN(Shelf);
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_H_
