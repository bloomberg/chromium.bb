// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_OVERVIEW_CONTROLLER_H_
#define ASH_WM_OVERVIEW_OVERVIEW_CONTROLLER_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "ash/wm/overview/overview_delegate.h"
#include "ash/wm/overview/overview_session.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "ui/aura/window_occlusion_tracker.h"

namespace ash {

// Manages a overview session which displays an overview of all windows and
// allows selecting a window to activate it.
class ASH_EXPORT OverviewController : public OverviewDelegate,
                                      public ::wm::ActivationChangeObserver {
 public:
  enum class AnimationCompleteReason {
    kCompleted,
    kCanceled,
  };

  OverviewController();
  ~OverviewController() override;

  // Returns true if selecting windows in an overview is enabled. This is false
  // at certain times, such as when the lock screen is visible.
  static bool CanSelect();

  // Attempts to toggle overview mode and returns true if successful (showing
  // overview would be unsuccessful if there are no windows to show). Depending
  // on |type| the enter/exit animation will look different.
  bool ToggleOverview(OverviewSession::EnterExitOverviewType type =
                          OverviewSession::EnterExitOverviewType::kNormal);

  // Returns true if window selection mode is active.
  bool IsSelecting() const;

  // Returns true if overview has been shutdown, but is still animating to the
  // end state ui.
  bool IsCompletingShutdownAnimations();

  // Moves the current selection by |increment| items. Positive values of
  // |increment| move the selection forward, negative values move it backward.
  void IncrementSelection(int increment);

  // Accepts current selection if any. Returns true if a selection was made,
  // false otherwise.
  bool AcceptSelection();

  // Called when the overview button tray has been long pressed. Enters
  // splitview mode if the active window is snappable. Also enters overview mode
  // if device is not currently in overview mode.
  void OnOverviewButtonTrayLongPressed(const gfx::Point& event_location);

  // Gets the windows list that are shown in the overview windows grids if the
  // overview mode is active for testing.
  std::vector<aura::Window*> GetWindowsListInOverviewGridsForTesting();

  // OverviewDelegate:
  void OnSelectionEnded() override;
  void AddDelayedAnimationObserver(
      std::unique_ptr<DelayedAnimationObserver> animation) override;
  void RemoveAndDestroyAnimationObserver(
      DelayedAnimationObserver* animation) override;
  void AddStartAnimationObserver(
      std::unique_ptr<DelayedAnimationObserver> animation_observer) override;
  void RemoveAndDestroyStartAnimationObserver(
      DelayedAnimationObserver* animation_observer) override;

  // ::wm::ActivationChangeObserver:
  void OnWindowActivating(ActivationReason reason,
                          aura::Window* gained_active,
                          aura::Window* lost_active) override;
  void OnWindowActivated(ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override {}
  void OnAttemptToReactivateWindow(aura::Window* request_active,
                                   aura::Window* actual_active) override;

  OverviewSession* overview_session() { return overview_session_.get(); }

  void set_occlusion_pause_duration_for_end_ms_for_test(int duration) {
    occlusion_pause_duration_for_end_ms_ = duration;
  }

  // Returns wallpaper blur status for testing.
  bool HasBlurForTest() const;
  bool HasBlurAnimationForTest() const;

 private:
  class OverviewBlurController;
  friend class OverviewSessionTest;
  FRIEND_TEST_ALL_PREFIXES(TabletModeControllerTest,
                           DisplayDisconnectionDuringOverview);

  // There is no need to blur or unblur the wallpaper for tests.
  static void SetDoNotChangeWallpaperBlurForTests();

  // Dispatched when window selection begins.
  void OnSelectionStarted();

  void OnStartingAnimationComplete(bool canceled);
  void OnEndingAnimationComplete(bool canceled);
  void ResetPauser();

  // Collection of DelayedAnimationObserver objects that own widgets that may be
  // still animating after overview mode ends. If shell needs to shut down while
  // those animations are in progress, the animations are shut down and the
  // widgets destroyed.
  std::vector<std::unique_ptr<DelayedAnimationObserver>> delayed_animations_;
  // Collection of DelayedAnimationObserver objects. When this becomes empty,
  // notify shell that the starting animations have been completed.
  std::vector<std::unique_ptr<DelayedAnimationObserver>> start_animations_;

  std::unique_ptr<aura::WindowOcclusionTracker::ScopedPause>
      occlusion_tracker_pauser_;

  std::unique_ptr<OverviewSession> overview_session_;
  base::Time last_selection_time_;

  int occlusion_pause_duration_for_end_ms_;

  // Handles blurring of the wallpaper when entering or exiting overview mode.
  // Animates the blurring if necessary.
  std::unique_ptr<OverviewBlurController> overview_blur_controller_;

  base::CancelableOnceClosure reset_pauser_task_;

  base::WeakPtrFactory<OverviewController> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(OverviewController);
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_OVERVIEW_CONTROLLER_H_
