// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/power_button_controller.h"

#include "ash/ash_switches.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/time.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/root_window.h"
#include "ui/aura/shared/root_window_event_filter.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/layer_animator.h"
#include "ui/gfx/canvas.h"
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

// Additional time (beyond kFastCloseAnimMs) to wait after starting the
// fast-close shutdown animation before actually requesting shutdown, to give
// the animation time to finish.
const int kShutdownRequestDelayMs = 50;

// Slightly-smaller size that we scale the screen down to for the pre-lock and
// pre-shutdown states.
const float kSlowCloseSizeRatio = 0.95f;

// Returns the transform that should be applied to containers for the slow-close
// animation.
ui::Transform GetSlowCloseTransform() {
  gfx::Size root_size = Shell::GetRootWindow()->bounds().size();
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
  gfx::Size root_size = Shell::GetRootWindow()->bounds().size();
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
  aura::Window* non_lock_screen_containers =
      Shell::GetInstance()->GetContainer(
          internal::kShellWindowId_NonLockScreenContainersContainer);
  aura::Window* lock_screen_containers =
      Shell::GetInstance()->GetContainer(
          internal::kShellWindowId_LockScreenContainersContainer);
  aura::Window* lock_screen_related_containers =
      Shell::GetInstance()->GetContainer(
          internal::kShellWindowId_LockScreenRelatedContainersContainer);

  containers->clear();
  switch (group) {
    case PowerButtonController::ALL_CONTAINERS:
      containers->push_back(non_lock_screen_containers);
      containers->push_back(lock_screen_containers);
      containers->push_back(lock_screen_related_containers);
      break;
    case PowerButtonController::SCREEN_LOCKER_CONTAINERS:
      containers->push_back(lock_screen_containers);
      break;
    case PowerButtonController::SCREEN_LOCKER_AND_RELATED_CONTAINERS:
      containers->push_back(lock_screen_containers);
      containers->push_back(lock_screen_related_containers);
      break;
    case PowerButtonController::ALL_BUT_SCREEN_LOCKER_AND_RELATED_CONTAINERS:
      containers->push_back(non_lock_screen_containers);
      break;
    default:
      NOTREACHED() << "Unhandled container group " << group;
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

gfx::Rect PowerButtonController::TestApi::GetBackgroundLayerBounds() const {
  ui::Layer* layer = controller_->background_layer_.get();
  return layer ? layer->bounds() : gfx::Rect();
}

PowerButtonController::PowerButtonController()
    : login_status_(user::LOGGED_IN_NONE),
      unlocked_login_status_(user::LOGGED_IN_NONE),
      power_button_down_(false),
      lock_button_down_(false),
      screen_is_off_(false),
      shutting_down_(false),
      has_legacy_power_button_(
          CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kAuraLegacyPowerButton)) {
  Shell::GetInstance()->GetRootWindow()->AddRootWindowObserver(this);
}

PowerButtonController::~PowerButtonController() {
  Shell::GetInstance()->GetRootWindow()->RemoveRootWindowObserver(this);
}

void PowerButtonController::OnLoginStateChanged(user::LoginStatus status) {
  login_status_ = status;
  unlocked_login_status_ = user::LOGGED_IN_NONE;
}

void PowerButtonController::OnAppTerminating() {
  // If we hear that Chrome is exiting but didn't request it ourselves, all we
  // can really hope for is that we'll have time to clear the screen.
  if (!shutting_down_) {
    shutting_down_ = true;
    ash::Shell::GetInstance()->root_filter()->
        set_update_cursor_visibility(false);
    Shell::GetRootWindow()->ShowCursor(false);
    ShowBackgroundLayer();
    StartAnimation(ALL_CONTAINERS, ANIMATION_HIDE);
  }
}

void PowerButtonController::OnLockStateChanged(bool locked) {
  if (shutting_down_ || (login_status_ == user::LOGGED_IN_LOCKED) == locked)
    return;

  if (!locked && login_status_ == user::LOGGED_IN_LOCKED) {
    login_status_ = unlocked_login_status_;
    unlocked_login_status_ = user::LOGGED_IN_NONE;
  } else {
    unlocked_login_status_ = login_status_;
    login_status_ = user::LOGGED_IN_LOCKED;
  }

  if (locked) {
    StartAnimation(SCREEN_LOCKER_CONTAINERS, ANIMATION_FADE_IN);
    lock_timer_.Stop();
    lock_fail_timer_.Stop();

    if (!has_legacy_power_button_ && power_button_down_) {
      lock_to_shutdown_timer_.Stop();
      lock_to_shutdown_timer_.Start(
          FROM_HERE,
          base::TimeDelta::FromMilliseconds(kLockToShutdownTimeoutMs),
          this, &PowerButtonController::OnLockToShutdownTimeout);
    }
  } else {
    StartAnimation(ALL_BUT_SCREEN_LOCKER_AND_RELATED_CONTAINERS,
                   ANIMATION_RESTORE);
    HideBackgroundLayer();
  }
}

void PowerButtonController::OnScreenBrightnessChanged(double percent) {
  screen_is_off_ = percent <= 0.001;
}

void PowerButtonController::OnStartingLock() {
  if (shutting_down_ || login_status_ == user::LOGGED_IN_LOCKED)
    return;

  // Ensure that the background layer is visible -- if the screen was locked via
  // the wrench menu, we won't have already shown the background as part of the
  // slow-close animation.
  ShowBackgroundLayer();

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

  // Avoid starting the lock/shutdown sequence if the power button is pressed
  // while the screen is off (http://crbug.com/128451).
  if (screen_is_off_)
    return;

  if (has_legacy_power_button_) {
    // If power button releases won't get reported correctly because we're not
    // running on official hardware, just lock the screen or shut down
    // immediately.
    if (down) {
      ShowBackgroundLayer();
      if (LoggedInAsNonGuest() && login_status_ != user::LOGGED_IN_LOCKED) {
        StartAnimation(ALL_BUT_SCREEN_LOCKER_AND_RELATED_CONTAINERS,
                       ANIMATION_SLOW_CLOSE);
        OnLockTimeout();
      } else {
        OnShutdownTimeout();
      }
    }
  } else {  // !has_legacy_power_button_
    if (down) {
      // If we already have a pending request to lock the screen, wait.
      if (lock_fail_timer_.IsRunning())
        return;

      if (LoggedInAsNonGuest() && login_status_ != user::LOGGED_IN_LOCKED)
        StartLockTimer();
      else
        StartShutdownTimer();
    } else {  // Button is up.
      if (lock_timer_.IsRunning() || shutdown_timer_.IsRunning())
        StartAnimation(
            (login_status_ == user::LOGGED_IN_LOCKED) ?
            SCREEN_LOCKER_AND_RELATED_CONTAINERS : ALL_CONTAINERS,
            ANIMATION_UNDO_SLOW_CLOSE);

      // Drop the background layer after the undo animation finishes.
      if (lock_timer_.IsRunning() ||
          (shutdown_timer_.IsRunning() && !LoggedInAsNonGuest())) {
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
  }
}

void PowerButtonController::OnLockButtonEvent(
    bool down, const base::TimeTicks& timestamp) {
  lock_button_down_ = down;

  if (shutting_down_ || !LoggedInAsNonGuest())
    return;

  // Bail if we're already locked or are in the process of locking.  Also give
  // the power button precedence over the lock button (we don't expect both
  // buttons to be present, so this is just making sure that we don't do
  // something completely stupid if that assumption changes later).
  if (login_status_ == user::LOGGED_IN_LOCKED ||
      lock_fail_timer_.IsRunning() || power_button_down_)
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

void PowerButtonController::RequestShutdown() {
  if (!shutting_down_)
    StartShutdownAnimationAndRequestShutdown();
}

void PowerButtonController::OnRootWindowResized(const aura::RootWindow* root,
                                                const gfx::Size& new_size) {
  if (background_layer_.get())
    background_layer_->SetBounds(gfx::Rect(root->bounds().size()));
}

bool PowerButtonController::LoggedInAsNonGuest() const {
  if (login_status_ == user::LOGGED_IN_NONE)
    return false;
  if (login_status_ == user::LOGGED_IN_GUEST)
    return false;
  // TODO(mukai): think about kiosk mode.
  return true;
}

void PowerButtonController::OnLockTimeout() {
  delegate_->RequestLockScreen();
  lock_fail_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromMilliseconds(kLockFailTimeoutMs),
      this, &PowerButtonController::OnLockFailTimeout);
}

void PowerButtonController::OnLockFailTimeout() {
  DCHECK_NE(login_status_, user::LOGGED_IN_LOCKED);
  LOG(ERROR) << "Screen lock request timed out";
  StartAnimation(ALL_BUT_SCREEN_LOCKER_AND_RELATED_CONTAINERS,
                 ANIMATION_RESTORE);
  HideBackgroundLayer();
}

void PowerButtonController::OnLockToShutdownTimeout() {
  DCHECK_EQ(login_status_, user::LOGGED_IN_LOCKED);
  StartShutdownTimer();
}

void PowerButtonController::OnShutdownTimeout() {
  if (!shutting_down_)
    StartShutdownAnimationAndRequestShutdown();
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

void PowerButtonController::StartShutdownAnimationAndRequestShutdown() {
  DCHECK(!shutting_down_);
  shutting_down_ = true;

  ash::Shell::GetInstance()->root_filter()->set_update_cursor_visibility(false);
  Shell::GetRootWindow()->ShowCursor(false);

  ShowBackgroundLayer();
  if (login_status_ != user::LOGGED_IN_NONE) {
    // Hide the other containers before starting the animation.
    // ANIMATION_FAST_CLOSE will make the screen locker windows partially
    // transparent, and we don't want the other windows to show through.
    StartAnimation(ALL_BUT_SCREEN_LOCKER_AND_RELATED_CONTAINERS,
                   ANIMATION_HIDE);
    StartAnimation(SCREEN_LOCKER_AND_RELATED_CONTAINERS, ANIMATION_FAST_CLOSE);
  } else {
    StartAnimation(ALL_CONTAINERS, ANIMATION_FAST_CLOSE);
  }

  real_shutdown_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromMilliseconds(
          kFastCloseAnimMs + kShutdownRequestDelayMs),
      this, &PowerButtonController::OnRealShutdownTimeout);
}

void PowerButtonController::ShowBackgroundLayer() {
  if (hide_background_layer_timer_.IsRunning())
    hide_background_layer_timer_.Stop();

  if (!background_layer_.get()) {
    background_layer_.reset(new ui::Layer(ui::LAYER_SOLID_COLOR));
    background_layer_->SetColor(SK_ColorBLACK);

    ui::Layer* root_layer = Shell::GetRootWindow()->layer();
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
