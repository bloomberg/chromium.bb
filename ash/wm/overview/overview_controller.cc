// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/overview_controller.h"

#include <algorithm>

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/root_window_controller.h"
#include "ash/scoped_animation_disabler.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/wallpaper/wallpaper_controller.h"
#include "ash/wallpaper/wallpaper_view.h"
#include "ash/wallpaper/wallpaper_widget_controller.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_constants.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/root_window_finder.h"
#include "ash/wm/screen_pinning_controller.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/bind.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/stl_util.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/wm/core/window_util.h"
#include "ui/wm/public/activation_client.h"

namespace ash {

namespace {

// Do not blur or unblur the wallpaper when entering or exiting overview mode
// when this is true.
bool g_disable_wallpaper_blur_for_tests = false;

constexpr int kBlurSlideDurationMs = 250;

// It can take up to two frames until the frame created in the UI thread that
// triggered animation observer is drawn. Wait 50ms in attempt to let its draw
// and swap finish.
constexpr int kOcclusionPauseDurationForStartMs = 50;

// Wait longer when exiting overview mode in case when a user may re-enter
// overview mode immediately, contents are ready.
constexpr int kOcclusionPauseDurationForEndMs = 500;

bool IsBlurAllowed() {
  return !g_disable_wallpaper_blur_for_tests &&
         Shell::Get()->wallpaper_controller()->IsBlurAllowed();
}

// Returns whether overview mode items should be slid in or out from the top of
// the screen.
bool ShouldSlideInOutOverview(const std::vector<aura::Window*>& windows) {
  // No sliding if home launcher is not available.
  if (!Shell::Get()
           ->tablet_mode_controller()
           ->IsTabletModeWindowManagerEnabled()) {
    return false;
  }

  if (windows.empty())
    return false;

  // Only slide in if all windows are minimized.
  for (const aura::Window* window : windows) {
    if (!wm::GetWindowState(window)->IsMinimized())
      return false;
  }

  return true;
}

}  // namespace

// Class that handles of blurring and dimming wallpaper upon entering and
// exiting overview mode. Blurs the wallpaper automatically if the wallpaper is
// not visible prior to entering overview mode (covered by a window), otherwise
// animates the blur and dim.
// TODO(sammiequon): Rename since this dims as well now.
class OverviewController::OverviewBlurController
    : public ui::CompositorAnimationObserver,
      public aura::WindowObserver {
 public:
  OverviewBlurController() = default;

  ~OverviewBlurController() override {
    if (compositor_)
      compositor_->RemoveAnimationObserver(this);
    for (aura::Window* root : roots_to_animate_)
      root->RemoveObserver(this);
  }

  void Blur(bool animate_only) {
    OnBlurChange(WallpaperAnimationState::kAddingBlur, animate_only);
  }

  void Unblur() {
    OnBlurChange(WallpaperAnimationState::kRemovingBlur,
                 /*animate_only=*/false);
  }

  bool has_blur() const { return state_ != WallpaperAnimationState::kNormal; }

  bool has_blur_animation() const { return !!compositor_; }

 private:
  enum class WallpaperAnimationState {
    kAddingBlur,
    kRemovingBlur,
    kNormal,
  };

  void OnAnimationStep(base::TimeTicks timestamp) override {
    if (start_time_ == base::TimeTicks()) {
      start_time_ = timestamp;
      return;
    }
    const float progress = (timestamp - start_time_).InMilliseconds() /
                           static_cast<float>(kBlurSlideDurationMs);
    const bool adding = state_ == WallpaperAnimationState::kAddingBlur;
    if (progress > 1.0f) {
      AnimationProgressed(adding ? 1.0f : 0.f);
      Stop();
    } else {
      AnimationProgressed(adding ? progress : 1.f - progress);
    }
  }

  void OnCompositingShuttingDown(ui::Compositor* compositor) override {
    if (compositor_ == compositor)
      Stop();
  }

  void Stop() {
    if (compositor_) {
      compositor_->RemoveAnimationObserver(this);
      compositor_ = nullptr;
    }
    state_ = WallpaperAnimationState::kNormal;
  }

  void Start() {
    DCHECK(!compositor_);
    compositor_ = Shell::GetPrimaryRootWindow()->GetHost()->compositor();
    compositor_->AddAnimationObserver(this);
    start_time_ = base::TimeTicks();
  }

  void AnimationProgressed(float value) {
    // Animate only to even numbers to reduce the load.
    int ivalue = static_cast<int>(value * kWallpaperBlurSigma) / 2 * 2;
    for (aura::Window* root : roots_to_animate_)
      ApplyBlurAndOpacity(root, ivalue);
  }

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override {
    window->RemoveObserver(this);
    auto it =
        std::find(roots_to_animate_.begin(), roots_to_animate_.end(), window);
    if (it != roots_to_animate_.end())
      roots_to_animate_.erase(it);
  }

  void ApplyBlurAndOpacity(aura::Window* root, int value) {
    DCHECK_GE(value, 0);
    DCHECK_LE(value, 10);
    const float opacity =
        gfx::Tween::FloatValueBetween(value / 10.0, 1.f, kShieldOpacity);
    auto* wallpaper_widget_controller =
        RootWindowController::ForWindow(root)->wallpaper_widget_controller();
    if (wallpaper_widget_controller->wallpaper_view()) {
      wallpaper_widget_controller->wallpaper_view()->RepaintBlurAndOpacity(
          value, opacity);
    }
  }

  // Called when the wallpaper is to be changed. Checks to see which root
  // windows should have their wallpaper blurs animated and fills
  // |roots_to_animate_| accordingly. Applys blur or unblur immediately if
  // the wallpaper does not need blur animation.
  // When |animate_only| is true, it'll apply blur only to the root windows that
  // requires animation.
  void OnBlurChange(WallpaperAnimationState state, bool animate_only) {
    Stop();
    for (aura::Window* root : roots_to_animate_)
      root->RemoveObserver(this);
    roots_to_animate_.clear();

    state_ = state;
    const bool should_blur = state_ == WallpaperAnimationState::kAddingBlur;
    if (animate_only)
      DCHECK(should_blur);

    const float value =
        should_blur ? kWallpaperBlurSigma : kWallpaperClearBlurSigma;

    OverviewSession* overview_session =
        Shell::Get()->overview_controller()->overview_session();
    for (aura::Window* root : Shell::Get()->GetAllRootWindows()) {
      // No need to animate the blur on exiting as this should only be called
      // after overview animations are finished.
      if (should_blur) {
        DCHECK(overview_session);
        OverviewGrid* grid = overview_session->GetGridWithRootWindow(root);
        bool should_animate = grid && grid->ShouldAnimateWallpaper();
        if (should_animate && animate_only) {
          root->AddObserver(this);
          roots_to_animate_.push_back(root);
          continue;
        }
        if (should_animate == animate_only)
          ApplyBlurAndOpacity(root, value);
      } else {
        ApplyBlurAndOpacity(root, value);
      }
    }

    // Run the animation if one of the roots needs to be animated.
    if (roots_to_animate_.empty())
      state_ = WallpaperAnimationState::kNormal;
    else
      Start();
  }

  ui::Compositor* compositor_ = nullptr;
  base::TimeTicks start_time_;

  WallpaperAnimationState state_ = WallpaperAnimationState::kNormal;
  // Vector which contains the root windows, if any, whose wallpaper should have
  // blur animated after Blur or Unblur is called.
  std::vector<aura::Window*> roots_to_animate_;

  DISALLOW_COPY_AND_ASSIGN(OverviewBlurController);
};

OverviewController::OverviewController()
    : occlusion_pause_duration_for_end_ms_(kOcclusionPauseDurationForEndMs),
      overview_blur_controller_(std::make_unique<OverviewBlurController>()),
      weak_ptr_factory_(this) {
  Shell::Get()->activation_client()->AddObserver(this);
}

OverviewController::~OverviewController() {
  Shell::Get()->activation_client()->RemoveObserver(this);
  overview_blur_controller_.reset();

  // Destroy widgets that may be still animating if shell shuts down soon after
  // exiting overview mode.
  for (std::unique_ptr<DelayedAnimationObserver>& animation_observer :
       delayed_animations_) {
    animation_observer->Shutdown();
  }

  if (overview_session_) {
    overview_session_->Shutdown();
    overview_session_.reset();
  }
}

// static
bool OverviewController::CanSelect() {
  // Don't allow a window overview if the user session is not active (e.g.
  // locked or in user-adding screen) or a modal dialog is open or running in
  // kiosk app session.
  SessionController* session_controller = Shell::Get()->session_controller();
  return session_controller->GetSessionState() ==
             session_manager::SessionState::ACTIVE &&
         !Shell::IsSystemModalWindowOpen() &&
         !Shell::Get()->screen_pinning_controller()->IsPinned() &&
         !session_controller->IsRunningInAppMode();
}

bool OverviewController::ToggleOverview(
    OverviewSession::EnterExitOverviewType type) {
  // Hide the virtual keyboard as it obstructs the overview mode.
  // Don't need to hide if it's the a11y keyboard, as overview mode
  // can accept text input and it resizes correctly with the a11y keyboard.
  keyboard::KeyboardController::Get()->HideKeyboardImplicitlyByUser();

  auto windows = Shell::Get()->mru_window_tracker()->BuildMruWindowList();

  // Hidden windows will be removed by wm::ShouldExcludeForOverview so we
  // must copy them out first.
  std::vector<aura::Window*> hide_windows(windows.size());
  auto end = std::copy_if(
      windows.begin(), windows.end(), hide_windows.begin(),
      [](aura::Window* w) { return w->GetProperty(kHideInOverviewKey); });
  hide_windows.resize(end - hide_windows.begin());
  base::EraseIf(windows, wm::ShouldExcludeForOverview);
  // Overview windows will handle showing their transient related windows, so if
  // a window in |windows| has a transient root also in |windows|, we can remove
  // it as the tranisent root will handle showing the window.
  wm::RemoveTransientDescendants(&windows);

  // We may want to slide the overview grid in or out in some cases, even if
  // not explicitly stated.
  OverviewSession::EnterExitOverviewType new_type = type;
  if (type == OverviewSession::EnterExitOverviewType::kNormal &&
      ShouldSlideInOutOverview(windows)) {
    new_type = OverviewSession::EnterExitOverviewType::kWindowsMinimized;
  }

  if (IsSelecting()) {
    // Do not allow ending overview if we're in single split mode unless swiping
    // up from the shelf.
    if (windows.empty() && Shell::Get()->IsSplitViewModeActive() &&
        type != OverviewSession::EnterExitOverviewType::kSwipeFromShelf) {
      return true;
    }

    // Suspend occlusion tracker until the exit animation is complete.
    PauseOcclusionTracker();

    overview_session_->set_enter_exit_overview_type(new_type);
    if (type == OverviewSession::EnterExitOverviewType::kWindowsMinimized ||
        type == OverviewSession::EnterExitOverviewType::kSwipeFromShelf) {
      // Minimize the windows without animations. When the home launcher button
      // is pressed, minimized widgets will get created in their place, and
      // those widgets will be slid out of overview. Otherwise,
      // HomeLauncherGestureHandler will handle sliding the windows out and when
      // this function is called, we do not need to create minimized widgets.
      std::vector<aura::Window*> windows_to_hide_minimize(windows.size());
      auto it = std::copy_if(
          windows.begin(), windows.end(), windows_to_hide_minimize.begin(),
          [](aura::Window* window) {
            return !wm::GetWindowState(window)->IsMinimized();
          });
      windows_to_hide_minimize.resize(
          std::distance(windows_to_hide_minimize.begin(), it));
      wm::HideAndMaybeMinimizeWithoutAnimation(windows_to_hide_minimize, true);
    }

    OnSelectionEnded();
  } else {
    // Don't start overview if window selection is not allowed.
    if (!CanSelect())
      return false;

    // Clear any animations that may be running from last overview end.
    for (const auto& animation : delayed_animations_)
      animation->Shutdown();
    if (!delayed_animations_.empty())
      OnEndingAnimationComplete(/*canceled=*/true);
    delayed_animations_.clear();

    // Suspend occlusion tracker until the enter animation is complete.
    PauseOcclusionTracker();

    overview_session_ = std::make_unique<OverviewSession>(this);
    overview_session_->set_enter_exit_overview_type(new_type);
    for (auto& observer : observers_)
      observer.OnOverviewModeStarting();
    overview_session_->Init(windows, hide_windows);
    if (IsBlurAllowed())
      overview_blur_controller_->Blur(/*animate_only=*/false);
    if (start_animations_.empty())
      OnStartingAnimationComplete(/*canceled=*/false);
    OnSelectionStarted();
  }
  return true;
}

void OverviewController::OnStartingAnimationComplete(bool canceled) {
  if (IsBlurAllowed())
    overview_blur_controller_->Blur(/*animate_only=*/true);

  for (auto& observer : observers_)
    observer.OnOverviewModeStartingAnimationComplete(canceled);
  if (overview_session_)
    overview_session_->OnStartingAnimationComplete(canceled);
  UnpauseOcclusionTracker(kOcclusionPauseDurationForStartMs);
}

void OverviewController::OnEndingAnimationComplete(bool canceled) {
  // Unblur when animation is completed (or right away if there was no
  // delayed animation) unless it's canceled, in which case, we should keep
  // the blur.
  if (IsBlurAllowed() && !canceled)
    overview_blur_controller_->Unblur();

  for (auto& observer : observers_)
    observer.OnOverviewModeEndingAnimationComplete(canceled);
  UnpauseOcclusionTracker(occlusion_pause_duration_for_end_ms_);
}

void OverviewController::ResetPauser() {
  occlusion_tracker_pauser_.reset();
}

bool OverviewController::IsSelecting() const {
  return overview_session_ != nullptr;
}

bool OverviewController::IsCompletingShutdownAnimations() {
  return !delayed_animations_.empty();
}

void OverviewController::IncrementSelection(int increment) {
  DCHECK(IsSelecting());
  overview_session_->IncrementSelection(increment);
}

bool OverviewController::AcceptSelection() {
  DCHECK(IsSelecting());
  return overview_session_->AcceptSelection();
}

void OverviewController::OnOverviewButtonTrayLongPressed(
    const gfx::Point& event_location) {
  // Do nothing if split view is not enabled.
  if (!ShouldAllowSplitView())
    return;

  // Depending on the state of the windows and split view, a long press has many
  // different results.
  // 1. Already in split view - exit split view. Activate the left window if it
  // is snapped left or both sides. Activate the right window if it is snapped
  // right.
  // 2. Not in overview mode - enter split view iff
  //     a) there is an active window
  //     b) there are at least two windows in the mru list
  //     c) the active window is snappable
  // 3. In overview mode - enter split view iff
  //     a) there are at least two windows in the mru list
  //     b) the first window in the mru list is snappable

  auto* split_view_controller = Shell::Get()->split_view_controller();
  // Exit split view mode if we are already in it.
  if (split_view_controller->IsSplitViewModeActive()) {
    // In some cases the window returned by wm::GetActiveWindow will be an item
    // in overview mode (maybe the overview mode text selection widget). The
    // active window may also be a transient descendant of the left or right
    // snapped window, in which we want to activate the transient window's
    // ancestor (left or right snapped window). Manually set |active_window| as
    // either the left or right window.
    aura::Window* active_window = wm::GetActiveWindow();
    while (::wm::GetTransientParent(active_window))
      active_window = ::wm::GetTransientParent(active_window);
    if (!split_view_controller->IsWindowInSplitView(active_window))
      active_window = split_view_controller->GetDefaultSnappedWindow();
    DCHECK(active_window);
    split_view_controller->EndSplitView();
    if (IsSelecting())
      ToggleOverview();
    ::wm::ActivateWindow(active_window);
    base::RecordAction(
        base::UserMetricsAction("Tablet_LongPressOverviewButtonExitSplitView"));
    return;
  }

  OverviewItem* item_to_snap = nullptr;
  if (!IsSelecting()) {
    // The current active window may be a transient child.
    aura::Window* active_window = wm::GetActiveWindow();
    while (active_window && ::wm::GetTransientParent(active_window))
      active_window = ::wm::GetTransientParent(active_window);

    // Do nothing if there are no active windows or less than two windows to
    // work with.
    if (!active_window ||
        Shell::Get()->mru_window_tracker()->BuildWindowForCycleList().size() <
            2u) {
      return;
    }

    // Show a toast if the window cannot be snapped.
    if (!CanSnapInSplitview(active_window)) {
      split_view_controller->ShowAppCannotSnapToast();
      return;
    }

    // If we are not in overview mode, enter overview mode and then find the
    // window item to snap.
    ToggleOverview();
    DCHECK(overview_session_);
    OverviewGrid* current_grid = overview_session_->GetGridWithRootWindow(
        active_window->GetRootWindow());
    if (current_grid)
      item_to_snap = current_grid->GetOverviewItemContaining(active_window);
  } else {
    // Currently in overview mode, with no snapped windows. Retrieve the first
    // overview item and attempt to snap that window.
    DCHECK(overview_session_);
    OverviewGrid* current_grid = overview_session_->GetGridWithRootWindow(
        wm::GetRootWindowAt(event_location));
    if (current_grid) {
      const auto& windows = current_grid->window_list();
      if (windows.size() > 1)
        item_to_snap = windows[0].get();
    }
  }

  // Do nothing if no item was retrieved, or if the retrieved item is
  // unsnappable.
  if (!item_to_snap || !CanSnapInSplitview(item_to_snap->GetWindow()))
    return;

  split_view_controller->SnapWindow(item_to_snap->GetWindow(),
                                    SplitViewController::LEFT);
  base::RecordAction(
      base::UserMetricsAction("Tablet_LongPressOverviewButtonEnterSplitView"));
}

bool OverviewController::IsInStartAnimation() {
  return !start_animations_.empty();
}

void OverviewController::PauseOcclusionTracker() {
  if (occlusion_tracker_pauser_)
    return;

  reset_pauser_task_.Cancel();
  occlusion_tracker_pauser_ =
      std::make_unique<aura::WindowOcclusionTracker::ScopedPause>(
          Shell::Get()->aura_env());
}

void OverviewController::UnpauseOcclusionTracker(int delay) {
  reset_pauser_task_.Reset(base::BindOnce(&OverviewController::ResetPauser,
                                          weak_ptr_factory_.GetWeakPtr()));
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, reset_pauser_task_.callback(),
      base::TimeDelta::FromMilliseconds(delay));
}

void OverviewController::AddObserver(OverviewObserver* observer) {
  observers_.AddObserver(observer);
}

void OverviewController::RemoveObserver(OverviewObserver* observer) {
  observers_.RemoveObserver(observer);
}

void OverviewController::DelayedUpdateMaskAndShadow() {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&OverviewController::UpdateMaskAndShadow,
                                weak_ptr_factory_.GetWeakPtr()));
}

// TODO(flackr): Make OverviewController observe the activation of
// windows, so we can remove OverviewDelegate.
// TODO(sammiequon): Rename to something like EndOverview() and refactor to use
// a single entry point for overview.
void OverviewController::OnSelectionEnded() {
  if (!IsSelecting())
    return;

  if (!occlusion_tracker_pauser_)
    PauseOcclusionTracker();

  if (!start_animations_.empty())
    OnStartingAnimationComplete(/*canceled=*/true);
  start_animations_.clear();

  auto* overview_session = overview_session_.release();
  // Do not show mask and show during overview shutdown.
  overview_session->UpdateMaskAndShadow();

  for (auto& observer : observers_)
    observer.OnOverviewModeEnding(overview_session);
  overview_session->Shutdown();
  // Don't delete |overview_session_| yet since the stack is still using it.
  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, overview_session);
  last_selection_time_ = base::Time::Now();
  for (auto& observer : observers_)
    observer.OnOverviewModeEnded();
  if (delayed_animations_.empty())
    OnEndingAnimationComplete(/*canceled=*/false);
}

void OverviewController::AddDelayedAnimationObserver(
    std::unique_ptr<DelayedAnimationObserver> animation_observer) {
  animation_observer->SetOwner(this);
  delayed_animations_.push_back(std::move(animation_observer));
}

void OverviewController::RemoveAndDestroyAnimationObserver(
    DelayedAnimationObserver* animation_observer) {
  const bool previous_empty = delayed_animations_.empty();
  base::EraseIf(delayed_animations_,
                base::MatchesUniquePtr(animation_observer));

  // If something has been removed and its the last observer, unblur the
  // wallpaper and let observers know. This function may be called while still
  // in overview (ie. splitview restores one window but leaves overview active)
  // so check that |overview_session_| is null before notifying.
  if (!overview_session_ && !previous_empty && delayed_animations_.empty())
    OnEndingAnimationComplete(/*canceled=*/false);
}

void OverviewController::OnWindowActivating(ActivationReason reason,
                                            aura::Window* gained_active,
                                            aura::Window* lost_active) {
  if (overview_session_)
    overview_session_->OnWindowActivating(reason, gained_active, lost_active);
}

void OverviewController::OnAttemptToReactivateWindow(
    aura::Window* request_active,
    aura::Window* actual_active) {
  if (overview_session_) {
    overview_session_->OnWindowActivating(
        ::wm::ActivationChangeObserver::ActivationReason::ACTIVATION_CLIENT,
        request_active, actual_active);
  }
}

bool OverviewController::HasBlurForTest() const {
  return overview_blur_controller_->has_blur();
}

bool OverviewController::HasBlurAnimationForTest() const {
  return overview_blur_controller_->has_blur_animation();
}

std::vector<aura::Window*>
OverviewController::GetWindowsListInOverviewGridsForTest() {
  std::vector<aura::Window*> windows;
  for (const std::unique_ptr<OverviewGrid>& grid :
       overview_session_->grid_list_for_testing()) {
    for (const auto& overview_session_item : grid->window_list())
      windows.push_back(overview_session_item->GetWindow());
  }
  return windows;
}

void OverviewController::AddStartAnimationObserver(
    std::unique_ptr<DelayedAnimationObserver> animation_observer) {
  animation_observer->SetOwner(this);
  start_animations_.push_back(std::move(animation_observer));
}

void OverviewController::RemoveAndDestroyStartAnimationObserver(
    DelayedAnimationObserver* animation_observer) {
  const bool previous_empty = start_animations_.empty();
  base::EraseIf(start_animations_, base::MatchesUniquePtr(animation_observer));

  if (!previous_empty && start_animations_.empty())
    OnStartingAnimationComplete(/*canceled=*/false);
}

void OverviewController::UpdateMaskAndShadow() {
  if (overview_session_)
    overview_session_->UpdateMaskAndShadow();
}

// static
void OverviewController::SetDoNotChangeWallpaperBlurForTests() {
  g_disable_wallpaper_blur_for_tests = true;
}

void OverviewController::OnSelectionStarted() {
  if (!last_selection_time_.is_null()) {
    UMA_HISTOGRAM_LONG_TIMES("Ash.WindowSelector.TimeBetweenUse",
                             base::Time::Now() - last_selection_time_);
  }
}

}  // namespace ash
