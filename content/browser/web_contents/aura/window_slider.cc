// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/aura/window_slider.h"

#include <algorithm>

#include "base/bind.h"
#include "base/callback.h"
#include "content/browser/web_contents/aura/shadow_layer_delegate.h"
#include "content/public/browser/overscroll_configuration.h"
#include "ui/aura/window.h"
#include "ui/base/events/event.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"

namespace content {

namespace {

void DeleteLayerAndShadow(ui::Layer* layer,
                          ShadowLayerDelegate* shadow) {
  delete shadow;
  delete layer;
}

// An animation observer that runs a callback at the end of the animation, and
// destroys itself.
class CallbackAnimationObserver : public ui::ImplicitAnimationObserver {
 public:
  CallbackAnimationObserver(const base::Closure& closure)
      : closure_(closure) {
  }

  virtual ~CallbackAnimationObserver() {}

 private:
  // Overridden from ui::ImplicitAnimationObserver:
  virtual void OnImplicitAnimationsCompleted() OVERRIDE {
    if (!closure_.is_null())
      closure_.Run();
    base::MessageLoop::current()->DeleteSoon(FROM_HERE, this);
  }

  const base::Closure closure_;

  DISALLOW_COPY_AND_ASSIGN(CallbackAnimationObserver);
};

}  // namespace

WindowSlider::WindowSlider(Delegate* delegate,
                           aura::Window* event_window,
                           aura::Window* owner)
    : delegate_(delegate),
      event_window_(event_window),
      owner_(owner),
      delta_x_(0.f),
      weak_factory_(this),
      horiz_start_threshold_(content::GetOverscrollConfig(
          content::OVERSCROLL_CONFIG_HORIZ_THRESHOLD_START)),
      complete_threshold_(content::GetOverscrollConfig(
          content::OVERSCROLL_CONFIG_HORIZ_THRESHOLD_COMPLETE)) {
  event_window_->AddPreTargetHandler(this);

  event_window_->AddObserver(this);
  owner_->AddObserver(this);
}

WindowSlider::~WindowSlider() {
  if (event_window_) {
    event_window_->RemovePreTargetHandler(this);
    event_window_->RemoveObserver(this);
  }
  if (owner_)
    owner_->RemoveObserver(this);
  delegate_->OnWindowSliderDestroyed();
}

void WindowSlider::ChangeOwner(aura::Window* new_owner) {
  if (owner_)
    owner_->RemoveObserver(this);
  owner_ = new_owner;
  if (owner_) {
    owner_->AddObserver(this);
    UpdateForScroll(0.f, 0.f);
  }
}

bool WindowSlider::IsSlideInProgress() const {
  return fabs(delta_x_) >= horiz_start_threshold_ || slider_.get() ||
      weak_factory_.HasWeakPtrs();
}

void WindowSlider::SetupSliderLayer() {
  ui::Layer* parent = owner_->layer()->parent();
  parent->Add(slider_.get());
  if (delta_x_ < 0)
    parent->StackAbove(slider_.get(), owner_->layer());
  else
    parent->StackBelow(slider_.get(), owner_->layer());
  slider_->SetBounds(owner_->layer()->bounds());
  slider_->SetVisible(true);
}

void WindowSlider::UpdateForScroll(float x_offset, float y_offset) {
  float old_delta = delta_x_;
  delta_x_ += x_offset;
  if (fabs(delta_x_) < horiz_start_threshold_ && !slider_.get())
    return;

  if ((old_delta < 0 && delta_x_ > 0) ||
      (old_delta > 0 && delta_x_ < 0)) {
    slider_.reset();
    shadow_.reset();
  }

  float translate = 0.f;
  ui::Layer* translate_layer = NULL;

  if (!slider_.get()) {
    slider_.reset(delta_x_ < 0 ? delegate_->CreateFrontLayer() :
                                 delegate_->CreateBackLayer());
    if (!slider_.get())
      return;
    SetupSliderLayer();
  }

  if (delta_x_ <= -horiz_start_threshold_) {
    translate = owner_->bounds().width() +
        std::max(delta_x_ + horiz_start_threshold_,
                 static_cast<float>(-owner_->bounds().width()));
    translate_layer = slider_.get();
  } else if (delta_x_ >= horiz_start_threshold_) {
    translate = std::min(delta_x_ - horiz_start_threshold_,
                         static_cast<float>(owner_->bounds().width()));
    translate_layer = owner_->layer();
  } else {
    return;
  }

  if (!shadow_.get())
    shadow_.reset(new ShadowLayerDelegate(translate_layer));

  gfx::Transform transform;
  transform.Translate(translate, 0);
  translate_layer->SetTransform(transform);
}

void WindowSlider::UpdateForFling(float x_velocity, float y_velocity) {
  if (!slider_.get())
    return;

  int width = owner_->bounds().width();
  float ratio = (fabs(delta_x_) - horiz_start_threshold_) / width;
  if (ratio < complete_threshold_) {
    ResetScroll();
    return;
  }

  ui::Layer* sliding = delta_x_ < 0 ? slider_.get() : owner_->layer();
  ui::ScopedLayerAnimationSettings settings(sliding->GetAnimator());
  settings.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  settings.SetTweenType(ui::Tween::EASE_OUT);
  settings.AddObserver(new CallbackAnimationObserver(
      base::Bind(&WindowSlider::CompleteWindowSlideAfterAnimation,
                 weak_factory_.GetWeakPtr())));

  gfx::Transform transform;
  transform.Translate(delta_x_ < 0 ? 0 : width, 0);
  sliding->SetTransform(transform);
}

void WindowSlider::ResetScroll() {
  if (!slider_.get())
    return;

  // Do not trigger any callbacks if this animation replaces any in-progress
  // animation.
  weak_factory_.InvalidateWeakPtrs();

  // Reset the state of the sliding layer.
  if (slider_.get()) {
    ui::Layer* layer = slider_.release();
    ui::ScopedLayerAnimationSettings settings(layer->GetAnimator());
    settings.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    settings.SetTweenType(ui::Tween::EASE_OUT);

    // Delete the layer and the shadow at the end of the animation.
    settings.AddObserver(new CallbackAnimationObserver(
        base::Bind(&DeleteLayerAndShadow,
                   base::Unretained(layer),
                   base::Unretained(shadow_.release()))));

    gfx::Transform transform;
    transform.Translate(delta_x_ < 0 ? layer->bounds().width() : 0, 0);
    layer->SetTransform(transform);
  }

  // Reset the state of the main layer.
  {
    ui::ScopedLayerAnimationSettings settings(owner_->layer()->GetAnimator());
    settings.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    settings.SetTweenType(ui::Tween::EASE_OUT);
    settings.AddObserver(new CallbackAnimationObserver(
        base::Bind(&WindowSlider::AbortWindowSlideAfterAnimation,
                   weak_factory_.GetWeakPtr())));
    owner_->layer()->SetTransform(gfx::Transform());
    owner_->layer()->SetLayerBrightness(0.f);
  }

  delta_x_ = 0.f;
}

void WindowSlider::CancelScroll() {
  ResetScroll();
}

void WindowSlider::CompleteWindowSlideAfterAnimation() {
  weak_factory_.InvalidateWeakPtrs();
  shadow_.reset();
  slider_.reset();
  delta_x_ = 0.f;

  delegate_->OnWindowSlideComplete();
}

void WindowSlider::AbortWindowSlideAfterAnimation() {
  weak_factory_.InvalidateWeakPtrs();

  delegate_->OnWindowSlideAborted();
}

void WindowSlider::OnKeyEvent(ui::KeyEvent* event) {
  CancelScroll();
}

void WindowSlider::OnMouseEvent(ui::MouseEvent* event) {
  if (!(event->flags() & ui::EF_IS_SYNTHESIZED))
    CancelScroll();
}

void WindowSlider::OnScrollEvent(ui::ScrollEvent* event) {
  if (event->type() == ui::ET_SCROLL)
    UpdateForScroll(event->x_offset_ordinal(), event->y_offset_ordinal());
  else if (event->type() == ui::ET_SCROLL_FLING_START)
    UpdateForFling(event->x_offset_ordinal(), event->y_offset_ordinal());
  else
    CancelScroll();
  event->SetHandled();
}

void WindowSlider::OnGestureEvent(ui::GestureEvent* event) {
  const ui::GestureEventDetails& details = event->details();
  switch (event->type()) {
    case ui::ET_GESTURE_SCROLL_BEGIN:
      ResetScroll();
      break;

    case ui::ET_GESTURE_SCROLL_UPDATE:
      UpdateForScroll(details.scroll_x(), details.scroll_y());
      break;

    case ui::ET_GESTURE_SCROLL_END:
      UpdateForFling(0.f, 0.f);
      break;

    case ui::ET_SCROLL_FLING_START:
      UpdateForFling(details.velocity_x(), details.velocity_y());
      break;

    case ui::ET_GESTURE_PINCH_BEGIN:
    case ui::ET_GESTURE_PINCH_UPDATE:
    case ui::ET_GESTURE_PINCH_END:
      CancelScroll();
      break;

    default:
      break;
  }

  event->SetHandled();
}

void WindowSlider::OnWindowRemovingFromRootWindow(aura::Window* window) {
  if (window == event_window_) {
    window->RemoveObserver(this);
    window->RemovePreTargetHandler(this);
    event_window_ = NULL;
  } else if (window == owner_) {
    window->RemoveObserver(this);
    owner_ = NULL;
    delete this;
  } else {
    NOTREACHED();
  }
}

}  // namespace content
