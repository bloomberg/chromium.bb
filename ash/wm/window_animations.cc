// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_animations.h"

#include "ash/ash_switches.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/stl_util.h"
#include "base/time.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/aura/window_property.h"
#include "ui/gfx/compositor/layer_animation_observer.h"
#include "ui/gfx/compositor/scoped_layer_animation_settings.h"

DECLARE_WINDOW_PROPERTY_TYPE(int)
DECLARE_WINDOW_PROPERTY_TYPE(ash::WindowVisibilityAnimationType)
DECLARE_WINDOW_PROPERTY_TYPE(ash::WindowVisibilityAnimationTransition)

namespace ash {
namespace internal {
namespace {

const aura::WindowProperty<WindowVisibilityAnimationType>
    kWindowVisibilityAnimationTypeProp =
        {WINDOW_VISIBILITY_ANIMATION_TYPE_DEFAULT};
const aura::WindowProperty<int> kWindowVisibilityAnimationDurationProp = {0};
const aura::WindowProperty<WindowVisibilityAnimationTransition>
    kWindowVisibilityAnimationTransitionProp = {ANIMATE_BOTH};

}  // namespace

const aura::WindowProperty<WindowVisibilityAnimationType>* const
    kWindowVisibilityAnimationTypeKey = &kWindowVisibilityAnimationTypeProp;
const aura::WindowProperty<int>* const kWindowVisibilityAnimationDurationKey =
    &kWindowVisibilityAnimationDurationProp;
const aura::WindowProperty<WindowVisibilityAnimationTransition>* const
    kWindowVisibilityAnimationTransitionKey =
        &kWindowVisibilityAnimationTransitionProp;

}  // namespace internal

void SetWindowVisibilityAnimationType(aura::Window* window,
                                      WindowVisibilityAnimationType type) {
  window->SetProperty(internal::kWindowVisibilityAnimationTypeKey, type);
}

void SetWindowVisibilityAnimationTransition(
    aura::Window* window,
    WindowVisibilityAnimationTransition transition) {
  window->SetProperty(internal::kWindowVisibilityAnimationTransitionKey,
                      transition);
}

void SetWindowVisibilityAnimationDuration(aura::Window* window,
                                          const base::TimeDelta& duration) {
  window->SetProperty(internal::kWindowVisibilityAnimationDurationKey,
                      static_cast<int>(duration.ToInternalValue()));
}

bool HasWindowVisibilityAnimationTransition(
    aura::Window* window,
    WindowVisibilityAnimationTransition transition) {
  WindowVisibilityAnimationTransition prop = window->GetProperty(
      internal::kWindowVisibilityAnimationTransitionKey);
  return (prop & transition) != 0;
}

namespace internal {
namespace {

const float kWindowAnimation_HideOpacity = 0.f;
const float kWindowAnimation_ShowOpacity = 1.f;
const float kWindowAnimation_TranslateFactor = -0.025f;
const float kWindowAnimation_ScaleFactor = 1.05f;

const float kWindowAnimation_Vertical_TranslateY = 15.f;

// Gets/sets the WindowVisibilityAnimationType associated with a window.
WindowVisibilityAnimationType GetWindowVisibilityAnimationType(
    aura::Window* window) {
  WindowVisibilityAnimationType type =
      window->GetProperty(kWindowVisibilityAnimationTypeKey);
  if (type == WINDOW_VISIBILITY_ANIMATION_TYPE_DEFAULT) {
    return (window->type() == aura::client::WINDOW_TYPE_MENU ||
            window->type() == aura::client::WINDOW_TYPE_TOOLTIP) ?
        WINDOW_VISIBILITY_ANIMATION_TYPE_FADE :
        WINDOW_VISIBILITY_ANIMATION_TYPE_DROP;
  }
  return type;
}

// Observes a hide animation.
// A window can be hidden for a variety of reasons. Sometimes, Hide() will be
// called and life is simple. Sometimes, the window is actually bound to a
// views::Widget and that Widget is closed, and life is a little more
// complicated. When a Widget is closed the aura::Window* is actually not
// destroyed immediately - it is actually just immediately hidden and then
// destroyed when the stack unwinds. To handle this case, we start the hide
// animation immediately when the window is hidden, then when the window is
// subsequently destroyed this object acquires ownership of the window's layer,
// so that it can continue animating it until the animation completes.
// Regardless of whether or not the window is destroyed, this object deletes
// itself when the animation completes.
class HidingWindowAnimationObserver : public ui::ImplicitAnimationObserver,
                                      public aura::WindowObserver {
 public:
  explicit HidingWindowAnimationObserver(aura::Window* window)
      : window_(window) {
    window_->AddObserver(this);
  }
  virtual ~HidingWindowAnimationObserver() {
    STLDeleteElements(&layers_);
  }

 private:
  // Overridden from ui::ImplicitAnimationObserver:
  virtual void OnImplicitAnimationsCompleted() OVERRIDE {
    // Restore the correct visibility value (overridden for the duration of the
    // animation in AnimateHideWindow()).
    layer()->SetVisible(false);
    // Window may have been destroyed by this point.
    if (window_)
      window_->RemoveObserver(this);
    MessageLoop::current()->DeleteSoon(FROM_HERE, this);
  }

  // Overridden from aura::WindowObserver:
  virtual void OnWindowDestroying(aura::Window* window) OVERRIDE {
    DCHECK_EQ(window, window_);
    DCHECK(layers_.empty());
    AcquireAllLayers(window_);
    window_->RemoveObserver(this);
    window_ = NULL;
  }

  void AcquireAllLayers(aura::Window* window) {
    ui::Layer* layer = window->AcquireLayer();
    DCHECK(layer);
    layers_.push_back(layer);
    for (aura::Window::Windows::const_iterator it = window->children().begin();
         it != window->children().end();
         ++it)
      AcquireAllLayers(*it);
  }

  ui::Layer* layer() { return window_ ? window_->layer() : layers_[0]; }

  aura::Window* window_;
  std::vector<ui::Layer*> layers_;

  DISALLOW_COPY_AND_ASSIGN(HidingWindowAnimationObserver);
};

// Shows a window using an animation, animating its opacity from 0.f to 1.f, and
// its transform from |start_transform| to |end_transform|.
void AnimateShowWindowCommon(aura::Window* window,
                             const ui::Transform& start_transform,
                             const ui::Transform& end_transform) {
  window->layer()->set_delegate(window);
  window->layer()->SetOpacity(kWindowAnimation_HideOpacity);
  window->layer()->SetTransform(start_transform);

  {
    // Property sets within this scope will be implicitly animated.
    ui::ScopedLayerAnimationSettings settings(window->layer()->GetAnimator());
    int duration =
        window->GetProperty(internal::kWindowVisibilityAnimationDurationKey);
    if (duration > 0) {
      settings.SetTransitionDuration(
          base::TimeDelta::FromInternalValue(duration));
    }

    window->layer()->SetTransform(end_transform);
    window->layer()->SetOpacity(kWindowAnimation_ShowOpacity);
  }
}

// Hides a window using an animation, animating its opacity from 1.f to 0.f, and
// its transform to |end_transform|.
void AnimateHideWindowCommon(aura::Window* window,
                             const ui::Transform& end_transform) {
  window->layer()->set_delegate(NULL);

  // Property sets within this scope will be implicitly animated.
  ui::ScopedLayerAnimationSettings settings(window->layer()->GetAnimator());
  settings.AddObserver(new HidingWindowAnimationObserver(window));

  int duration =
      window->GetProperty(internal::kWindowVisibilityAnimationDurationKey);
  if (duration > 0) {
    settings.SetTransitionDuration(
        base::TimeDelta::FromInternalValue(duration));
  }

  window->layer()->SetOpacity(kWindowAnimation_HideOpacity);
  window->layer()->SetTransform(end_transform);
}

// Show/Hide windows using a shrink animation.
void AnimateShowWindow_Drop(aura::Window* window) {
  ui::Transform transform;
  transform.ConcatScale(kWindowAnimation_ScaleFactor,
                        kWindowAnimation_ScaleFactor);
  transform.ConcatTranslate(
      kWindowAnimation_TranslateFactor * window->bounds().width(),
      kWindowAnimation_TranslateFactor * window->bounds().height());
  AnimateShowWindowCommon(window, transform, ui::Transform());
}

void AnimateHideWindow_Drop(aura::Window* window) {
  ui::Transform transform;
  transform.ConcatScale(kWindowAnimation_ScaleFactor,
                        kWindowAnimation_ScaleFactor);
  transform.ConcatTranslate(
      kWindowAnimation_TranslateFactor * window->bounds().width(),
      kWindowAnimation_TranslateFactor * window->bounds().height());
  AnimateHideWindowCommon(window, transform);
}

// Show/Hide windows using a vertical Glenimation.
void AnimateShowWindow_Vertical(aura::Window* window) {
  ui::Transform transform;
  transform.ConcatTranslate(0, kWindowAnimation_Vertical_TranslateY);
  AnimateShowWindowCommon(window, transform, ui::Transform());
}

void AnimateHideWindow_Vertical(aura::Window* window) {
  ui::Transform transform;
  transform.ConcatTranslate(0, kWindowAnimation_Vertical_TranslateY);
  AnimateHideWindowCommon(window, transform);
}

// Show/Hide windows using a fade.
void AnimateShowWindow_Fade(aura::Window* window) {
  AnimateShowWindowCommon(window, ui::Transform(), ui::Transform());
}

void AnimateHideWindow_Fade(aura::Window* window) {
  AnimateHideWindowCommon(window, ui::Transform());
}

bool AnimateShowWindow(aura::Window* window) {
  if (!HasWindowVisibilityAnimationTransition(window, ANIMATE_SHOW))
    return false;

  switch (GetWindowVisibilityAnimationType(window)) {
    case WINDOW_VISIBILITY_ANIMATION_TYPE_DROP:
      AnimateShowWindow_Drop(window);
      return true;
    case WINDOW_VISIBILITY_ANIMATION_TYPE_VERTICAL:
      AnimateShowWindow_Vertical(window);
      return true;
    case WINDOW_VISIBILITY_ANIMATION_TYPE_FADE:
      AnimateShowWindow_Fade(window);
      return true;
    default:
      NOTREACHED();
      return false;
  }
}

bool AnimateHideWindow(aura::Window* window) {
  if (!HasWindowVisibilityAnimationTransition(window, ANIMATE_HIDE))
    return false;

  switch (GetWindowVisibilityAnimationType(window)) {
    case WINDOW_VISIBILITY_ANIMATION_TYPE_DROP:
      AnimateHideWindow_Drop(window);
      return true;
    case WINDOW_VISIBILITY_ANIMATION_TYPE_VERTICAL:
      AnimateHideWindow_Vertical(window);
      return true;
    case WINDOW_VISIBILITY_ANIMATION_TYPE_FADE:
      AnimateHideWindow_Fade(window);
      return true;
    default:
      NOTREACHED();
      return false;
  }
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// WindowAnimation, public:

bool AnimateOnChildWindowVisibilityChanged(aura::Window* window, bool visible) {
  if (window->GetProperty(aura::client::kAnimationsDisabledKey) ||
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAuraWindowAnimationsDisabled)) {
    return false;
  }
  if (visible) {
    return AnimateShowWindow(window);
  } else {
    // Don't start hiding the window again if it's already being hidden.
    return window->layer()->GetTargetOpacity() != 0.0f &&
        AnimateHideWindow(window);
  }
}

}  // namespace internal
}  // namespace ash
