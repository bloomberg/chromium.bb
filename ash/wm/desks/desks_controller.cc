// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desks_controller.h"

#include <utility>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/desks/root_window_desk_switch_animator.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/window_util.h"
#include "base/auto_reset.h"
#include "base/logging.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/compositor.h"

namespace ash {

namespace {

// Appends the given |windows| to the end of the currently active overview mode
// session such that the most-recently used window is added first. If
// |should_animate| is true, the windows will animate to their positions in the
// overview grid.
void AppendWindowsToOverview(const base::flat_set<aura::Window*>& windows,
                             bool should_animate) {
  DCHECK(Shell::Get()->overview_controller()->IsSelecting());

  auto* overview_session =
      Shell::Get()->overview_controller()->overview_session();
  for (auto* window :
       Shell::Get()->mru_window_tracker()->BuildMruWindowList()) {
    if (!windows.contains(window) || wm::ShouldExcludeForOverview(window))
      continue;

    overview_session->AppendItem(window, /*reposition=*/true, should_animate);
  }
}

// Removes the given |windows| from the currently active overview mode session.
void RemoveWindowsFromOverview(const base::flat_set<aura::Window*>& windows) {
  DCHECK(Shell::Get()->overview_controller()->IsSelecting());

  auto* overview_session =
      Shell::Get()->overview_controller()->overview_session();
  for (auto* window : windows) {
    auto* item = overview_session->GetOverviewItemForWindow(window);
    if (item)
      overview_session->RemoveItem(item);
  }
}

}  // namespace

DesksController::DesksController() {
  for (int id : desks_util::GetDesksContainersIds())
    available_container_ids_.push(id);

  // There's always one default desk.
  NewDesk();
  active_desk_ = desks_.back().get();
  active_desk_->Activate(/*update_window_activation=*/true);
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

bool DesksController::AreDesksBeingModified() const {
  return are_desks_being_modified_ || !desk_switch_animators_.empty();
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

  base::AutoReset<bool> in_progress(&are_desks_being_modified_, true);

  desks_.emplace_back(std::make_unique<Desk>(available_container_ids_.front()));
  available_container_ids_.pop();

  for (auto& observer : observers_)
    observer.OnDeskAdded(desks_.back().get());
}

void DesksController::RemoveDesk(const Desk* desk) {
  DCHECK(CanRemoveDesks());

  base::AutoReset<bool> in_progress(&are_desks_being_modified_, true);

  auto iter = std::find_if(
      desks_.begin(), desks_.end(),
      [desk](const std::unique_ptr<Desk>& d) { return d.get() == desk; });
  DCHECK(iter != desks_.end());

  // Keep the removed desk alive until the end of this function.
  std::unique_ptr<Desk> removed_desk = std::move(*iter);
  DCHECK_EQ(removed_desk.get(), desk);
  auto iter_after = desks_.erase(iter);

  DCHECK(!desks_.empty());

  const bool is_selecting = Shell::Get()->overview_controller()->IsSelecting();
  const base::flat_set<aura::Window*> removed_desk_windows =
      removed_desk->windows();

  // - Move windows in removed desk (if any) to the currently active desk.
  // - If the active desk is the one being removed, activate the desk to its
  //   left, if no desk to the left, activate one on the right.
  if (removed_desk.get() != active_desk_) {
    removed_desk->MoveWindowsToDesk(active_desk_);

    // If overview mode is active, we add the windows of the removed desk to the
    // overview grid in the order of their MRU. Note that this can only be done
    // after the windows have moved to the active desk above, so that building
    // the window MRU list should contain those windows.
    if (is_selecting)
      AppendWindowsToOverview(removed_desk_windows, /*should_animate=*/true);
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

    // The removed desk is the active desk, so temporarily remove its windows
    // from the overview grid which will result in removing the
    // "OverviewModeLabel" widgets created by overview mode for these windows.
    // This way the removed desk tracks only real windows, which are now ready
    // to be moved to the target desk.
    if (is_selecting)
      RemoveWindowsFromOverview(removed_desk_windows);

    // If overview mode is active, change desk activation without changing
    // window activation. Activation should remain on the dummy
    // "OverviewModeFocusedWidget" while overview mode is active.
    removed_desk->MoveWindowsToDesk(target_desk);
    ActivateDeskInternal(target_desk,
                         /*update_window_activation=*/!is_selecting);

    // Now that the windows from the removed and target desks merged, add them
    // all without animation to the grid in the order of their MRU.
    if (is_selecting)
      AppendWindowsToOverview(target_desk->windows(), /*should_animate=*/false);
  }

  for (auto& observer : observers_)
    observer.OnDeskRemoved(removed_desk.get());

  available_container_ids_.push(removed_desk->container_id());

  DCHECK_LE(available_container_ids_.size(), desks_util::kMaxNumberOfDesks);
}

void DesksController::ActivateDesk(const Desk* desk) {
  DCHECK(HasDesk(desk));

  if (desk == active_desk_) {
    OverviewController* overview_controller =
        Shell::Get()->overview_controller();
    if (overview_controller->IsSelecting()) {
      // Selecting the active desk's mini_view in overview mode is allowed and
      // should just exit overview mode normally.
      overview_controller->ToggleOverview();
    }
    return;
  }

  // New desks are always added at the end of the list to the right of existing
  // desks. Therefore, desks at lower indices are located on the left of desks
  // with higher indices.
  const bool move_left = GetDeskIndex(active_desk_) < GetDeskIndex(desk);
  for (auto* root : Shell::GetAllRootWindows()) {
    desk_switch_animators_.emplace_back(
        std::make_unique<RootWindowDeskSwitchAnimator>(root, desk, this,
                                                       move_left));
  }

  // Once all animators are created, start them all by taking the starting desk
  // screenshots. This is to avoid any potential race conditions that might
  // happen if one animator finished phase (1) of the animation while other
  // animators are still being constructed.
  for (auto& animator : desk_switch_animators_)
    animator->TakeStartingDeskScreenshot();
}

void DesksController::OnRootWindowAdded(aura::Window* root_window) {
  for (auto& desk : desks_)
    desk->OnRootWindowAdded(root_window);
}

void DesksController::OnRootWindowClosing(aura::Window* root_window) {
  for (auto& desk : desks_)
    desk->OnRootWindowClosing(root_window);
}

void DesksController::OnStartingDeskScreenshotTaken(const Desk* ending_desk) {
  DCHECK(!desk_switch_animators_.empty());

  // Once all starting desk screenshots on all roots are taken and placed on the
  // screens, do the actual desk activation logic.
  for (const auto& animator : desk_switch_animators_) {
    if (!animator->starting_desk_screenshot_taken())
      return;
  }

  // Extend the compositors' timeouts in order to prevents any repaints until
  // the desks are switched and overview mode exits.
  const auto roots = Shell::GetAllRootWindows();
  for (auto* root : roots)
    root->GetHost()->compositor()->SetAllowLocksToExtendTimeout(true);

  const bool is_selecting = Shell::Get()->overview_controller()->IsSelecting();

  ActivateDeskInternal(ending_desk, /*update_window_activation=*/true);

  // Activating a desk should not change the overview mode state.
  DCHECK_EQ(is_selecting, Shell::Get()->overview_controller()->IsSelecting());

  if (is_selecting) {
    // Exit overview mode immediately without any animations before taking the
    // ending desk screenshot. This makes sure that the ending desk
    // screenshot will only show the windows in that desk, not overview stuff.
    Shell::Get()->overview_controller()->ToggleOverview(
        OverviewSession::EnterExitOverviewType::kImmediateExit);
  }

  for (auto* root : roots)
    root->GetHost()->compositor()->SetAllowLocksToExtendTimeout(false);

  // Continue the second phase of the animation by taking the ending desk
  // screenshot and actually animating the layers.
  for (auto& animator : desk_switch_animators_)
    animator->TakeEndingDeskScreenshot();
}

void DesksController::OnEndingDeskScreenshotTaken() {
  DCHECK(!desk_switch_animators_.empty());

  // Once all ending desk screenshots on all roots are taken, start the
  // animation on all roots at the same time, so that they look synchrnoized.
  for (const auto& animator : desk_switch_animators_) {
    if (!animator->ending_desk_screenshot_taken())
      return;
  }

  for (auto& animator : desk_switch_animators_)
    animator->StartAnimation();
}

void DesksController::OnDeskSwitchAnimationFinished() {
  DCHECK(!desk_switch_animators_.empty());

  // Once all desk switch animations on all roots finish, destroy all the
  // animators.
  for (const auto& animator : desk_switch_animators_) {
    if (!animator->animation_finished())
      return;
  }

  desk_switch_animators_.clear();

  for (auto& observer : observers_)
    observer.OnDeskSwitchAnimationFinished();
}

bool DesksController::HasDesk(const Desk* desk) const {
  auto iter = std::find_if(
      desks_.begin(), desks_.end(),
      [desk](const std::unique_ptr<Desk>& d) { return d.get() == desk; });
  return iter != desks_.end();
}

int DesksController::GetDeskIndex(const Desk* desk) const {
  for (size_t i = 0; i < desks_.size(); ++i) {
    if (desk == desks_[i].get())
      return i;
  }

  NOTREACHED();
  return -1;
}

void DesksController::ActivateDeskInternal(const Desk* desk,
                                           bool update_window_activation) {
  DCHECK(HasDesk(desk));

  if (desk == active_desk_)
    return;

  base::AutoReset<bool> in_progress(&are_desks_being_modified_, true);

  // Mark the new desk as active first, so that deactivating windows on the
  // `old_active` desk do not activate other windows on the same desk. See
  // `ash::IsWindowConsideredVisibleForActivation()`.
  Desk* old_active = active_desk_;
  active_desk_ = const_cast<Desk*>(desk);

  // There should always be an active desk at any time.
  DCHECK(old_active);
  old_active->Deactivate(update_window_activation);
  active_desk_->Activate(update_window_activation);

  for (auto& observer : observers_)
    observer.OnDeskActivationChanged(active_desk_, old_active);
}

}  // namespace ash
