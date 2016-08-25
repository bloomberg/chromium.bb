// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SHELF_SHELF_H_
#define ASH_COMMON_SHELF_SHELF_H_

#include "ash/ash_export.h"
#include "ash/common/shelf/shelf_widget.h"
#include "base/macros.h"

namespace app_list {
class ApplicationDragAndDropHost;
}

namespace gfx {
class Rect;
}

namespace ash {
class AppListButton;
class ShelfView;
class WmShelf;

namespace test {
class ShelfTestAPI;
}

// Controller for shelf state.
// DEPRECATED: WmShelf is replacing this class as part of the mus/mash refactor.
// Use WmShelf for access to state (visibility, auto-hide, etc.).
class ASH_EXPORT Shelf {
 public:
  Shelf(WmShelf* wm_shelf, ShelfView* shelf_view, ShelfWidget* widget);
  ~Shelf();

  // Return the shelf for the primary display. NULL if no user is logged in yet.
  // Useful for tests. For production code use ForWindow() because the user may
  // have multiple displays.
  static Shelf* ForPrimaryDisplay();

  // Return the shelf for the display that |window| is currently on, or a shelf
  // on primary display if the shelf per display feature is disabled. NULL if no
  // user is logged in yet.
  static Shelf* ForWindow(WmWindow* window);

  // For porting from Shelf to WmShelf.
  // TODO(jamescook): Remove this.
  WmShelf* wm_shelf() { return wm_shelf_; }

  AppListButton* GetAppListButton() const;

  ShelfWidget* shelf_widget() { return shelf_widget_; }

  // TODO(msw): ShelfLayoutManager should not be accessed externally.
  ShelfLayoutManager* shelf_layout_manager() {
    return shelf_widget_->shelf_layout_manager();
  }

  // Returns ApplicationDragAndDropHost for this shelf.
  app_list::ApplicationDragAndDropHost* GetDragAndDropHostForAppList();

  ShelfView* shelf_view_for_testing() { return shelf_view_; }

 private:
  friend class test::ShelfTestAPI;

  // The shelf controller. Owned by the root window controller.
  WmShelf* wm_shelf_;
  ShelfWidget* shelf_widget_;
  ShelfView* shelf_view_;

  DISALLOW_COPY_AND_ASSIGN(Shelf);
};

}  // namespace ash

#endif  // ASH_COMMON_SHELF_SHELF_H_
