// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/wm/window_overview_mode.h"

#include <algorithm>
#include <functional>
#include <vector>

#include "athena/wm/overview_toolbar.h"
#include "athena/wm/public/window_list_provider.h"
#include "athena/wm/public/window_list_provider_observer.h"
#include "athena/wm/split_view_controller.h"
#include "base/bind.h"
#include "base/macros.h"
#include "ui/aura/scoped_window_targeter.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_property.h"
#include "ui/aura/window_targeter.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/closure_animation_observer.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/compositor_animation_observer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/events/event_handler.h"
#include "ui/events/gestures/fling_curve.h"
#include "ui/gfx/frame_time.h"
#include "ui/gfx/transform.h"
#include "ui/wm/core/shadow_types.h"
#include "ui/wm/core/window_animations.h"
#include "ui/wm/core/window_util.h"

namespace {

const float kOverviewDefaultScale = 0.75f;

struct WindowOverviewState {
  // The current overview state of the window. 0.f means the window is at the
  // topmost position. 1.f means the window is at the bottom-most position.
  float progress;

  // The top-most and bottom-most vertical position of the window in overview
  // mode.
  float max_y;
  float min_y;

  // |split| is set if this window is one of the two split windows in split-view
  // mode.
  bool split;
};

}  // namespace

DECLARE_WINDOW_PROPERTY_TYPE(WindowOverviewState*)
DEFINE_OWNED_WINDOW_PROPERTY_KEY(WindowOverviewState,
                                 kWindowOverviewState,
                                 NULL)
namespace athena {

namespace {

gfx::Transform GetTransformForSplitWindow(aura::Window* window, float scale) {
  const float kScrollWindowPositionInOverview = 0.65f;
  int x_translate = window->bounds().width() * (1 - scale) / 2;
  gfx::Transform transform;
  transform.Translate(
      x_translate, window->bounds().height() * kScrollWindowPositionInOverview);
  transform.Scale(scale, scale);
  return transform;
}

// Gets the transform for the window in its current state.
gfx::Transform GetTransformForState(aura::Window* window,
                                    WindowOverviewState* state) {
  if (state->split)
    return GetTransformForSplitWindow(window, kOverviewDefaultScale);

  const float kProgressToStartShrinking = 0.07;
  const float kOverviewScale = 0.75f;
  float scale = kOverviewScale;
  if (state->progress < kProgressToStartShrinking) {
    const float kShrunkMinimumScale = 0.7f;
    scale = gfx::Tween::FloatValueBetween(
        state->progress / kProgressToStartShrinking,
        kShrunkMinimumScale,
        kOverviewScale);
  }
  int container_width = window->parent()->bounds().width();
  int window_width = window->bounds().width();
  int window_x = window->bounds().x();
  float x_translate = (container_width - (window_width * scale)) / 2 - window_x;
  float y_translate = gfx::Tween::FloatValueBetween(
      state->progress, state->min_y, state->max_y);
  gfx::Transform transform;
  transform.Translate(x_translate, y_translate);
  transform.Scale(scale, scale);
  return transform;
}

// Sets the progress-state for the window in the overview mode.
void SetWindowProgress(aura::Window* window, float progress) {
  WindowOverviewState* state = window->GetProperty(kWindowOverviewState);
  state->progress = progress;

  gfx::Transform transform = GetTransformForState(window, state);
  window->SetTransform(transform);
}

void HideWindowIfNotVisible(aura::Window* window,
                            SplitViewController* split_view_controller) {
  bool should_hide = true;
  if (split_view_controller->IsSplitViewModeActive()) {
    should_hide = window != split_view_controller->left_window() &&
                  window != split_view_controller->right_window();
  } else {
    should_hide = !wm::IsActiveWindow(window);
  }
  if (should_hide)
    window->Hide();
}

// Resets the overview-related state for |window|.
void RestoreWindowState(aura::Window* window,
                        SplitViewController* split_view_controller) {
  window->ClearProperty(kWindowOverviewState);

  ui::ScopedLayerAnimationSettings settings(window->layer()->GetAnimator());
  settings.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  settings.SetTransitionDuration(base::TimeDelta::FromMilliseconds(250));

  settings.AddObserver(new ui::ClosureAnimationObserver(
      base::Bind(&HideWindowIfNotVisible, window, split_view_controller)));

  window->SetTransform(gfx::Transform());

  // Reset the window opacity in case the user is dragging a window.
  window->layer()->SetOpacity(1.0f);

  wm::SetShadowType(window, wm::SHADOW_TYPE_NONE);
}

gfx::RectF GetTransformedBounds(aura::Window* window) {
  gfx::Transform transform;
  gfx::RectF bounds = window->bounds();
  transform.Translate(bounds.x(), bounds.y());
  transform.PreconcatTransform(window->layer()->transform());
  transform.Translate(-bounds.x(), -bounds.y());
  transform.TransformRect(&bounds);
  return bounds;
}

void TransformSplitWindowScale(aura::Window* window, float scale) {
  gfx::Transform transform = window->layer()->GetTargetTransform();
  if (transform.Scale2d() == gfx::Vector2dF(scale, scale))
    return;
  ui::ScopedLayerAnimationSettings settings(window->layer()->GetAnimator());
  window->SetTransform(GetTransformForSplitWindow(window, scale));
}

void AnimateWindowTo(aura::Window* animate_window,
                     aura::Window* target_window) {
  ui::ScopedLayerAnimationSettings settings(
      animate_window->layer()->GetAnimator());
  settings.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  WindowOverviewState* target_state =
      target_window->GetProperty(kWindowOverviewState);
  SetWindowProgress(animate_window, target_state->progress);
}

// Always returns the same target.
class StaticWindowTargeter : public aura::WindowTargeter {
 public:
  explicit StaticWindowTargeter(aura::Window* target) : target_(target) {}
  virtual ~StaticWindowTargeter() {}

 private:
  // aura::WindowTargeter:
  virtual ui::EventTarget* FindTargetForEvent(ui::EventTarget* root,
                                              ui::Event* event) OVERRIDE {
    return target_;
  }

  virtual ui::EventTarget* FindTargetForLocatedEvent(
      ui::EventTarget* root,
      ui::LocatedEvent* event) OVERRIDE {
    return target_;
  }

  aura::Window* target_;
  DISALLOW_COPY_AND_ASSIGN(StaticWindowTargeter);
};

class WindowOverviewModeImpl : public WindowOverviewMode,
                               public ui::EventHandler,
                               public ui::CompositorAnimationObserver,
                               public WindowListProviderObserver {
 public:
  WindowOverviewModeImpl(aura::Window* container,
                         WindowListProvider* window_list_provider,
                         SplitViewController* split_view_controller,
                         WindowOverviewModeDelegate* delegate)
      : container_(container),
        window_list_provider_(window_list_provider),
        split_view_controller_(split_view_controller),
        delegate_(delegate),
        scoped_targeter_(new aura::ScopedWindowTargeter(
            container,
            scoped_ptr<ui::EventTargeter>(
                new StaticWindowTargeter(container)))),
        dragged_window_(NULL) {
    CHECK(delegate_);
    container_->set_target_handler(this);

    // Prepare the desired transforms for all the windows, and set the initial
    // state on the windows.
    ComputeTerminalStatesForAllWindows();
    SetInitialWindowStates();

    window_list_provider_->AddObserver(this);
  }

  virtual ~WindowOverviewModeImpl() {
    window_list_provider_->RemoveObserver(this);
    container_->set_target_handler(container_->delegate());
    RemoveAnimationObserver();
    const aura::Window::Windows& windows =
        window_list_provider_->GetWindowList();
    if (windows.empty())
      return;
    std::for_each(windows.begin(),
                  windows.end(),
                  std::bind2nd(std::ptr_fun(&RestoreWindowState),
                               split_view_controller_));
  }

 private:
  // Computes the transforms for all windows in both the topmost and bottom-most
  // positions. The transforms are set in the |kWindowOverviewState| property of
  // the windows.
  void ComputeTerminalStatesForAllWindows() {
    size_t index = 0;

    const aura::Window::Windows& windows =
        window_list_provider_->GetWindowList();
    for (aura::Window::Windows::const_reverse_iterator iter = windows.rbegin();
         iter != windows.rend();
         ++iter, ++index) {
      aura::Window* window = (*iter);
      wm::SetShadowType(window, wm::SHADOW_TYPE_RECTANGULAR_ALWAYS_ACTIVE);

      WindowOverviewState* state = new WindowOverviewState;
      window->SetProperty(kWindowOverviewState, state);
      if (split_view_controller_->IsSplitViewModeActive() &&
          (window == split_view_controller_->left_window() ||
           window == split_view_controller_->right_window())) {
        // Do not let the left/right windows be scrolled.
        gfx::Transform transform =
            GetTransformForSplitWindow(window, kOverviewDefaultScale);
        state->max_y = state->min_y = transform.To2dTranslation().y();
        state->split = true;
        --index;
        continue;
      }
      state->split = false;
      UpdateTerminalStateForWindowAtIndex(window, index, windows.size());
    }
  }

  // Computes the terminal states (i.e. the transforms for the top-most and
  // bottom-most position in the stack) for |window|. |window_count| is the
  // number of windows in the stack, and |index| is the position of the window
  // in the stack (0 being the front-most window).
  void UpdateTerminalStateForWindowAtIndex(aura::Window* window,
                                           size_t index,
                                           size_t window_count) {
    const int kGapBetweenWindowsBottom = 10;
    const int kGapBetweenWindowsTop = 5;

    int top = (window_count - index - 1) * kGapBetweenWindowsTop;
    int bottom = GetScrollableHeight() - (index * kGapBetweenWindowsBottom);

    WindowOverviewState* state = window->GetProperty(kWindowOverviewState);
    CHECK(state);
    if (state->split)
      return;
    state->min_y = top;
    state->max_y = bottom - window->bounds().y();
    state->progress = 0.f;
  }

  // Sets the initial position for the windows for the overview mode.
  void SetInitialWindowStates() {
    // The initial overview state of the topmost three windows.
    const float kInitialProgress[] = { 0.5f, 0.05f, 0.01f };
    size_t index = 0;
    const aura::Window::Windows& windows =
        window_list_provider_->GetWindowList();
    for (aura::Window::Windows::const_reverse_iterator iter = windows.rbegin();
         iter != windows.rend();
         ++iter) {
      float progress = 0.f;
      aura::Window* window = *iter;
      if (split_view_controller_->IsSplitViewModeActive() &&
          (window == split_view_controller_->left_window() ||
           window == split_view_controller_->right_window())) {
        progress = 1;
      } else {
        if (index < arraysize(kInitialProgress))
          progress = kInitialProgress[index];
        ++index;
      }

      scoped_refptr<ui::LayerAnimator> animator =
          window->layer()->GetAnimator();

      // Unset any in-progress animation.
      animator->AbortAllAnimations();
      window->Show();
      window->SetTransform(gfx::Transform());
      // Setup the animation.
      {
        ui::ScopedLayerAnimationSettings settings(animator);
        settings.SetPreemptionStrategy(
            ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
        settings.SetTransitionDuration(base::TimeDelta::FromMilliseconds(250));
        SetWindowProgress(window, progress);
      }
    }
  }

  aura::Window* SelectWindowAt(ui::LocatedEvent* event) {
    CHECK_EQ(container_, event->target());
    // Find the old targeter to find the target of the event.
    ui::EventTarget* window = container_;
    ui::EventTargeter* targeter = scoped_targeter_->old_targeter();
    while (!targeter && window->GetParentTarget()) {
      window = window->GetParentTarget();
      targeter = window->GetEventTargeter();
    }
    if (!targeter)
      return NULL;
    aura::Window* target = static_cast<aura::Window*>(
        targeter->FindTargetForLocatedEvent(container_, event));
    while (target && target->parent() != container_)
      target = target->parent();
    return target;
  }

  // Scroll the window list by |delta_y| amount. |delta_y| is negative when
  // scrolling up; and positive when scrolling down.
  void DoScroll(float delta_y) {
    const float kEpsilon = 1e-3f;
    float delta_y_p = std::abs(delta_y) / GetScrollableHeight();
    const aura::Window::Windows& windows =
        window_list_provider_->GetWindowList();
    if (delta_y < 0) {
      // Scroll up. Start with the top-most (i.e. behind-most in terms of
      // z-index) window, and try to scroll them up.
      for (aura::Window::Windows::const_iterator iter = windows.begin();
           delta_y_p > kEpsilon && iter != windows.end();
           ++iter) {
        aura::Window* window = (*iter);
        WindowOverviewState* state = window->GetProperty(kWindowOverviewState);
        if (state->progress > kEpsilon) {
          // It is possible to scroll |window| up. Scroll it up, and update
          // |delta_y_p| for the next window.
          float apply = delta_y_p * state->progress;
          SetWindowProgress(window, std::max(0.f, state->progress - apply * 3));
          delta_y_p -= apply;
        }
      }
    } else {
      // Scroll down. Start with the bottom-most (i.e. front-most in terms of
      // z-index) window, and try to scroll them down.
      aura::Window::Windows::const_reverse_iterator iter;
      for (iter = windows.rbegin();
           delta_y_p > kEpsilon && iter != windows.rend();
           ++iter) {
        aura::Window* window = (*iter);
        WindowOverviewState* state = window->GetProperty(kWindowOverviewState);
        if (1.f - state->progress > kEpsilon) {
          // It is possible to scroll |window| down. Scroll it down, and update
          // |delta_y_p| for the next window.
          SetWindowProgress(window, std::min(1.f, state->progress + delta_y_p));
          delta_y_p /= 2.f;
        }
      }
    }
  }

  int GetScrollableHeight() const {
    const float kScrollableFraction = 0.85f;
    const float kScrollableFractionInSplit = 0.5f;
    const float fraction = split_view_controller_->IsSplitViewModeActive()
                               ? kScrollableFractionInSplit
                               : kScrollableFraction;
    return container_->bounds().height() * fraction;
  }

  void CreateFlingerFor(const ui::GestureEvent& event) {
    gfx::Vector2dF velocity(event.details().velocity_x(),
                            event.details().velocity_y());
    fling_.reset(new ui::FlingCurve(velocity, gfx::FrameTime::Now()));
  }

  void AddAnimationObserver() {
    ui::Compositor* compositor = container_->GetHost()->compositor();
    if (!compositor->HasAnimationObserver(this))
      compositor->AddAnimationObserver(this);
  }

  void RemoveAnimationObserver() {
    ui::Compositor* compositor = container_->GetHost()->compositor();
    if (compositor->HasAnimationObserver(this))
      compositor->RemoveAnimationObserver(this);
  }

  aura::Window* GetSplitWindowDropTarget(const ui::GestureEvent& event) const {
    if (!split_view_controller_->IsSplitViewModeActive())
      return NULL;
    CHECK(dragged_window_);
    CHECK_NE(split_view_controller_->left_window(), dragged_window_);
    CHECK_NE(split_view_controller_->right_window(), dragged_window_);
    aura::Window* window = split_view_controller_->left_window();
    if (GetTransformedBounds(window).Contains(event.location()))
      return window;
    window = split_view_controller_->right_window();
    if (GetTransformedBounds(window).Contains(event.location()))
      return window;
    return NULL;
  }

  void DragWindow(const ui::GestureEvent& event) {
    CHECK(dragged_window_);
    CHECK_EQ(ui::ET_GESTURE_SCROLL_UPDATE, event.type());
    CHECK(overview_toolbar_);
    gfx::Vector2dF dragged_distance =
        dragged_start_location_ - event.location();
    WindowOverviewState* dragged_state =
        dragged_window_->GetProperty(kWindowOverviewState);
    CHECK(dragged_state);
    gfx::Transform transform =
        GetTransformForState(dragged_window_, dragged_state);
    transform.Translate(-dragged_distance.x(), 0);
    dragged_window_->SetTransform(transform);

    // Update the toolbar.
    const int kMinDistanceForActionButtons = 20;
    if (fabs(dragged_distance.x()) > kMinDistanceForActionButtons)
      overview_toolbar_->ShowActionButtons();
    else
      overview_toolbar_->HideActionButtons();

    // See if the touch-point is above one of the action-buttons.
    OverviewToolbar::ActionType new_action =
        overview_toolbar_->GetHighlightAction(event);

    // If the touch-point is not above any of the action buttons, then highlight
    // the close-button by default, if the user has dragged enough to close the
    // window.
    if (new_action == OverviewToolbar::ACTION_TYPE_NONE) {
      if (fabs(dragged_distance.x()) > kMinDistanceForDismissal)
        new_action = OverviewToolbar::ACTION_TYPE_CLOSE;
      else
        new_action = OverviewToolbar::ACTION_TYPE_NONE;
    }
    OverviewToolbar::ActionType previous_action =
        overview_toolbar_->current_action();
    overview_toolbar_->SetHighlightAction(new_action);

    aura::Window* split_drop = GetSplitWindowDropTarget(event);

    // If the user has selected to get into split-view mode, then show the
    // window with full opacity. Otherwise, fade it out as it closes. Animate
    // the opacity if transitioning to/from the split-view button.
    bool animate_opacity =
        (new_action != previous_action) &&
        ((new_action == OverviewToolbar::ACTION_TYPE_SPLIT) ||
         (previous_action == OverviewToolbar::ACTION_TYPE_SPLIT));
    float ratio = std::min(
        1.f, std::abs(dragged_distance.x()) / kMinDistanceForDismissal);
    float opacity =
        (new_action == OverviewToolbar::ACTION_TYPE_SPLIT || split_drop)
            ? 1
            : gfx::Tween::FloatValueBetween(ratio, kMaxOpacity, kMinOpacity);
    if (animate_opacity) {
      ui::ScopedLayerAnimationSettings settings(
          dragged_window_->layer()->GetAnimator());
      dragged_window_->layer()->SetOpacity(opacity);
    } else {
      dragged_window_->layer()->SetOpacity(opacity);
    }

    if (split_view_controller_->IsSplitViewModeActive()) {
      float scale = kOverviewDefaultScale;
      if (split_drop == split_view_controller_->left_window())
        scale = kMaxScaleForSplitTarget;
      TransformSplitWindowScale(split_view_controller_->left_window(), scale);

      scale = kOverviewDefaultScale;
      if (split_drop == split_view_controller_->right_window())
        scale = kMaxScaleForSplitTarget;
      TransformSplitWindowScale(split_view_controller_->right_window(), scale);
    }
  }

  bool ShouldCloseDragWindow(const ui::GestureEvent& event) const {
    gfx::Vector2dF dragged_distance =
        dragged_start_location_ - event.location();
    if (event.type() == ui::ET_GESTURE_SCROLL_END)
      return std::abs(dragged_distance.x()) >= kMinDistanceForDismissal;
    CHECK_EQ(ui::ET_SCROLL_FLING_START, event.type());
    const bool dragging_towards_right = dragged_distance.x() < 0;
    const bool swipe_towards_right = event.details().velocity_x() > 0;
    if (dragging_towards_right != swipe_towards_right)
      return false;
    const float kMinVelocityForDismissal = 500.f;
    return std::abs(event.details().velocity_x()) > kMinVelocityForDismissal;
  }

  void CloseDragWindow(const ui::GestureEvent& gesture) {
    // Animate |dragged_window_| offscreen first, then destroy it.
    {
      wm::ScopedHidingAnimationSettings settings(dragged_window_);
      settings.layer_animation_settings()->SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);

      WindowOverviewState* dragged_state =
          dragged_window_->GetProperty(kWindowOverviewState);
      CHECK(dragged_state);
      gfx::Transform transform = dragged_window_->layer()->transform();
      gfx::RectF transformed_bounds = dragged_window_->bounds();
      transform.TransformRect(&transformed_bounds);
      float transform_x = 0.f;
      if (gesture.location().x() > dragged_start_location_.x())
        transform_x = container_->bounds().right() - transformed_bounds.x();
      else
        transform_x = -(transformed_bounds.x() + transformed_bounds.width());
      transform.Translate(transform_x / kOverviewDefaultScale, 0);
      dragged_window_->SetTransform(transform);
      dragged_window_->layer()->SetOpacity(kMinOpacity);
    }
    delete dragged_window_;
    dragged_window_ = NULL;
  }

  void RestoreDragWindow() {
    CHECK(dragged_window_);
    WindowOverviewState* dragged_state =
        dragged_window_->GetProperty(kWindowOverviewState);
    CHECK(dragged_state);

    ui::ScopedLayerAnimationSettings settings(
        dragged_window_->layer()->GetAnimator());
    settings.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    dragged_window_->SetTransform(
        GetTransformForState(dragged_window_, dragged_state));
    dragged_window_->layer()->SetOpacity(1.f);
    dragged_window_ = NULL;
  }

  void EndDragWindow(const ui::GestureEvent& gesture) {
    CHECK(dragged_window_);
    CHECK(overview_toolbar_);
    OverviewToolbar::ActionType action = overview_toolbar_->current_action();
    overview_toolbar_.reset();
    if (action == OverviewToolbar::ACTION_TYPE_SPLIT) {
      delegate_->OnSelectSplitViewWindow(NULL,
                                         dragged_window_,
                                         dragged_window_);
      return;
    }

    // If the window is dropped on one of the left/right windows in split-mode,
    // then switch that window.
    aura::Window* split_drop = GetSplitWindowDropTarget(gesture);
    if (split_drop) {
      aura::Window* left = split_view_controller_->left_window();
      aura::Window* right = split_view_controller_->right_window();
      if (left == split_drop)
        left = dragged_window_;
      else
        right = dragged_window_;
      delegate_->OnSelectSplitViewWindow(left, right, dragged_window_);
      return;
    }

    if (ShouldCloseDragWindow(gesture))
      CloseDragWindow(gesture);
    else
      RestoreDragWindow();
  }

  void SelectWindow(aura::Window* window) {
    if (!split_view_controller_->IsSplitViewModeActive()) {
      delegate_->OnSelectWindow(window);
    } else {
      // If the selected window is one of the left/right windows, then keep the
      // current state.
      if (window == split_view_controller_->left_window() ||
          window == split_view_controller_->right_window()) {
        delegate_->OnSelectSplitViewWindow(
            split_view_controller_->left_window(),
            split_view_controller_->right_window(),
            window);
      } else {
        delegate_->OnSelectWindow(window);
      }
    }
  }

  // ui::EventHandler:
  virtual void OnMouseEvent(ui::MouseEvent* mouse) OVERRIDE {
    if (mouse->type() == ui::ET_MOUSE_PRESSED) {
      aura::Window* select = SelectWindowAt(mouse);
      if (select) {
        mouse->SetHandled();
        SelectWindow(select);
      }
    } else if (mouse->type() == ui::ET_MOUSEWHEEL) {
      DoScroll(static_cast<ui::MouseWheelEvent*>(mouse)->y_offset());
    }
  }

  virtual void OnScrollEvent(ui::ScrollEvent* scroll) OVERRIDE {
    if (scroll->type() == ui::ET_SCROLL)
      DoScroll(scroll->y_offset());
  }

  virtual void OnGestureEvent(ui::GestureEvent* gesture) OVERRIDE {
    if (gesture->type() == ui::ET_GESTURE_TAP) {
      aura::Window* select = SelectWindowAt(gesture);
      if (select) {
        gesture->SetHandled();
        SelectWindow(select);
      }
    } else if (gesture->type() == ui::ET_GESTURE_SCROLL_BEGIN) {
      if (std::abs(gesture->details().scroll_x_hint()) >
          std::abs(gesture->details().scroll_y_hint()) * 2) {
        dragged_start_location_ = gesture->location();
        dragged_window_ = SelectWindowAt(gesture);
        if (split_view_controller_->IsSplitViewModeActive() &&
            (dragged_window_ == split_view_controller_->left_window() ||
             dragged_window_ == split_view_controller_->right_window())) {
          // TODO(sad): Allow closing the left/right window. Closing one of
          // these windows will terminate the split-view mode. Until then, do
          // not allow closing these (since otherwise it gets into an undefined
          // state).
          dragged_window_ = NULL;
        }

        if (dragged_window_) {
          // Show the toolbar (for closing a window, or going into split-view
          // mode). If already in split-view mode, then do not show the 'Split'
          // option.
          overview_toolbar_.reset(new OverviewToolbar(container_));
          if (!split_view_controller_->CanActivateSplitViewMode()) {
            overview_toolbar_->DisableAction(
                OverviewToolbar::ACTION_TYPE_SPLIT);
          }
        }
      }
    } else if (gesture->type() == ui::ET_GESTURE_SCROLL_UPDATE) {
      if (dragged_window_)
        DragWindow(*gesture);
      else
        DoScroll(gesture->details().scroll_y());
      gesture->SetHandled();
    } else if (gesture->type() == ui::ET_GESTURE_SCROLL_END) {
      if (dragged_window_)
        EndDragWindow(*gesture);
      gesture->SetHandled();
    } else if (gesture->type() == ui::ET_SCROLL_FLING_START) {
      if (dragged_window_) {
        EndDragWindow(*gesture);
      } else {
        CreateFlingerFor(*gesture);
        AddAnimationObserver();
      }
      gesture->SetHandled();
    } else if (gesture->type() == ui::ET_GESTURE_TAP_DOWN) {
      if (fling_) {
        fling_.reset();
        RemoveAnimationObserver();
        gesture->SetHandled();
      }
      dragged_window_ = NULL;
    }
  }

  // ui::CompositorAnimationObserver:
  virtual void OnAnimationStep(base::TimeTicks timestamp) OVERRIDE {
    CHECK(fling_);
    if (fling_->start_timestamp() > timestamp)
      return;
    gfx::Vector2dF scroll = fling_->GetScrollAmountAtTime(timestamp);
    if (scroll.IsZero()) {
      fling_.reset();
      RemoveAnimationObserver();
    } else {
      DoScroll(scroll.y());
    }
  }

  // WindowListProviderObserver:
  virtual void OnWindowStackingChanged() OVERRIDE {
    // Recompute the states of all windows. There isn't enough information at
    // this point to do anything more clever.
    ComputeTerminalStatesForAllWindows();
    SetInitialWindowStates();
  }

  virtual void OnWindowRemoved(aura::Window* removed_window,
                               int index) OVERRIDE {
    const aura::Window::Windows& windows =
        window_list_provider_->GetWindowList();
    if (windows.empty())
      return;
    CHECK_LE(index, static_cast<int>(windows.size()));
    if (index == 0) {
      // The back-most window has been removed. Move all the remaining windows
      // one step backwards.
      for (int i = windows.size() - 1; i > 0; --i) {
        UpdateTerminalStateForWindowAtIndex(
            windows[i], windows.size() - 1 - i, windows.size());
        AnimateWindowTo(windows[i], windows[i - 1]);
      }
      UpdateTerminalStateForWindowAtIndex(windows.front(),
                                          windows.size() - 1,
                                          windows.size());
      AnimateWindowTo(windows.front(), removed_window);
    } else {
      // Move all windows behind the removed window one step forwards.
      for (int i = 0; i < index - 1; ++i) {
        UpdateTerminalStateForWindowAtIndex(windows[i], windows.size() - 1 - i,
                                            windows.size());
        AnimateWindowTo(windows[i], windows[i + 1]);
      }
      UpdateTerminalStateForWindowAtIndex(windows[index - 1],
                                          windows.size() - index,
                                          windows.size());
      AnimateWindowTo(windows[index - 1], removed_window);
    }
  }

  const int kMinDistanceForDismissal = 300;
  const float kMaxOpacity = 1.0f;
  const float kMinOpacity = 0.2f;
  const float kMaxScaleForSplitTarget = 0.9f;

  aura::Window* container_;
  // Provider of the stack of windows to show in the overview mode. Not owned.
  WindowListProvider* window_list_provider_;
  SplitViewController* split_view_controller_;

  WindowOverviewModeDelegate* delegate_;
  scoped_ptr<aura::ScopedWindowTargeter> scoped_targeter_;
  scoped_ptr<ui::FlingCurve> fling_;

  aura::Window* dragged_window_;
  gfx::Point dragged_start_location_;
  scoped_ptr<OverviewToolbar> overview_toolbar_;

  DISALLOW_COPY_AND_ASSIGN(WindowOverviewModeImpl);
};

}  // namespace

// static
scoped_ptr<WindowOverviewMode> WindowOverviewMode::Create(
    aura::Window* container,
    WindowListProvider* window_list_provider,
    SplitViewController* split_view_controller,
    WindowOverviewModeDelegate* delegate) {
  return scoped_ptr<WindowOverviewMode>(
      new WindowOverviewModeImpl(container, window_list_provider,
                                 split_view_controller, delegate));
}

}  // namespace athena
