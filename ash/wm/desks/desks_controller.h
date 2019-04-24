// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_DESKS_CONTROLLER_H_
#define ASH_WM_DESKS_DESKS_CONTROLLER_H_

#include <memory>
#include <queue>
#include <vector>

#include "ash/ash_export.h"
#include "base/macros.h"
#include "base/observer_list.h"

namespace aura {
class Window;
}  // namespace aura

namespace ash {

class Desk;

// Defines a controller for creating, destroying and managing virtual desks and
// their windows.
class ASH_EXPORT DesksController {
 public:
  class Observer {
   public:
    // Called when |desk| has been created and added to
    // `DesksController::desks_`.
    virtual void OnDeskAdded(const Desk* desk) = 0;

    // Called when |desk| has been removed from `DesksController::desks_`.
    // However |desk| is kept alive temporarily and will be destroyed after all
    // observers have been notified with this.
    virtual void OnDeskRemoved(const Desk* desk) = 0;

   protected:
    virtual ~Observer() = default;
  };

  DesksController();
  ~DesksController();

  // Convenience method for returning the DesksController instance. The actual
  // instance is created and owned by Shell.
  static DesksController* Get();

  const std::vector<std::unique_ptr<Desk>>& desks() const { return desks_; }

  const Desk* active_desk() const { return active_desk_; }

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Returns true if we haven't reached the maximum allowed number of desks.
  bool CanCreateDesks() const;

  // Returns true as long as there are two or more desks. It is required that
  // there is at least one single desk at any time.
  bool CanRemoveDesks() const;

  // Creates a new desk. CanCreateDesks() must be checked before calling this.
  void NewDesk();

  // Removes and deletes the given |desk|. |desk| must already exist, and
  // CanRemoveDesks() must be checked before this.
  void RemoveDesk(const Desk* desk);

  // Activates the given |desk| and deactivates the currently active one. |desk|
  // has to be an existing desk.
  void ActivateDesk(const Desk* desk);

  // Called explicitly by the RootWindowController when a root window has been
  // added or about to be removed in order to update all the available desks.
  void OnRootWindowAdded(aura::Window* root_window);
  void OnRootWindowClosing(aura::Window* root_window);

 private:
  bool HasDesk(const Desk* desk) const;

  std::vector<std::unique_ptr<Desk>> desks_;

  Desk* active_desk_ = nullptr;

  // A free list of desk container IDs to be used for newly-created desks. New
  // desks pops from this queue and removed desks's associated container IDs are
  // re-pushed on this queue.
  std::queue<int> available_container_ids_;

  base::ObserverList<Observer>::Unchecked observers_;

  DISALLOW_COPY_AND_ASSIGN(DesksController);
};

}  // namespace ash

#endif  // ASH_WM_DESKS_DESKS_CONTROLLER_H_
