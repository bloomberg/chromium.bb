// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_animations.h"

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/gfx/compositor/layer_animation_observer.h"

namespace ash {
namespace internal {
namespace {

const float kWindowAnimation_HideOpacity = 0.0f;
const float kWindowAnimation_ShowOpacity = 1.0f;
const float kWindowAnimation_TranslateFactor = -0.025f;
const float kWindowAnimation_ScaleFactor = 1.05f;

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

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// WindowAnimation, public:

void AnimateShowWindow(aura::Window* window) {
  // Set the start state pre-animation.
  window->layer()->SetOpacity(kWindowAnimation_HideOpacity);
  ui::Transform transform;
  transform.ConcatScale(kWindowAnimation_ScaleFactor,
                        kWindowAnimation_ScaleFactor);
  transform.ConcatTranslate(
      kWindowAnimation_TranslateFactor * window->bounds().width(),
      kWindowAnimation_TranslateFactor * window->bounds().height());
  window->layer()->SetTransform(transform);

  {
    // Property sets within this scope will be implicitly animated.
    ui::LayerAnimator::ScopedSettings settings(window->layer()->GetAnimator());
    window->layer()->SetTransform(ui::Transform());
    window->layer()->SetOpacity(kWindowAnimation_ShowOpacity);
  }
}

void AnimateHideWindow(aura::Window* window) {
  // The window's layer was just hidden, but we need it to draw until it's fully
  // transparent, so we show it again. This is undone once the animation is
  // complete.
  window->layer()->SetVisible(true);
  {
    // Property sets within this scope will be implicitly animated.
    ui::LayerAnimator::ScopedSettings settings(window->layer()->GetAnimator());
    settings.AddImplicitObserver(new HidingWindowAnimationObserver(window));

    window->layer()->SetOpacity(kWindowAnimation_HideOpacity);

    ui::Transform transform;
    transform.ConcatScale(kWindowAnimation_ScaleFactor,
                          kWindowAnimation_ScaleFactor);
    transform.ConcatTranslate(
        kWindowAnimation_TranslateFactor * window->bounds().width(),
        kWindowAnimation_TranslateFactor * window->bounds().height());
    window->layer()->SetTransform(transform);
  }
}

}  // namespace internal
}  // namespace ash
