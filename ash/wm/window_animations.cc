// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_animations.h"

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/gfx/compositor/layer_animation_observer.h"
#include "ui/gfx/compositor/scoped_layer_animation_settings.h"

namespace ash {
namespace internal {
const char kWindowVisibilityAnimationTypeKey[] =
    "WindowVisibilityAnimationType";
}  // namespace internal

void SetWindowVisibilityAnimationType(aura::Window* window,
                                      WindowVisibilityAnimationType type) {
  window->SetIntProperty(internal::kWindowVisibilityAnimationTypeKey, type);
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
      static_cast<WindowVisibilityAnimationType>(
          window->GetIntProperty(kWindowVisibilityAnimationTypeKey));
  if (type == WINDOW_VISIBILITY_ANIMATION_TYPE_DEFAULT) {
    return window->type() == aura::client::WINDOW_TYPE_MENU ?
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
  virtual ~HidingWindowAnimationObserver() {}

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
    layer_.reset(window_->AcquireLayer());
    window_->RemoveObserver(this);
    window_ = NULL;
  }

  ui::Layer* layer() { return window_ ? window_->layer() : layer_.get(); }

  aura::Window* window_;
  scoped_ptr<ui::Layer> layer_;

  DISALLOW_COPY_AND_ASSIGN(HidingWindowAnimationObserver);
};

// Shows a window using an animation, animating its opacity from 0.f to 1.f, and
// its transform from |start_transform| to |end_transform|.
void AnimateShowWindowCommon(aura::Window* window,
                             const ui::Transform& start_transform,
                             const ui::Transform& end_transform) {
  window->layer()->SetOpacity(kWindowAnimation_HideOpacity);
  window->layer()->SetTransform(start_transform);

  {
    // Property sets within this scope will be implicitly animated.
    ui::ScopedLayerAnimationSettings settings(window->layer()->GetAnimator());
    window->layer()->SetTransform(end_transform);
    window->layer()->SetOpacity(kWindowAnimation_ShowOpacity);
  }
}

// Hides a window using an animation, animating its opacity from 1.f to 0.f, and
// its transform to |end_transform|.
void AnimateHideWindowCommon(aura::Window* window,
                             const ui::Transform& end_transform) {
  // The window's layer was just hidden, but we need it to draw until it's fully
  // transparent, so we show it again. This is undone once the animation is
  // complete.
  window->layer()->SetVisible(true);
  {
    // Property sets within this scope will be implicitly animated.
    ui::ScopedLayerAnimationSettings settings(window->layer()->GetAnimator());
    settings.AddImplicitObserver(new HidingWindowAnimationObserver(window));

    window->layer()->SetOpacity(kWindowAnimation_HideOpacity);
    window->layer()->SetTransform(end_transform);
  }
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

void AnimateShowWindow(aura::Window* window) {
  switch (GetWindowVisibilityAnimationType(window)) {
    case WINDOW_VISIBILITY_ANIMATION_TYPE_DROP:
      AnimateShowWindow_Drop(window);
      break;
    case WINDOW_VISIBILITY_ANIMATION_TYPE_VERTICAL:
      AnimateShowWindow_Vertical(window);
      break;
    case WINDOW_VISIBILITY_ANIMATION_TYPE_FADE:
      AnimateShowWindow_Fade(window);
      break;
    default:
      NOTREACHED();
      break;
  }
}

void AnimateHideWindow(aura::Window* window) {
  switch (GetWindowVisibilityAnimationType(window)) {
    case WINDOW_VISIBILITY_ANIMATION_TYPE_DROP:
      AnimateHideWindow_Drop(window);
      break;
    case WINDOW_VISIBILITY_ANIMATION_TYPE_VERTICAL:
      AnimateHideWindow_Vertical(window);
      break;
    case WINDOW_VISIBILITY_ANIMATION_TYPE_FADE:
      AnimateHideWindow_Fade(window);
      break;
    default:
      NOTREACHED();
      break;
  }
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// WindowAnimation, public:

void AnimateOnChildWindowVisibilityChanged(aura::Window* window, bool visible) {
  if (window->GetIntProperty(aura::client::kAnimationsDisabledKey) == 1)
    return;
  if (visible) {
    AnimateShowWindow(window);
  } else {
    // Don't start hiding the window again if it's already being hidden.
    if (window->layer()->GetTargetOpacity() != 0.0f)
      AnimateHideWindow(window);
  }
}

}  // namespace internal
}  // namespace ash
