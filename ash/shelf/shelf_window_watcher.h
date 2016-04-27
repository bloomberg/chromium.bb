// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_WINDOW_WATCHER_H_
#define ASH_SHELF_SHELF_WINDOW_WATCHER_H_

#include <stdint.h>

#include "ash/shelf/scoped_observer_with_duplicated_sources.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "ui/aura/window_observer.h"
#include "ui/display/display_observer.h"
#include "ui/wm/public/activation_change_observer.h"

namespace aura {

class Window;

namespace client {
class ActivationClient;
}

}  // namespace aura

namespace ash {

class ShelfModel;
class ShelfItemDelegateManager;

// ShelfWindowWatcher creates and handles a ShelfItem for windows that have
// a ShelfItemDetails property in the default container.
class ShelfWindowWatcher : public aura::client::ActivationChangeObserver,
                           public aura::WindowObserver,
                           public display::DisplayObserver {
 public:
  ShelfWindowWatcher(ShelfModel* model,
                     ShelfItemDelegateManager* item_delegate_manager);
  ~ShelfWindowWatcher() override;

 private:
  class RootWindowObserver : public aura::WindowObserver {
   public:
    explicit RootWindowObserver(ShelfWindowWatcher* window_watcher);
    ~RootWindowObserver() override;

   private:
    // aura::WindowObserver overrides:
    void OnWindowDestroying(aura::Window* window) override;

    // Owned by Shell.
    ShelfWindowWatcher* window_watcher_;

    DISALLOW_COPY_AND_ASSIGN(RootWindowObserver);
  };

  // Used to track windows that are removed. See description of
  // ShelfWindowWatcher::StartObservingRemovedWindow() for more details.
  class RemovedWindowObserver : public aura::WindowObserver {
   public:
    explicit RemovedWindowObserver(ShelfWindowWatcher* window_watcher);
    ~RemovedWindowObserver() override;

   private:
    // aura::WindowObserver overrides:
    void OnWindowParentChanged(aura::Window* window,
                               aura::Window* parent) override;
    void OnWindowDestroyed(aura::Window* window) override;

    // Owned by Shell.
    ShelfWindowWatcher* window_watcher_;

    DISALLOW_COPY_AND_ASSIGN(RemovedWindowObserver);
  };

  // Creates a ShelfItem for |window| that has ShelfItemDetails.
  void AddShelfItem(aura::Window* window);

  // Removes a ShelfItem for |window|.
  void RemoveShelfItem(aura::Window* window);

  // Adds observer to default container and ActivationClient of |root_window|.
  void OnRootWindowAdded(aura::Window* root_window);

  // Removes observer from ActivationClient of |root_window|.
  void OnRootWindowRemoved(aura::Window* root_window);

  // Updates the status of ShelfItem for |window|.
  void UpdateShelfItemStatus(aura::Window* window, bool is_active);

  // Returns the index of ShelfItem associated with |window|.
  int GetShelfItemIndexForWindow(aura::Window* window) const;

  // Used when a window is removed. During the dragging a window may be removed
  // and when the drag completes added back. When this happens we don't want to
  // remove the shelf item. StartObservingRemovedWindow, if necessary, attaches
  // an observer. When done, FinishObservingRemovedWindow() is invoked.
  void StartObservingRemovedWindow(aura::Window* window);

  // Stop observing |window| by RemovedWindowObserver and remove an item
  // associated with |window|.
  void FinishObservingRemovedWindow(aura::Window* window);

  // aura::client::ActivationChangeObserver overrides:
  void OnWindowActivated(
      aura::client::ActivationChangeObserver::ActivationReason reason,
      aura::Window* gained_active,
      aura::Window* lost_active) override;

  // aura::WindowObserver overrides:
  void OnWindowAdded(aura::Window* window) override;
  void OnWillRemoveWindow(aura::Window* window) override;
  void OnWindowDestroying(aura::Window* window) override;
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override;

  // display::DisplayObserver overrides:
  void OnDisplayAdded(const display::Display& display) override;
  void OnDisplayRemoved(const display::Display& old_display) override;
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t metrics) override;

  // Owned by Shell.
  ShelfModel* model_;
  ShelfItemDelegateManager* item_delegate_manager_;

  RootWindowObserver root_window_observer_;

  RemovedWindowObserver removed_window_observer_;

  // Holds all observed windows.
  ScopedObserver<aura::Window, aura::WindowObserver> observed_windows_;

  // Holds all observed root windows.
  ScopedObserver<aura::Window, aura::WindowObserver> observed_root_windows_;

  // Holds removed windows that has an item from default container.
  ScopedObserver<aura::Window, aura::WindowObserver> observed_removed_windows_;

  // Holds all observed activation clients.
  ScopedObserverWithDuplicatedSources<aura::client::ActivationClient,
      aura::client::ActivationChangeObserver> observed_activation_clients_;

  DISALLOW_COPY_AND_ASSIGN(ShelfWindowWatcher);
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_WINDOW_WATCHER_H_
