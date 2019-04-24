// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desks_controller.h"

#include <utility>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desks_util.h"
#include "base/logging.h"

namespace ash {

DesksController::DesksController() {
  for (int id : desks_util::GetDesksContainersIds())
    available_container_ids_.push(id);

  // There's always one default desk.
  NewDesk();
  active_desk_ = desks_.back().get();
  active_desk_->Activate();
}

DesksController::~DesksController() = default;

// static
DesksController* DesksController::Get() {
  return Shell::Get()->desks_controller();
}

void DesksController::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void DesksController::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool DesksController::CanCreateDesks() const {
  // TODO(afakhry): Disable creating new desks in tablet mode.
  return desks_.size() < desks_util::kMaxNumberOfDesks;
}

bool DesksController::CanRemoveDesks() const {
  return desks_.size() > 1;
}

void DesksController::NewDesk() {
  DCHECK(CanCreateDesks());
  DCHECK(!available_container_ids_.empty());

  desks_.emplace_back(std::make_unique<Desk>(available_container_ids_.front()));
  available_container_ids_.pop();

  for (auto& observer : observers_)
    observer.OnDeskAdded(desks_.back().get());
}

void DesksController::RemoveDesk(const Desk* desk) {
  DCHECK(CanRemoveDesks());

  auto iter = std::find_if(
      desks_.begin(), desks_.end(),
      [desk](const std::unique_ptr<Desk>& d) { return d.get() == desk; });
  DCHECK(iter != desks_.end());

  // Keep the removed desk alive until the end of this function.
  std::unique_ptr<Desk> removed_desk = std::move(*iter);
  DCHECK_EQ(removed_desk.get(), desk);
  auto iter_after = desks_.erase(iter);

  DCHECK(!desks_.empty());

  // - Move windows in removed desk (if any) to the currently active desk.
  // - If the active desk is the one being removed, activate the desk to its
  //   left, if no desk to the left, activate one on the right.
  if (removed_desk.get() != active_desk_) {
    removed_desk->MoveWindowsToDesk(active_desk_);
  } else {
    Desk* target_desk = nullptr;
    if (iter_after == desks_.begin()) {
      // Nothing before this desk.
      target_desk = (*iter_after).get();
    } else {
      // Back up to select the desk on the left.
      target_desk = (*(--iter_after)).get();
    }

    DCHECK(target_desk);
    removed_desk->MoveWindowsToDesk(target_desk);
    ActivateDesk(target_desk);
  }

  for (auto& observer : observers_)
    observer.OnDeskRemoved(removed_desk.get());

  available_container_ids_.push(removed_desk->container_id());

  DCHECK_LE(available_container_ids_.size(), desks_util::kMaxNumberOfDesks);
}

void DesksController::ActivateDesk(const Desk* desk) {
  DCHECK(HasDesk(desk));

  if (desk == active_desk_)
    return;

  // Mark the new desk as active first, so that deactivating windows on the
  // `old_active` desk do not activate other windows on the same desk. See
  // `ash::IsWindowConsideredVisibleForActivation()`.
  Desk* old_active = active_desk_;
  active_desk_ = const_cast<Desk*>(desk);

  // There should always be an active desk at any time.
  DCHECK(old_active);
  old_active->Deactivate();
  active_desk_->Activate();

  // TODO(afakhry): Do desk activation animation.
}

void DesksController::OnRootWindowAdded(aura::Window* root_window) {
  for (auto& desk : desks_)
    desk->OnRootWindowAdded(root_window);
}

void DesksController::OnRootWindowClosing(aura::Window* root_window) {
  for (auto& desk : desks_)
    desk->OnRootWindowClosing(root_window);
}

bool DesksController::HasDesk(const Desk* desk) const {
  auto iter = std::find_if(
      desks_.begin(), desks_.end(),
      [desk](const std::unique_ptr<Desk>& d) { return d.get() == desk; });
  return iter != desks_.end();
}

}  // namespace ash
