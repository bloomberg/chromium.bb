// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/wm/window_overview_mode.h"

#include <algorithm>
#include <functional>
#include <vector>

#include "athena/common/closure_animation_observer.h"
#include "athena/wm/public/window_list_provider.h"
#include "base/bind.h"
#include "base/macros.h"
#include "ui/aura/scoped_window_targeter.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_property.h"
#include "ui/aura/window_targeter.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/compositor_animation_observer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/events/event_handler.h"
#include "ui/events/gestures/fling_curve.h"
#include "ui/gfx/frame_time.h"
#include "ui/gfx/transform.h"
#include "ui/wm/core/shadow.h"

namespace {

struct WindowOverviewState {
  // The transform for when the window is at the topmost position.
  gfx::Transform top;

  // The transform for when the window is at the bottom-most position.
  gfx::Transform bottom;

  // The current overview state of the window. 0.f means the window is at the
  // topmost position. 1.f means the window is at the bottom-most position.
  float progress;

  scoped_ptr<wm::Shadow> shadow;
};

}  // namespace

DECLARE_WINDOW_PROPERTY_TYPE(WindowOverviewState*)
DEFINE_OWNED_WINDOW_PROPERTY_KEY(WindowOverviewState,
                                 kWindowOverviewState,
                                 NULL)
namespace athena {

namespace {

// Gets the transform for the window in its current state.
gfx::Transform GetTransformForState(WindowOverviewState* state) {
  return gfx::Tween::TransformValueBetween(state->progress,
                                           state->top,
                                           state->bottom);
}

// Sets the progress-state for the window in the overview mode.
void SetWindowProgress(aura::Window* window, float progress) {
  WindowOverviewState* state = window->GetProperty(kWindowOverviewState);
  state->progress = progress;
  window->SetTransform(GetTransformForState(state));
}

// Resets the overview-related state for |window|.
void RestoreWindowState(aura::Window* window) {
  window->ClearProperty(kWindowOverviewState);

  ui::ScopedLayerAnimationSettings settings(window->layer()->GetAnimator());
  settings.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  settings.SetTransitionDuration(base::TimeDelta::FromMilliseconds(250));
  window->SetTransform(gfx::Transform());
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
                               public ui::CompositorAnimationObserver {
 public:
  WindowOverviewModeImpl(aura::Window* container,
                         const WindowListProvider* window_list_provider,
                         WindowOverviewModeDelegate* delegate)
      : container_(container),
        window_list_provider_(window_list_provider),
        delegate_(delegate),
        scoped_targeter_(new aura::ScopedWindowTargeter(
            container,
            scoped_ptr<ui::EventTargeter>(
                new StaticWindowTargeter(container)))),
        dragged_window_(NULL) {
    container_->set_target_handler(this);

    // Prepare the desired transforms for all the windows, and set the initial
    // state on the windows.
    ComputeTerminalStatesForAllWindows();
    SetInitialWindowStates();
  }

  virtual ~WindowOverviewModeImpl() {
    container_->set_target_handler(container_->delegate());
    RemoveAnimationObserver();
    aura::Window::Windows windows = window_list_provider_->GetWindowList();
    if (windows.empty())
      return;
    std::for_each(windows.begin(), windows.end(), &RestoreWindowState);
  }

 private:
  // Computes the transforms for all windows in both the topmost and bottom-most
  // positions. The transforms are set in the |kWindowOverviewState| property of
  // the windows.
  void ComputeTerminalStatesForAllWindows() {
    aura::Window::Windows windows = window_list_provider_->GetWindowList();
    size_t window_count = windows.size();
    size_t index = 0;
    const gfx::Size container_size = container_->bounds().size();

    const int kGapBetweenWindowsBottom = 10;
    const int kGapBetweenWindowsTop = 5;

    for (aura::Window::Windows::const_reverse_iterator iter = windows.rbegin();
         iter != windows.rend();
         ++iter, ++index) {
      aura::Window* window = (*iter);

      gfx::Transform top_transform;
      int top = (window_count - index - 1) * kGapBetweenWindowsTop;
      float x_translate = container_size.width() * (1 - kMinScale) / 2.;
      top_transform.Translate(x_translate, top);
      top_transform.Scale(kMinScale, kMinScale);

      gfx::Transform bottom_transform;
      int bottom = GetScrollableHeight() - (index * kGapBetweenWindowsBottom);
      x_translate = container_size.width() * (1 - kMaxScale) / 2.;
      bottom_transform.Translate(x_translate, bottom - window->bounds().y());
      bottom_transform.Scale(kMaxScale, kMaxScale);

      WindowOverviewState* state = new WindowOverviewState;
      state->top = top_transform;
      state->bottom = bottom_transform;
      state->progress = 0.f;
      state->shadow = CreateShadowForWindow(window);
      window->SetProperty(kWindowOverviewState, state);
    }
  }

  // Sets the initial position for the windows for the overview mode.
  void SetInitialWindowStates() {
    aura::Window::Windows windows = window_list_provider_->GetWindowList();
    size_t window_count = windows.size();
    // The initial overview state of the topmost three windows.
    const float kInitialProgress[] = { 0.5f, 0.05f, 0.01f };
    for (size_t i = 0; i < window_count; ++i) {
      float progress = 0.f;
      aura::Window* window = windows[window_count - 1 - i];
      if (i < arraysize(kInitialProgress))
        progress = kInitialProgress[i];

      scoped_refptr<ui::LayerAnimator> animator =
          window->layer()->GetAnimator();

      // Unset any in-progress animation.
      {
        ui::ScopedLayerAnimationSettings settings(animator);
        settings.SetPreemptionStrategy(
            ui::LayerAnimator::IMMEDIATELY_SET_NEW_TARGET);
        window->Show();
        window->SetTransform(gfx::Transform());
      }
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

  scoped_ptr<wm::Shadow> CreateShadowForWindow(aura::Window* window) {
    scoped_ptr<wm::Shadow> shadow(new wm::Shadow());
    shadow->Init(wm::Shadow::STYLE_ACTIVE);
    shadow->SetContentBounds(gfx::Rect(container_->bounds().size()));
    shadow->layer()->SetVisible(true);
    window->layer()->Add(shadow->layer());
    return shadow.Pass();
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
    aura::Window::Windows windows = window_list_provider_->GetWindowList();
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
    const float kScrollableFraction = 0.65f;
    return container_->bounds().height() * kScrollableFraction;
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

  void DragWindow(const ui::GestureEvent& event) {
    CHECK(dragged_window_);
    CHECK_EQ(ui::ET_GESTURE_SCROLL_UPDATE, event.type());
    gfx::Vector2dF dragged_distance =
        dragged_start_location_ - event.location();
    WindowOverviewState* dragged_state =
        dragged_window_->GetProperty(kWindowOverviewState);
    CHECK(dragged_state);
    gfx::Transform transform = GetTransformForState(dragged_state);
    transform.Translate(-dragged_distance.x(), 0);
    dragged_window_->SetTransform(transform);

    float ratio = std::min(
        1.f, std::abs(dragged_distance.x()) / kMinDistanceForDismissal);
    float opacity =
        gfx::Tween::FloatValueBetween(ratio, kMaxOpacity, kMinOpacity);
    dragged_window_->layer()->SetOpacity(opacity);
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
    ui::ScopedLayerAnimationSettings settings(
        dragged_window_->layer()->GetAnimator());
    settings.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    settings.AddObserver(new ClosureAnimationObserver(
        base::Bind(&base::DeletePointer<aura::Window>, dragged_window_)));

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
    float scale = gfx::Tween::FloatValueBetween(
        dragged_state->progress, kMinScale, kMaxScale);
    transform.Translate(transform_x / scale, 0);
    dragged_window_->SetTransform(transform);
    dragged_window_->layer()->SetOpacity(kMinOpacity);

    // Move the windows behind |dragged_window_| in the stack forward one step.
    const aura::Window::Windows& list = container_->children();
    for (aura::Window::Windows::const_iterator iter = list.begin();
         iter != list.end() && *iter != dragged_window_;
         ++iter) {
      aura::Window* window = *iter;
      ui::ScopedLayerAnimationSettings settings(window->layer()->GetAnimator());
      settings.SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
      WindowOverviewState* state = window->GetProperty(kWindowOverviewState);

      aura::Window* next = *(iter + 1);
      WindowOverviewState* next_state = next->GetProperty(kWindowOverviewState);
      state->top = next_state->top;
      state->bottom = next_state->bottom;
      SetWindowProgress(window, next_state->progress);
    }

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
    dragged_window_->SetTransform(GetTransformForState(dragged_state));
    dragged_window_->layer()->SetOpacity(1.f);
    dragged_window_ = NULL;
  }

  // ui::EventHandler:
  virtual void OnMouseEvent(ui::MouseEvent* mouse) OVERRIDE {
    if (mouse->type() == ui::ET_MOUSE_PRESSED) {
      aura::Window* select = SelectWindowAt(mouse);
      if (select) {
        mouse->SetHandled();
        delegate_->OnSelectWindow(select);
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
        delegate_->OnSelectWindow(select);
      }
    } else if (gesture->type() == ui::ET_GESTURE_SCROLL_BEGIN) {
      if (std::abs(gesture->details().scroll_x_hint()) >
          std::abs(gesture->details().scroll_y_hint()) * 2) {
        dragged_start_location_ = gesture->location();
        dragged_window_ = SelectWindowAt(gesture);
      }
    } else if (gesture->type() == ui::ET_GESTURE_SCROLL_UPDATE) {
      if (dragged_window_)
        DragWindow(*gesture);
      else
        DoScroll(gesture->details().scroll_y());
      gesture->SetHandled();
    } else if (gesture->type() == ui::ET_GESTURE_SCROLL_END) {
      if (dragged_window_) {
        if (ShouldCloseDragWindow(*gesture))
          CloseDragWindow(*gesture);
        else
          RestoreDragWindow();
      }
      gesture->SetHandled();
    } else if (gesture->type() == ui::ET_SCROLL_FLING_START) {
      if (dragged_window_) {
        if (ShouldCloseDragWindow(*gesture))
          CloseDragWindow(*gesture);
        else
          RestoreDragWindow();
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

  const int kMinDistanceForDismissal = 300;
  const float kMinScale = 0.6f;
  const float kMaxScale = 0.95f;
  const float kMaxOpacity = 1.0f;
  const float kMinOpacity = 0.2f;

  aura::Window* container_;
  // Provider of the stack of windows to show in the overview mode. Not owned.
  const WindowListProvider* window_list_provider_;
  WindowOverviewModeDelegate* delegate_;
  scoped_ptr<aura::ScopedWindowTargeter> scoped_targeter_;
  scoped_ptr<ui::FlingCurve> fling_;

  aura::Window* dragged_window_;
  gfx::Point dragged_start_location_;

  DISALLOW_COPY_AND_ASSIGN(WindowOverviewModeImpl);
};

}  // namespace

// static
scoped_ptr<WindowOverviewMode> WindowOverviewMode::Create(
    aura::Window* container,
    const WindowListProvider* window_list_provider,
    WindowOverviewModeDelegate* delegate) {
  return scoped_ptr<WindowOverviewMode>(
      new WindowOverviewModeImpl(container, window_list_provider, delegate));
}

}  // namespace athena
