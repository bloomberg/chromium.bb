// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/session_state_animator.h"

#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ui/aura/root_window.h"
#include "ui/compositor/layer_animation_sequence.h"

namespace ash {
namespace internal {

namespace {

// Amount of time taken to scale the snapshot of the screen down to a
// slightly-smaller size once the user starts holding the power button.  Used
// for both the pre-lock and pre-shutdown animations.
const int kSlowCloseAnimMs = 400;

// Amount of time taken to scale the snapshot of the screen back to its original
// size when the button is released.
const int kUndoSlowCloseAnimMs = 100;

// Amount of time taken to scale the snapshot down to a point in the center of
// the screen once the screen has been locked or we've been notified that the
// system is shutting down.
const int kFastCloseAnimMs = 150;

// Amount of time taken to make the lock window fade in when the screen is
// locked.
const int kLockFadeInAnimMs = 200;

// Slightly-smaller size that we scale the screen down to for the pre-lock and
// pre-shutdown states.
const float kSlowCloseSizeRatio = 0.95f;

// Returns the transform that should be applied to containers for the slow-close
// animation.
ui::Transform GetSlowCloseTransform() {
  gfx::Size root_size = Shell::GetPrimaryRootWindow()->bounds().size();
  ui::Transform transform;
  transform.SetScale(kSlowCloseSizeRatio, kSlowCloseSizeRatio);
  transform.ConcatTranslate(
      floor(0.5 * (1.0 - kSlowCloseSizeRatio) * root_size.width() + 0.5),
      floor(0.5 * (1.0 - kSlowCloseSizeRatio) * root_size.height() + 0.5));
  return transform;
}

// Returns the transform that should be applied to containers for the fast-close
// animation.
ui::Transform GetFastCloseTransform() {
  gfx::Size root_size = Shell::GetPrimaryRootWindow()->bounds().size();
  ui::Transform transform;
  transform.SetScale(0.0, 0.0);
  transform.ConcatTranslate(floor(0.5 * root_size.width() + 0.5),
                            floor(0.5 * root_size.height() + 0.5));
  return transform;
}

// Slowly shrinks |window| to a slightly-smaller size.
void StartSlowCloseAnimationForWindow(aura::Window* window) {
  ui::LayerAnimator* animator = window->layer()->GetAnimator();
  animator->set_preemption_strategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  animator->StartAnimation(
      new ui::LayerAnimationSequence(
          ui::LayerAnimationElement::CreateTransformElement(
              GetSlowCloseTransform(),
              base::TimeDelta::FromMilliseconds(kSlowCloseAnimMs))));
}

// Quickly undoes the effects of the slow-close animation on |window|.
void StartUndoSlowCloseAnimationForWindow(aura::Window* window) {
  ui::LayerAnimator* animator = window->layer()->GetAnimator();
  animator->set_preemption_strategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  animator->StartAnimation(
      new ui::LayerAnimationSequence(
          ui::LayerAnimationElement::CreateTransformElement(
              ui::Transform(),
              base::TimeDelta::FromMilliseconds(kUndoSlowCloseAnimMs))));
}

// Quickly shrinks |window| down to a point in the center of the screen and
// fades it out to 0 opacity.
void StartFastCloseAnimationForWindow(aura::Window* window) {
  ui::LayerAnimator* animator = window->layer()->GetAnimator();
  animator->set_preemption_strategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  animator->StartAnimation(
      new ui::LayerAnimationSequence(
          ui::LayerAnimationElement::CreateTransformElement(
              GetFastCloseTransform(),
              base::TimeDelta::FromMilliseconds(kFastCloseAnimMs))));
  animator->StartAnimation(
      new ui::LayerAnimationSequence(
          ui::LayerAnimationElement::CreateOpacityElement(
              0.0, base::TimeDelta::FromMilliseconds(kFastCloseAnimMs))));
}

// Fades |window| in to full opacity.
void FadeInWindow(aura::Window* window) {
  ui::LayerAnimator* animator = window->layer()->GetAnimator();
  animator->set_preemption_strategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  animator->StartAnimation(
      new ui::LayerAnimationSequence(
          ui::LayerAnimationElement::CreateOpacityElement(
              1.0, base::TimeDelta::FromMilliseconds(kLockFadeInAnimMs))));
}

// Makes |window| fully transparent instantaneously.
void HideWindow(aura::Window* window) {
  window->layer()->SetOpacity(0.0);
}

// Restores |window| to its original position and scale and full opacity
// instantaneously.
void RestoreWindow(aura::Window* window) {
  window->layer()->SetTransform(ui::Transform());
  window->layer()->SetOpacity(1.0);
}

}  // namespace

void SessionStateAnimator::TestApi::TriggerHideBlackLayerTimeout() {
      animator_->DropBlackLayer();
      animator_->hide_black_layer_timer_.Stop();
}

bool SessionStateAnimator::TestApi::ContainersAreAnimated(
    int container_mask, AnimationType type) const {
  aura::Window::Windows containers;
  animator_->GetContainers(container_mask, &containers);
  for (aura::Window::Windows::const_iterator it = containers.begin();
       it != containers.end(); ++it) {
    aura::Window* window = *it;
    ui::Layer* layer = window->layer();

    switch (type) {
      case ANIMATION_SLOW_CLOSE:
        if (layer->GetTargetTransform() != GetSlowCloseTransform())
          return false;
        break;
      case ANIMATION_UNDO_SLOW_CLOSE:
        if (layer->GetTargetTransform() != ui::Transform())
          return false;
        break;
      case ANIMATION_FAST_CLOSE:
        if (layer->GetTargetTransform() != GetFastCloseTransform() ||
            layer->GetTargetOpacity() > 0.0001)
          return false;
        break;
      case ANIMATION_FADE_IN:
        if (layer->GetTargetOpacity() < 0.9999)
          return false;
        break;
      case ANIMATION_HIDE:
        if (layer->GetTargetOpacity() > 0.0001)
          return false;
        break;
      case ANIMATION_RESTORE:
        if (layer->opacity() < 0.9999 || layer->transform() != ui::Transform())
          return false;
        break;
      default:
        NOTREACHED() << "Unhandled animation type " << type;
        return false;
    }
  }
  return true;
}

bool SessionStateAnimator::TestApi::BlackLayerIsVisible() const {
  return animator_->black_layer_.get() &&
         animator_->black_layer_->visible();
}

gfx::Rect SessionStateAnimator::TestApi::GetBlackLayerBounds() const {
  ui::Layer* layer = animator_->black_layer_.get();
  return layer ? layer->bounds() : gfx::Rect();
}

SessionStateAnimator::SessionStateAnimator() {
  Shell::GetPrimaryRootWindow()->AddRootWindowObserver(this);
}

SessionStateAnimator::~SessionStateAnimator() {
  Shell::GetPrimaryRootWindow()->RemoveRootWindowObserver(this);
}

// Fills |containers| with the containers described by |container_mask|.
void SessionStateAnimator::GetContainers(int container_mask,
                                         aura::Window::Windows* containers) {
  aura::RootWindow* root_window = Shell::GetPrimaryRootWindow();
  containers->clear();

  if (container_mask & DESKTOP_BACKGROUND) {
    containers->push_back(Shell::GetContainer(
        root_window,
        internal::kShellWindowId_DesktopBackgroundContainer));
  }
  if (container_mask & LAUNCHER) {
    containers->push_back(Shell::GetContainer(
        root_window,
        internal::kShellWindowId_LauncherContainer));
  }
  if (container_mask & NON_LOCK_SCREEN_CONTAINERS) {
    // TODO(antrim): Figure out a way to eliminate a need to exclude launcher
    // in such way.
    aura::Window* non_lock_screen_containers = Shell::GetContainer(
        root_window,
        internal::kShellWindowId_NonLockScreenContainersContainer);
    aura::Window::Windows children = non_lock_screen_containers->children();

    for (aura::Window::Windows::const_iterator it = children.begin();
         it != children.end(); ++it) {
      aura::Window* window = *it;
      if (window->id() == internal::kShellWindowId_LauncherContainer)
        continue;
      containers->push_back(window);
    }
  }
  if (container_mask & LOCK_SCREEN_BACKGROUND) {
    containers->push_back(Shell::GetContainer(
        root_window,
        internal::kShellWindowId_LockScreenBackgroundContainer));
  }
  if (container_mask & LOCK_SCREEN_CONTAINERS) {
    containers->push_back(Shell::GetContainer(
        root_window,
        internal::kShellWindowId_LockScreenContainersContainer));
  }
  if (container_mask & LOCK_SCREEN_RELATED_CONTAINERS) {
    containers->push_back(Shell::GetContainer(
        root_window,
        internal::kShellWindowId_LockScreenRelatedContainersContainer));
  }
}

// Apply animation |type| to all containers described by |container_mask|.
void SessionStateAnimator::StartAnimation(int container_mask,
                                          AnimationType type) {
  aura::Window::Windows containers;
  GetContainers(container_mask, &containers);

  for (aura::Window::Windows::const_iterator it = containers.begin();
       it != containers.end(); ++it) {
    aura::Window* window = *it;
    switch (type) {
      case ANIMATION_SLOW_CLOSE:
        StartSlowCloseAnimationForWindow(window);
        break;
      case ANIMATION_UNDO_SLOW_CLOSE:
        StartUndoSlowCloseAnimationForWindow(window);
        break;
      case ANIMATION_FAST_CLOSE:
        StartFastCloseAnimationForWindow(window);
        break;
      case ANIMATION_FADE_IN:
        FadeInWindow(window);
        break;
      case ANIMATION_HIDE:
        HideWindow(window);
        break;
      case ANIMATION_RESTORE:
        RestoreWindow(window);
        break;
      default:
        NOTREACHED() << "Unhandled animation type " << type;
    }
  }
}

void SessionStateAnimator::OnRootWindowResized(const aura::RootWindow* root,
                                               const gfx::Size& new_size) {
  if (black_layer_.get())
    black_layer_->SetBounds(gfx::Rect(root->bounds().size()));
}

void SessionStateAnimator::ShowBlackLayer() {
  if (hide_black_layer_timer_.IsRunning())
    hide_black_layer_timer_.Stop();

  if (!black_layer_.get()) {
    black_layer_.reset(new ui::Layer(ui::LAYER_SOLID_COLOR));
    black_layer_->SetColor(SK_ColorBLACK);

    ui::Layer* root_layer = Shell::GetPrimaryRootWindow()->layer();
    black_layer_->SetBounds(root_layer->bounds());
    root_layer->Add(black_layer_.get());
    root_layer->StackAtBottom(black_layer_.get());
  }
  black_layer_->SetVisible(true);
}

void SessionStateAnimator::DropBlackLayer() {
  black_layer_.reset();
}

void SessionStateAnimator::ScheduleDropBlackLayer() {
  hide_black_layer_timer_.Stop();
  hide_black_layer_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromMilliseconds(kUndoSlowCloseAnimMs),
      this, &SessionStateAnimator::DropBlackLayer);
}

}  // namespace internal
}  // namespace ash
