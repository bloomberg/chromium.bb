// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SHELF_SHELF_WINDOW_WATCHER_H_
#define ASH_COMMON_SHELF_SHELF_WINDOW_WATCHER_H_

#include "ash/common/wm_activation_observer.h"
#include "ash/common/wm_window_observer.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "ui/display/display_observer.h"

namespace ash {

class ShelfModel;
class WmWindow;

// ShelfWindowWatcher creates and handles a ShelfItem for windows in the default
// container that have a valid ShelfItemType property (e.g. the task manager
// dialog or the OS settings window). It adds the ShelfItem when the window is
// added to the default container and maintains it until the window is closed,
// even if the window is transiently reparented (e.g. during a drag).
class ShelfWindowWatcher : public WmActivationObserver,
                           public display::DisplayObserver {
 public:
  explicit ShelfWindowWatcher(ShelfModel* model);
  ~ShelfWindowWatcher() override;

 private:
  // Observes for windows being added to a root window's default container.
  class ContainerWindowObserver : public WmWindowObserver {
   public:
    explicit ContainerWindowObserver(ShelfWindowWatcher* window_watcher);
    ~ContainerWindowObserver() override;

   private:
    // WmWindowObserver:
    void OnWindowTreeChanged(WmWindow* window,
                             const TreeChangeParams& params) override;
    void OnWindowDestroying(WmWindow* window) override;

    ShelfWindowWatcher* window_watcher_;

    DISALLOW_COPY_AND_ASSIGN(ContainerWindowObserver);
  };

  // Observes individual user windows to detect when they are closed or when
  // their shelf item properties have changed.
  class UserWindowObserver : public WmWindowObserver {
   public:
    explicit UserWindowObserver(ShelfWindowWatcher* window_watcher);
    ~UserWindowObserver() override;

   private:
    // WmWindowObserver:
    void OnWindowPropertyChanged(WmWindow* window,
                                 WmWindowProperty property) override;
    void OnWindowDestroying(WmWindow* window) override;

    ShelfWindowWatcher* window_watcher_;

    DISALLOW_COPY_AND_ASSIGN(UserWindowObserver);
  };

  // Creates a ShelfItem for |window|.
  void AddShelfItem(WmWindow* window);

  // Removes a ShelfItem for |window|.
  void RemoveShelfItem(WmWindow* window);

  // Updates the status of ShelfItem for |window|.
  void UpdateShelfItemStatus(WmWindow* window, bool is_active);

  // Returns the index of ShelfItem associated with |window|.
  int GetShelfItemIndexForWindow(WmWindow* window) const;

  // Cleans up observers on |container|.
  void OnContainerWindowDestroying(WmWindow* container);

  // Adds a shelf item for new windows added to the default container that have
  // a valid ShelfItemType property value.
  void OnUserWindowAdded(WmWindow* window);

  // Adds, updates or removes the shelf item based on a property change.
  void OnUserWindowPropertyChanged(WmWindow* window);

  // Removes the shelf item when a window closes.
  void OnUserWindowDestroying(WmWindow* window);

  // WmActivationObserver:
  void OnWindowActivated(WmWindow* gained_active,
                         WmWindow* lost_active) override;

  // display::DisplayObserver overrides:
  void OnDisplayAdded(const display::Display& display) override;
  void OnDisplayRemoved(const display::Display& old_display) override;
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t metrics) override;

  ShelfModel* model_;

  ContainerWindowObserver container_window_observer_;
  UserWindowObserver user_window_observer_;

  ScopedObserver<WmWindow, ContainerWindowObserver> observed_container_windows_;
  ScopedObserver<WmWindow, UserWindowObserver> observed_user_windows_;

  DISALLOW_COPY_AND_ASSIGN(ShelfWindowWatcher);
};

}  // namespace ash

#endif  // ASH_COMMON_SHELF_SHELF_WINDOW_WATCHER_H_
