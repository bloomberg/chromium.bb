// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/wm/window_overview_mode.h"

#include <algorithm>
#include <functional>
#include <vector>

#include "base/macros.h"
#include "ui/aura/scoped_window_targeter.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_property.h"
#include "ui/aura/window_targeter.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/events/event_handler.h"
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

bool ShouldShowWindowInOverviewMode(aura::Window* window) {
  return window->type() == ui::wm::WINDOW_TYPE_NORMAL;
}

// Sets the progress-state for the window in the overview mode.
void SetWindowProgress(aura::Window* window, float progress) {
  WindowOverviewState* state = window->GetProperty(kWindowOverviewState);
  gfx::Transform transform =
      gfx::Tween::TransformValueBetween(progress, state->top, state->bottom);
  window->SetTransform(transform);
  state->progress = progress;
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
                               public ui::EventHandler {
 public:
  WindowOverviewModeImpl(aura::Window* container,
                         WindowOverviewModeDelegate* delegate)
      : container_(container),
        delegate_(delegate),
        scoped_targeter_(new aura::ScopedWindowTargeter(
            container,
            scoped_ptr<ui::EventTargeter>(
                new StaticWindowTargeter(container)))) {
    container_->set_target_handler(this);

    // Prepare the desired transforms for all the windows, and set the initial
    // state on the windows.
    ComputeTerminalStatesForAllWindows();
    SetInitialWindowStates();
  }

  virtual ~WindowOverviewModeImpl() {
    container_->set_target_handler(container_->delegate());

    const aura::Window::Windows& windows = container_->children();
    for (aura::Window::Windows::const_iterator iter = windows.begin();
         iter != windows.end();
         ++iter) {
      if ((*iter)->GetProperty(kWindowOverviewState))
        RestoreWindowState(*iter);
    }
  }

 private:
  // Computes the transforms for all windows in both the topmost and bottom-most
  // positions. The transforms are set in the |kWindowOverviewState| property of
  // the windows.
  void ComputeTerminalStatesForAllWindows() {
    const aura::Window::Windows& windows = container_->children();
    size_t window_count = std::count_if(windows.begin(), windows.end(),
                                        ShouldShowWindowInOverviewMode);

    size_t index = 0;
    const gfx::Size container_size = container_->bounds().size();

    const int kGapBetweenWindowsBottom = 10;
    const int kGapBetweenWindowsTop = 5;
    const float kMinScale = 0.6f;
    const float kMaxScale = 0.95f;

    for (aura::Window::Windows::const_reverse_iterator iter = windows.rbegin();
         iter != windows.rend();
         ++iter) {
      aura::Window* window = (*iter);
      if (!ShouldShowWindowInOverviewMode(window))
        continue;

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

      index++;
    }
  }

  // Sets the initial position for the windows for the overview mode.
  void SetInitialWindowStates() {
    // The initial overview state of the topmost three windows.
    const float kInitialProgress[] = { 0.5f, 0.05f, 0.01f };
    size_t index = 0;
    const aura::Window::Windows& windows = container_->children();
    for (aura::Window::Windows::const_reverse_iterator iter = windows.rbegin();
         iter != windows.rend();
         ++iter) {
      aura::Window* window = (*iter);
      if (!window->GetProperty(kWindowOverviewState))
        continue;

      float progress = 0.f;
      if (index < arraysize(kInitialProgress))
        progress = kInitialProgress[index];

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
      index++;
    }
  }

  scoped_ptr<wm::Shadow> CreateShadowForWindow(aura::Window* window) {
    scoped_ptr<wm::Shadow> shadow(new wm::Shadow());
    shadow->Init(wm::Shadow::STYLE_ACTIVE);
    shadow->SetContentBounds(gfx::Rect(window->bounds().size()));
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
    const aura::Window::Windows& windows = container_->children();
    if (delta_y < 0) {
      // Scroll up. Start with the top-most (i.e. behind-most in terms of
      // z-index) window, and try to scroll them up.
      for (aura::Window::Windows::const_iterator iter = windows.begin();
           delta_y_p > kEpsilon && iter != windows.end();
           ++iter) {
        aura::Window* window = (*iter);
        WindowOverviewState* state = window->GetProperty(kWindowOverviewState);
        if (!state)
          continue;
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
      for (aura::Window::Windows::const_reverse_iterator iter =
               windows.rbegin();
           delta_y_p > kEpsilon && iter != windows.rend();
           ++iter) {
        aura::Window* window = (*iter);
        WindowOverviewState* state = window->GetProperty(kWindowOverviewState);
        if (!state)
          continue;
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
    } else if (gesture->type() == ui::ET_GESTURE_SCROLL_UPDATE) {
      DoScroll(gesture->details().scroll_y());
    }
  }

  aura::Window* container_;
  WindowOverviewModeDelegate* delegate_;
  scoped_ptr<aura::ScopedWindowTargeter> scoped_targeter_;

  DISALLOW_COPY_AND_ASSIGN(WindowOverviewModeImpl);
};

}  // namespace

// static
scoped_ptr<WindowOverviewMode> WindowOverviewMode::Create(
    aura::Window* container,
    WindowOverviewModeDelegate* delegate) {
  return scoped_ptr<WindowOverviewMode>(
      new WindowOverviewModeImpl(container, delegate));
}

}  // namespace athena
