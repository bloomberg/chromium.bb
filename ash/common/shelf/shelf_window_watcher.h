// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SHELF_SHELF_WINDOW_WATCHER_H_
#define ASH_COMMON_SHELF_SHELF_WINDOW_WATCHER_H_

#include <set>

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "ui/aura/window_observer.h"
#include "ui/display/display_observer.h"
#include "ui/wm/public/activation_change_observer.h"

namespace ash {

class ShelfModel;

// ShelfWindowWatcher creates and handles a ShelfItem for windows in the default
// container and panels in the panel container that have a valid ShelfItemType
// property (e.g. the task manager dialog or the OS settings window). It adds
// the ShelfItem when the window is added to the default container and maintains
// it until the window is closed, even if the window is transiently reparented
// (e.g. during a drag).
class ShelfWindowWatcher : public aura::client::ActivationChangeObserver,
                           public display::DisplayObserver {
 public:
  explicit ShelfWindowWatcher(ShelfModel* model);
  ~ShelfWindowWatcher() override;

 private:
  // Observes for windows being added to a root window's default container.
  class ContainerWindowObserver : public aura::WindowObserver {
   public:
    explicit ContainerWindowObserver(ShelfWindowWatcher* window_watcher);
    ~ContainerWindowObserver() override;

   private:
    // aura::WindowObserver:
    void OnWindowHierarchyChanged(const HierarchyChangeParams& params) override;
    void OnWindowDestroying(aura::Window* window) override;

    ShelfWindowWatcher* window_watcher_;

    DISALLOW_COPY_AND_ASSIGN(ContainerWindowObserver);
  };

  // Observes individual user windows to detect when they are closed or when
  // their shelf item properties have changed.
  class UserWindowObserver : public aura::WindowObserver {
   public:
    explicit UserWindowObserver(ShelfWindowWatcher* window_watcher);
    ~UserWindowObserver() override;

   private:
    // aura::WindowObserver:
    void OnWindowPropertyChanged(aura::Window* window,
                                 const void* key,
                                 intptr_t old) override;
    void OnWindowDestroying(aura::Window* window) override;
    void OnWindowVisibilityChanged(aura::Window* window, bool visible) override;
    void OnWindowTitleChanged(aura::Window* window) override;

    ShelfWindowWatcher* window_watcher_;

    DISALLOW_COPY_AND_ASSIGN(UserWindowObserver);
  };

  // Creates a ShelfItem for |window|.
  void AddShelfItem(aura::Window* window);

  // Removes a ShelfItem for |window|.
  void RemoveShelfItem(aura::Window* window);

  // Returns the index of ShelfItem associated with |window|, or -1 if none.
  int GetShelfItemIndexForWindow(aura::Window* window) const;

  // Cleans up observers on |container|.
  void OnContainerWindowDestroying(aura::Window* container);

  // Adds a shelf item for new windows added to the default container that have
  // a valid ShelfItemType property value.
  void OnUserWindowAdded(aura::Window* window);

  // Adds, updates or removes the shelf item based on a property change.
  void OnUserWindowPropertyChanged(aura::Window* window);

  // Removes the shelf item when a window closes.
  void OnUserWindowDestroying(aura::Window* window);

  // aura::client::ActivationChangeObserver:
  void OnWindowActivated(ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override;

  // display::DisplayObserver overrides:
  void OnDisplayAdded(const display::Display& display) override;
  void OnDisplayRemoved(const display::Display& old_display) override;
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t metrics) override;

  ShelfModel* model_;

  ContainerWindowObserver container_window_observer_;
  UserWindowObserver user_window_observer_;

  ScopedObserver<aura::Window, ContainerWindowObserver>
      observed_container_windows_;
  ScopedObserver<aura::Window, UserWindowObserver> observed_user_windows_;

  // The set of windows with shelf items managed by this ShelfWindowWatcher.
  std::set<aura::Window*> user_windows_with_items_;

  DISALLOW_COPY_AND_ASSIGN(ShelfWindowWatcher);
};

}  // namespace ash

#endif  // ASH_COMMON_SHELF_SHELF_WINDOW_WATCHER_H_
