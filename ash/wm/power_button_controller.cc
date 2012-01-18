// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/power_button_controller.h"

#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "base/logging.h"
#include "base/time.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/compositor/layer.h"
#include "ui/gfx/compositor/layer_animation_element.h"
#include "ui/gfx/compositor/layer_animation_sequence.h"
#include "ui/gfx/compositor/layer_animator.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"
#include "ui/gfx/transform.h"

namespace ash {

namespace {

// Amount of time that the power button needs to be held before we lock the
// screen.
const int kLockTimeoutMs = 400;

// Amount of time that the power button needs to be held before we shut down.
const int kShutdownTimeoutMs = 400;

// Amount of time to wait for our lock requests to be honored before giving up.
const int kLockFailTimeoutMs = 4000;

// When the button has been held continuously from the unlocked state, amount of
// time that we wait after the screen locker window is shown before starting the
// pre-shutdown animation.
const int kLockToShutdownTimeoutMs = 150;

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
const int kLockFadeInAnimMs = 500;

// Slightly-smaller size that we scale the screen down to for the pre-lock and
// pre-shutdown states.
const float kSlowCloseSizeRatio = 0.95f;

// Containers holding screen locker windows.
const int kScreenLockerContainerIds[] = {
  internal::kShellWindowId_LockScreenContainer,
  internal::kShellWindowId_LockSystemModalContainer,
};

// Containers holding additional windows that should be shown while the screen
// is locked.
const int kRelatedContainerIds[] = {
  internal::kShellWindowId_StatusContainer,
  internal::kShellWindowId_MenuAndTooltipContainer,
  internal::kShellWindowId_SettingBubbleContainer,
};

// Is |window| a container that holds screen locker windows?
bool IsScreenLockerContainer(aura::Window* window) {
  for (size_t i = 0; i < arraysize(kScreenLockerContainerIds); ++i)
    if (window->id() == kScreenLockerContainerIds[i])
      return true;
  return false;
}

// Is |window| a container that holds other windows that should be shown while
// the screen is locked?
bool IsRelatedContainer(aura::Window* window) {
  for (size_t i = 0; i < arraysize(kRelatedContainerIds); ++i)
    if (window->id() == kRelatedContainerIds[i])
      return true;
  return false;
}

// Returns the transform that should be applied to containers for the slow-close
// animation.
ui::Transform GetSlowCloseTransform() {
  gfx::Size root_size = aura::RootWindow::GetInstance()->bounds().size();
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
  gfx::Size root_size = aura::RootWindow::GetInstance()->bounds().size();
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

// Fills |containers| with the containers described by |group|.
void GetContainers(PowerButtonController::ContainerGroup group,
                   aura::Window::Windows* containers) {
  containers->clear();

  aura::Window* root = aura::RootWindow::GetInstance();
  for (aura::Window::Windows::const_iterator it = root->children().begin();
       it != root->children().end(); ++it) {
    aura::Window* window = *it;

    bool matched = true;
    if (group != PowerButtonController::ALL_CONTAINERS) {
      bool is_screen_locker = IsScreenLockerContainer(window);
      bool is_related = IsRelatedContainer(window);

      switch (group) {
        case PowerButtonController::SCREEN_LOCKER_CONTAINERS:
          matched = is_screen_locker;
          break;
        case PowerButtonController::SCREEN_LOCKER_AND_RELATED_CONTAINERS:
          matched = is_screen_locker || is_related;
          break;
        case PowerButtonController::
            ALL_BUT_SCREEN_LOCKER_AND_RELATED_CONTAINERS:
          matched = !is_screen_locker && !is_related;
          break;
        default:
          NOTREACHED() << "Unhandled container group " << group;
      }
    }

    if (matched)
      containers->push_back(window);
  }
}

// Apply animation |type| to all containers described by |group|.
void StartAnimation(PowerButtonController::ContainerGroup group,
                    PowerButtonController::AnimationType type) {
  aura::Window::Windows containers;
  GetContainers(group, &containers);

  for (aura::Window::Windows::const_iterator it = containers.begin();
       it != containers.end(); ++it) {
    aura::Window* window = *it;
    switch (type) {
      case PowerButtonController::ANIMATION_SLOW_CLOSE:
        StartSlowCloseAnimationForWindow(window);
        break;
      case PowerButtonController::ANIMATION_UNDO_SLOW_CLOSE:
        StartUndoSlowCloseAnimationForWindow(window);
        break;
      case PowerButtonController::ANIMATION_FAST_CLOSE:
        StartFastCloseAnimationForWindow(window);
        break;
      case PowerButtonController::ANIMATION_FADE_IN:
        FadeInWindow(window);
        break;
      case PowerButtonController::ANIMATION_HIDE:
        HideWindow(window);
        break;
      case PowerButtonController::ANIMATION_RESTORE:
        RestoreWindow(window);
        break;
      default:
        NOTREACHED() << "Unhandled animation type " << type;
    }
  }
}

}  // namespace

bool PowerButtonController::TestApi::ContainerGroupIsAnimated(
    ContainerGroup group, AnimationType type) const {
  aura::Window::Windows containers;
  GetContainers(group, &containers);
  for (aura::Window::Windows::const_iterator it = containers.begin();
       it != containers.end(); ++it) {
    aura::Window* window = *it;
    ui::Layer* layer = window->layer();

    switch (type) {
      case PowerButtonController::ANIMATION_SLOW_CLOSE:
        if (layer->GetTargetTransform() != GetSlowCloseTransform())
          return false;
        break;
      case PowerButtonController::ANIMATION_UNDO_SLOW_CLOSE:
        if (layer->GetTargetTransform() != ui::Transform())
          return false;
        break;
      case PowerButtonController::ANIMATION_FAST_CLOSE:
        if (layer->GetTargetTransform() != GetFastCloseTransform() ||
            layer->GetTargetOpacity() > 0.0001)
          return false;
        break;
      case PowerButtonController::ANIMATION_FADE_IN:
        if (layer->GetTargetOpacity() < 0.9999)
          return false;
        break;
      case PowerButtonController::ANIMATION_HIDE:
        if (layer->GetTargetOpacity() > 0.0001)
          return false;
        break;
      case PowerButtonController::ANIMATION_RESTORE:
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

bool PowerButtonController::TestApi::BackgroundLayerIsVisible() const {
  return controller_->background_layer_.get() &&
         controller_->background_layer_->visible();
}

// Simple class that fills |background_layer_| with black.
class PowerButtonController::BackgroundLayerDelegate
    : public ui::LayerDelegate {
 public:
  BackgroundLayerDelegate() {}
  virtual ~BackgroundLayerDelegate() {}

  // ui::LayerDelegate implementation:
  virtual void OnPaintLayer(gfx::Canvas* canvas) OVERRIDE {
    canvas->FillRect(SK_ColorBLACK, aura::RootWindow::GetInstance()->bounds());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BackgroundLayerDelegate);
};

PowerButtonController::PowerButtonController()
    : logged_in_as_non_guest_(false),
      locked_(false),
      power_button_down_(false),
      lock_button_down_(false),
      shutting_down_(false) {
}

PowerButtonController::~PowerButtonController() {
}

void PowerButtonController::OnLoginStateChange(bool logged_in, bool is_guest) {
  logged_in_as_non_guest_ = logged_in && !is_guest;
}

void PowerButtonController::OnLockStateChange(bool locked) {
  if (shutting_down_ || locked_ == locked)
    return;

  locked_ = locked;
  if (locked) {
    StartAnimation(SCREEN_LOCKER_CONTAINERS, ANIMATION_FADE_IN);
    lock_timer_.Stop();
    lock_fail_timer_.Stop();

#if !defined(CHROMEOS_LEGACY_POWER_BUTTON)
    if (power_button_down_) {
      lock_to_shutdown_timer_.Stop();
      lock_to_shutdown_timer_.Start(
          FROM_HERE,
          base::TimeDelta::FromMilliseconds(kLockToShutdownTimeoutMs),
          this, &PowerButtonController::OnLockToShutdownTimeout);
    }
#endif
  } else {
    StartAnimation(ALL_BUT_SCREEN_LOCKER_AND_RELATED_CONTAINERS,
                   ANIMATION_RESTORE);
    HideBackgroundLayer();
  }
}

void PowerButtonController::OnStartingLock() {
  if (shutting_down_ || locked_)
    return;

  StartAnimation(ALL_BUT_SCREEN_LOCKER_AND_RELATED_CONTAINERS,
                 ANIMATION_FAST_CLOSE);

  // Hide the screen locker containers so we can make them fade in later.
  StartAnimation(SCREEN_LOCKER_CONTAINERS, ANIMATION_HIDE);
}

void PowerButtonController::OnPowerButtonEvent(
    bool down, const base::TimeTicks& timestamp) {
  power_button_down_ = down;

  if (shutting_down_)
    return;

#if defined(CHROMEOS_LEGACY_POWER_BUTTON)
  // If power button releases won't get reported correctly because we're not
  // running on official hardware, just lock the screen or shut down
  // immediately.
  if (down) {
    ShowBackgroundLayer();
    if (logged_in_as_non_guest_ && !locked_) {
      StartAnimation(ALL_BUT_SCREEN_LOCKER_AND_RELATED_CONTAINERS,
                     ANIMATION_SLOW_CLOSE);
      OnLockTimeout();
    } else {
      OnShutdownTimeout();
    }
  }
#else
  if (down) {
    // If we already have a pending request to lock the screen, wait.
    if (lock_fail_timer_.IsRunning())
      return;

    if (logged_in_as_non_guest_ && !locked_)
      StartLockTimer();
    else
      StartShutdownTimer();
  } else {  // Button is up.
    if (lock_timer_.IsRunning() || shutdown_timer_.IsRunning())
      StartAnimation(
          locked_ ? SCREEN_LOCKER_AND_RELATED_CONTAINERS : ALL_CONTAINERS,
          ANIMATION_UNDO_SLOW_CLOSE);

    // Drop the background layer after the undo animation finishes.
    if (lock_timer_.IsRunning() ||
        (shutdown_timer_.IsRunning() && !logged_in_as_non_guest_)) {
      hide_background_layer_timer_.Stop();
      hide_background_layer_timer_.Start(
          FROM_HERE,
          base::TimeDelta::FromMilliseconds(kUndoSlowCloseAnimMs),
          this, &PowerButtonController::HideBackgroundLayer);
    }

    lock_timer_.Stop();
    shutdown_timer_.Stop();
    lock_to_shutdown_timer_.Stop();
  }
#endif
}

void PowerButtonController::OnLockButtonEvent(
    bool down, const base::TimeTicks& timestamp) {
  lock_button_down_ = down;

  if (shutting_down_ || !logged_in_as_non_guest_)
    return;

  // Bail if we're already locked or are in the process of locking.  Also give
  // the power button precedence over the lock button (we don't expect both
  // buttons to be present, so this is just making sure that we don't do
  // something completely stupid if that assumption changes later).
  if (locked_ || lock_fail_timer_.IsRunning() || power_button_down_)
    return;

  if (down) {
    StartLockTimer();
  } else {
    if (lock_timer_.IsRunning()) {
      StartAnimation(ALL_BUT_SCREEN_LOCKER_AND_RELATED_CONTAINERS,
                     ANIMATION_UNDO_SLOW_CLOSE);
      hide_background_layer_timer_.Stop();
      hide_background_layer_timer_.Start(
          FROM_HERE,
          base::TimeDelta::FromMilliseconds(kUndoSlowCloseAnimMs),
          this, &PowerButtonController::HideBackgroundLayer);
      lock_timer_.Stop();
    }
  }
}

void PowerButtonController::OnRootWindowResized(const gfx::Size& new_size) {
  if (background_layer_.get())
    background_layer_->SetBounds(gfx::Rect(new_size));
}

void PowerButtonController::OnLockTimeout() {
  delegate_->RequestLockScreen();
  lock_fail_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromMilliseconds(kLockFailTimeoutMs),
      this, &PowerButtonController::OnLockFailTimeout);
}

void PowerButtonController::OnLockFailTimeout() {
  DCHECK(!locked_);
  LOG(ERROR) << "Screen lock request timed out";
  StartAnimation(ALL_BUT_SCREEN_LOCKER_AND_RELATED_CONTAINERS,
                 ANIMATION_RESTORE);
  HideBackgroundLayer();
}

void PowerButtonController::OnLockToShutdownTimeout() {
  DCHECK(locked_);
  StartShutdownTimer();
}

void PowerButtonController::OnShutdownTimeout() {
  DCHECK(!shutting_down_);
  shutting_down_ = true;
  aura::RootWindow::GetInstance()->ShowCursor(false);
  StartAnimation(ALL_CONTAINERS, ANIMATION_FAST_CLOSE);
  real_shutdown_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromMilliseconds(kFastCloseAnimMs),
      this, &PowerButtonController::OnRealShutdownTimeout);
}

void PowerButtonController::OnRealShutdownTimeout() {
  DCHECK(shutting_down_);
  delegate_->RequestShutdown();
}

void PowerButtonController::StartLockTimer() {
  ShowBackgroundLayer();
  StartAnimation(ALL_BUT_SCREEN_LOCKER_AND_RELATED_CONTAINERS,
                 ANIMATION_SLOW_CLOSE);
  lock_timer_.Stop();
  lock_timer_.Start(FROM_HERE,
                    base::TimeDelta::FromMilliseconds(kSlowCloseAnimMs),
                    this, &PowerButtonController::OnLockTimeout);
}

void PowerButtonController::StartShutdownTimer() {
  ShowBackgroundLayer();
  StartAnimation(ALL_CONTAINERS, ANIMATION_SLOW_CLOSE);
  shutdown_timer_.Stop();
  shutdown_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromMilliseconds(kShutdownTimeoutMs),
      this, &PowerButtonController::OnShutdownTimeout);
}

void PowerButtonController::ShowBackgroundLayer() {
  if (hide_background_layer_timer_.IsRunning())
    hide_background_layer_timer_.Stop();

  if (!background_layer_.get()) {
    background_layer_delegate_.reset(new BackgroundLayerDelegate);
    background_layer_.reset(new ui::Layer(ui::Layer::LAYER_HAS_TEXTURE));
    background_layer_->set_delegate(background_layer_delegate_.get());

    ui::Layer* root_layer = aura::RootWindow::GetInstance()->layer();
    background_layer_->SetBounds(root_layer->bounds());
    root_layer->Add(background_layer_.get());
    root_layer->StackAtBottom(background_layer_.get());
  }
  background_layer_->SetVisible(true);
}

void PowerButtonController::HideBackgroundLayer() {
  background_layer_.reset();
}

}  // namespace ash
