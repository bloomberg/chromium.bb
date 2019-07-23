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
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ash/wm/window_util.h"
#include "base/auto_reset.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/compositor.h"
#include "ui/wm/public/activation_client.h"

namespace ash {

namespace {

constexpr char kNewDeskHistogramName[] = "Ash.Desks.NewDesk";
constexpr char kDesksCountHistogramName[] = "Ash.Desks.DesksCount";
constexpr char kRemoveDeskHistogramName[] = "Ash.Desks.RemoveDesk";
constexpr char kDeskSwitchHistogramName[] = "Ash.Desks.DesksSwitch";
constexpr char kMoveWindowFromActiveDeskHistogramName[] =
    "Ash.Desks.MoveWindowFromActiveDesk";
constexpr char kNumberOfWindowsOnDesk_1_HistogramName[] =
    "Ash.Desks.NumberOfWindowsOnDesk_1";
constexpr char kNumberOfWindowsOnDesk_2_HistogramName[] =
    "Ash.Desks.NumberOfWindowsOnDesk_2";
constexpr char kNumberOfWindowsOnDesk_3_HistogramName[] =
    "Ash.Desks.NumberOfWindowsOnDesk_3";
constexpr char kNumberOfWindowsOnDesk_4_HistogramName[] =
    "Ash.Desks.NumberOfWindowsOnDesk_4";

// Appends the given |windows| to the end of the currently active overview mode
// session such that the most-recently used window is added first. If
// |should_animate| is true, the windows will animate to their positions in the
// overview grid.
void AppendWindowsToOverview(const std::vector<aura::Window*>& windows,
                             bool should_animate) {
  DCHECK(Shell::Get()->overview_controller()->InOverviewSession());

  auto* overview_session =
      Shell::Get()->overview_controller()->overview_session();
  for (auto* window :
       Shell::Get()->mru_window_tracker()->BuildMruWindowList(kActiveDesk)) {
    if (!base::Contains(windows, window) ||
        wm::ShouldExcludeForOverview(window)) {
      continue;
    }

    overview_session->AppendItem(window, /*reposition=*/true, should_animate);
  }
}

// Removes the given |windows| from the currently active overview mode session.
void RemoveWindowsFromOverview(const base::flat_set<aura::Window*>& windows) {
  DCHECK(Shell::Get()->overview_controller()->InOverviewSession());

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
  Shell::Get()->activation_client()->AddObserver(this);

  for (int id : desks_util::GetDesksContainersIds())
    available_container_ids_.push(id);

  // There's always one default desk.
  NewDesk(DesksCreationRemovalSource::kButton);
  active_desk_ = desks_.back().get();
  active_desk_->Activate(/*update_window_activation=*/true);
}

DesksController::~DesksController() {
  Shell::Get()->activation_client()->RemoveObserver(this);
}

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
  return desks_.size() < desks_util::kMaxNumberOfDesks;
}

Desk* DesksController::GetNextDesk() const {
  int next_index = GetDeskIndex(active_desk_);
  if (++next_index >= static_cast<int>(desks_.size()))
    return nullptr;
  return desks_[next_index].get();
}

Desk* DesksController::GetPreviousDesk() const {
  int previous_index = GetDeskIndex(active_desk_);
  if (--previous_index < 0)
    return nullptr;
  return desks_[previous_index].get();
}

bool DesksController::CanRemoveDesks() const {
  return desks_.size() > 1;
}

void DesksController::NewDesk(DesksCreationRemovalSource source) {
  DCHECK(CanCreateDesks());
  DCHECK(!available_container_ids_.empty());

  base::AutoReset<bool> in_progress(&are_desks_being_modified_, true);

  desks_.emplace_back(std::make_unique<Desk>(available_container_ids_.front()));
  available_container_ids_.pop();

  UMA_HISTOGRAM_ENUMERATION(kNewDeskHistogramName, source);
  ReportDesksCountHistogram();

  for (auto& observer : observers_)
    observer.OnDeskAdded(desks_.back().get());
}

void DesksController::RemoveDesk(const Desk* desk,
                                 DesksCreationRemovalSource source) {
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

  auto* overview_controller = Shell::Get()->overview_controller();
  const bool in_overview = overview_controller->InOverviewSession();
  const std::vector<aura::Window*> removed_desk_windows =
      removed_desk->windows();

  // No need to spend time refreshing the mini_views of the removed desk.
  auto removed_desk_mini_views_pauser =
      removed_desk->GetScopedNotifyContentChangedDisabler();

  // - Move windows in removed desk (if any) to the currently active desk.
  // - If the active desk is the one being removed, activate the desk to its
  //   left, if no desk to the left, activate one on the right.
  const bool will_switch_desks = (removed_desk.get() == active_desk_);
  if (!will_switch_desks) {
    // We will refresh the mini_views of the active desk only once at the end.
    auto active_desk_mini_view_pauser =
        active_desk_->GetScopedNotifyContentChangedDisabler();

    removed_desk->MoveWindowsToDesk(active_desk_);

    // If overview mode is active, we add the windows of the removed desk to the
    // overview grid in the order of their MRU. Note that this can only be done
    // after the windows have moved to the active desk above, so that building
    // the window MRU list should contain those windows.
    if (in_overview)
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

    // The target desk, which is about to become active, will have its
    // mini_views refreshed at the end.
    auto target_desk_mini_view_pauser =
        target_desk->GetScopedNotifyContentChangedDisabler();

    // Exit split view if active, before activating the new desk. We will
    // restore the split view state of the newly activated desk at the end.
    SplitViewController* split_view_controller =
        Shell::Get()->split_view_controller();
    split_view_controller->EndSplitView(
        SplitViewController::EndReason::kDesksChange);

    // The removed desk is the active desk, so temporarily remove its windows
    // from the overview grid which will result in removing the
    // "OverviewModeLabel" widgets created by overview mode for these windows.
    // This way the removed desk tracks only real windows, which are now ready
    // to be moved to the target desk.
    if (in_overview)
      RemoveWindowsFromOverview(removed_desk_windows);

    // If overview mode is active, change desk activation without changing
    // window activation. Activation should remain on the dummy
    // "OverviewModeFocusedWidget" while overview mode is active.
    removed_desk->MoveWindowsToDesk(target_desk);
    ActivateDesk(target_desk, DesksSwitchSource::kDeskRemoved);

    // Desk activation should not change overview mode state.
    DCHECK_EQ(in_overview, overview_controller->InOverviewSession());

    // Now that the windows from the removed and target desks merged, add them
    // all without animation to the grid in the order of their MRU.
    if (in_overview)
      AppendWindowsToOverview(target_desk->windows(), /*should_animate=*/false);
  }

  // It's OK now to refresh the mini_views of *only* the active desk, and only
  // if windows from the removed desk moved to it.
  DCHECK(active_desk_->should_notify_content_changed());
  if (!removed_desk_windows.empty())
    active_desk_->NotifyContentChanged();

  for (auto& observer : observers_)
    observer.OnDeskRemoved(removed_desk.get());

  available_container_ids_.push(removed_desk->container_id());

  // Avoid having stale backdrop state as a desk is removed while in overview
  // mode, since the backdrop controller won't update the backdrop window as
  // the removed desk's windows move out from the container. Therefore, we need
  // to update it manually.
  if (in_overview)
    removed_desk->UpdateDeskBackdrops();

  // Restoring split view may start or end overview mode, therefore do this at
  // the end to avoid getting into a bad state.
  if (will_switch_desks)
    MaybeRestoreSplitView(/*refresh_snapped_windows=*/true);

  UMA_HISTOGRAM_ENUMERATION(kRemoveDeskHistogramName, source);
  ReportDesksCountHistogram();
  ReportNumberOfWindowsPerDeskHistogram();

  DCHECK_LE(available_container_ids_.size(), desks_util::kMaxNumberOfDesks);
}

void DesksController::ActivateDesk(const Desk* desk, DesksSwitchSource source) {
  DCHECK(HasDesk(desk));

  UMA_HISTOGRAM_ENUMERATION(kDeskSwitchHistogramName, source);

  OverviewController* overview_controller = Shell::Get()->overview_controller();
  const bool in_overview = overview_controller->InOverviewSession();
  if (desk == active_desk_) {
    if (in_overview) {
      // Selecting the active desk's mini_view in overview mode is allowed and
      // should just exit overview mode normally.
      overview_controller->EndOverview();
    }
    return;
  }

  if (source == DesksSwitchSource::kDeskRemoved) {
    ActivateDeskInternal(desk, /*update_window_activation=*/!in_overview);
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

void DesksController::MoveWindowFromActiveDeskTo(
    aura::Window* window,
    Desk* target_desk,
    DesksMoveWindowFromActiveDeskSource source) {
  DCHECK_NE(active_desk_, target_desk);

  base::AutoReset<bool> in_progress(&are_desks_being_modified_, true);

  auto* overview_controller = Shell::Get()->overview_controller();
  const bool in_overview = overview_controller->InOverviewSession();

  active_desk_->MoveWindowToDesk(window, target_desk);

  if (in_overview) {
    DCHECK(overview_controller->InOverviewSession());
    auto* overview_session = overview_controller->overview_session();
    auto* item = overview_session->GetOverviewItemForWindow(window);
    DCHECK(item);
    // Restore the dragged item window, so that its transform is reset to
    // identity.
    item->RestoreWindow(/*reset_transform=*/true);
    // The item no longer needs to be in the overview grid.
    overview_session->RemoveItem(item);
    // When in overview, we should return immediately and not change the window
    // activation as we do below, since the dummy "OverviewModeFocusedWidget"
    // should remain active while overview mode is active..
    return;
  }

  UMA_HISTOGRAM_ENUMERATION(kMoveWindowFromActiveDeskHistogramName, source);
  ReportNumberOfWindowsPerDeskHistogram();

  // A window moving out of the active desk cannot be active.
  wm::DeactivateWindow(window);
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

  // The order here matters. Overview must end before ending split view before
  // switching desks. That's because we don't want TabletModeWindowManager
  // maximizing all windows because we cleared the snapped ones in
  // split_view_controller first. See:
  // `TabletModeWindowManager::OnOverviewModeEndingAnimationComplete()`.
  // See also test coverage for this case in:
  // `TabletModeDesksTest.SnappedStateRetainedOnSwitchingDesksFromOverview`.
  const bool in_overview =
      Shell::Get()->overview_controller()->InOverviewSession();
  if (in_overview) {
    // Exit overview mode immediately without any animations before taking the
    // ending desk screenshot. This makes sure that the ending desk
    // screenshot will only show the windows in that desk, not overview stuff.
    Shell::Get()->overview_controller()->EndOverview(
        OverviewSession::EnterExitOverviewType::kImmediateExit);
  }
  SplitViewController* split_view_controller =
      Shell::Get()->split_view_controller();
  split_view_controller->EndSplitView(
      SplitViewController::EndReason::kDesksChange);

  ActivateDeskInternal(ending_desk, /*update_window_activation=*/true);

  MaybeRestoreSplitView(/*refresh_snapped_windows=*/true);

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

void DesksController::OnWindowActivating(ActivationReason reason,
                                         aura::Window* gaining_active,
                                         aura::Window* losing_active) {
  if (AreDesksBeingModified())
    return;

  if (!gaining_active)
    return;

  const Desk* window_desk = FindDeskOfWindow(gaining_active);
  if (!window_desk || window_desk == active_desk_)
    return;

  ActivateDesk(window_desk, DesksSwitchSource::kWindowActivated);
}

void DesksController::OnWindowActivated(ActivationReason reason,
                                        aura::Window* gained_active,
                                        aura::Window* lost_active) {}

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
  // `ash::AshFocusRules::GetNextActivatableWindow()`.
  Desk* old_active = active_desk_;
  active_desk_ = const_cast<Desk*>(desk);

  // There should always be an active desk at any time.
  DCHECK(old_active);
  old_active->Deactivate(update_window_activation);
  active_desk_->Activate(update_window_activation);

  for (auto& observer : observers_)
    observer.OnDeskActivationChanged(active_desk_, old_active);
}

const Desk* DesksController::FindDeskOfWindow(aura::Window* window) const {
  DCHECK(window);

  for (const auto& desk : desks_) {
    if (base::Contains(desk->windows(), window))
      return desk.get();
  }

  return nullptr;
}

void DesksController::ReportNumberOfWindowsPerDeskHistogram() const {
  for (size_t i = 0; i < desks_.size(); ++i) {
    const size_t windows_count = desks_[i]->windows().size();
    switch (i) {
      case 0:
        UMA_HISTOGRAM_COUNTS_100(kNumberOfWindowsOnDesk_1_HistogramName,
                                 windows_count);
        break;

      case 1:
        UMA_HISTOGRAM_COUNTS_100(kNumberOfWindowsOnDesk_2_HistogramName,
                                 windows_count);
        break;

      case 2:
        UMA_HISTOGRAM_COUNTS_100(kNumberOfWindowsOnDesk_3_HistogramName,
                                 windows_count);
        break;

      case 3:
        UMA_HISTOGRAM_COUNTS_100(kNumberOfWindowsOnDesk_4_HistogramName,
                                 windows_count);
        break;

      default:
        NOTREACHED();
        break;
    }
  }
}

void DesksController::ReportDesksCountHistogram() const {
  DCHECK_LE(desks_.size(), desks_util::kMaxNumberOfDesks);
  UMA_HISTOGRAM_EXACT_LINEAR(kDesksCountHistogramName, desks_.size(),
                             desks_util::kMaxNumberOfDesks);
}

}  // namespace ash
