// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/rotator/screen_rotation_animator.h"

#include <string>
#include <utility>
#include <vector>

#include "ash/display/window_tree_host_manager.h"
#include "ash/rotator/screen_rotation_animation.h"
#include "ash/rotator/screen_rotation_animator_observer.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/layer_owner.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/display/display.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/transform.h"
#include "ui/gfx/transform_util.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

// The number of degrees that the rotation animations animate through.
const int kRotationDegrees = 20;

// The time it takes for the rotation animations to run.
const int kRotationDurationInMs = 250;

// The rotation factors.
const int kCounterClockWiseRotationFactor = 1;
const int kClockWiseRotationFactor = -1;

// Aborts the active animations of the layer, and recurses upon its child
// layers.
void AbortAnimations(ui::Layer* layer) {
  for (ui::Layer* child_layer : layer->children())
    AbortAnimations(child_layer);
  layer->GetAnimator()->AbortAllAnimations();
}

display::Display::Rotation GetCurrentScreenRotation(int64_t display_id) {
  return Shell::GetInstance()
      ->display_manager()
      ->GetDisplayInfo(display_id)
      .GetActiveRotation();
}

bool IsDisplayIdValid(int64_t display_id) {
  return Shell::GetInstance()->display_manager()->IsDisplayIdValid(display_id);
}

// 180 degree rotations should animate clock-wise.
int GetRotationFactor(display::Display::Rotation initial_rotation,
                      display::Display::Rotation new_rotation) {
  return (initial_rotation + 3) % 4 == new_rotation
             ? kCounterClockWiseRotationFactor
             : kClockWiseRotationFactor;
}

aura::Window* GetRootWindow(int64_t display_id) {
  return Shell::GetInstance()
      ->window_tree_host_manager()
      ->GetRootWindowForDisplayId(display_id);
}

// Returns true if the rotation between |initial_rotation| and |new_rotation| is
// 180 degrees.
bool Is180DegreeFlip(display::Display::Rotation initial_rotation,
                     display::Display::Rotation new_rotation) {
  return (initial_rotation + 2) % 4 == new_rotation;
}

// Returns the initial degrees the old layer animation to begin with.
int GetInitialDegrees(display::Display::Rotation initial_rotation,
                      display::Display::Rotation new_rotation) {
  return (Is180DegreeFlip(initial_rotation, new_rotation) ? 180 : 90);
}

// A LayerAnimationObserver that will destroy the contained LayerTreeOwner
// when notified that a layer animation has ended or was aborted.
class LayerCleanupObserver : public ui::LayerAnimationObserver {
 public:
  // Takes WeakPtr of ScreenRotationAnimator. |this| may outlive the |animator_|
  // instance and the |animator_| isn't detaching itself as an observer when
  // being destroyed. However, ideally, when |animator_| is destroying,
  // deleting |old_layer_tree_owner_| will trigger OnLayerAnimationAborted and
  // delete |this| before |animator_| deleted.
  explicit LayerCleanupObserver(base::WeakPtr<ScreenRotationAnimator> animator);
  ~LayerCleanupObserver() override;

  // ui::LayerAnimationObserver:
  void OnLayerAnimationEnded(ui::LayerAnimationSequence* sequence) override;
  void OnLayerAnimationAborted(ui::LayerAnimationSequence* sequence) override;
  void OnLayerAnimationScheduled(
      ui::LayerAnimationSequence* sequence) override {}

 protected:
  // ui::LayerAnimationObserver:
  bool RequiresNotificationWhenAnimatorDestroyed() const override {
    return true;
  }
  void OnAttachedToSequence(ui::LayerAnimationSequence* sequence) override;
  void OnDetachedFromSequence(ui::LayerAnimationSequence* sequence) override;

 private:
  base::WeakPtr<ScreenRotationAnimator> animator_;

  // The LayerAnimationSequence that |this| has been attached to. Defaults to
  // nullptr.
  ui::LayerAnimationSequence* sequence_;

  DISALLOW_COPY_AND_ASSIGN(LayerCleanupObserver);
};

LayerCleanupObserver::LayerCleanupObserver(
    base::WeakPtr<ScreenRotationAnimator> animator)
    : animator_(animator), sequence_(nullptr) {}

LayerCleanupObserver::~LayerCleanupObserver() {
  // We must eplicitly detach from |sequence_| because we return true from
  // RequiresNotificationWhenAnimatorDestroyed.
  if (sequence_)
    sequence_->RemoveObserver(this);
}

void LayerCleanupObserver::OnLayerAnimationEnded(
    ui::LayerAnimationSequence* sequence) {
  if (animator_)
    animator_->OnLayerAnimationEnded();

  delete this;
}

void LayerCleanupObserver::OnLayerAnimationAborted(
    ui::LayerAnimationSequence* sequence) {
  if (animator_)
    animator_->OnLayerAnimationAborted();

  delete this;
}

void LayerCleanupObserver::OnAttachedToSequence(
    ui::LayerAnimationSequence* sequence) {
  sequence_ = sequence;
}

void LayerCleanupObserver::OnDetachedFromSequence(
    ui::LayerAnimationSequence* sequence) {
  DCHECK_EQ(sequence, sequence_);
  sequence_ = nullptr;
}

}  // namespace

struct ScreenRotationAnimator::ScreenRotationRequest {
  ScreenRotationRequest(display::Display::Rotation to_rotation,
                        display::Display::RotationSource from_source)
      : new_rotation(to_rotation), source(from_source) {}
  display::Display::Rotation new_rotation;
  display::Display::RotationSource source;
};

ScreenRotationAnimator::ScreenRotationAnimator(int64_t display_id)
    : display_id_(display_id),
      is_rotating_(false),
      disable_animation_timers_for_test_(false),
      weak_factory_(this) {}

ScreenRotationAnimator::~ScreenRotationAnimator() {}

void ScreenRotationAnimator::AnimateRotation(
    std::unique_ptr<ScreenRotationRequest> rotation_request) {
  aura::Window* root_window = GetRootWindow(display_id_);

  const gfx::Rect original_screen_bounds = root_window->GetTargetBounds();

  const int rotation_factor = GetRotationFactor(
      GetCurrentScreenRotation(display_id_), rotation_request->new_rotation);

  const int old_layer_initial_rotation_degrees = GetInitialDegrees(
      GetCurrentScreenRotation(display_id_), rotation_request->new_rotation);

  const base::TimeDelta duration =
      base::TimeDelta::FromMilliseconds(kRotationDurationInMs);

  const gfx::Tween::Type tween_type = gfx::Tween::FAST_OUT_LINEAR_IN;

  std::unique_ptr<ui::LayerTreeOwner> old_layer_tree =
      ::wm::RecreateLayers(root_window);
  old_layer_tree->root()->set_name("ScreenRotationAnimator:old_layer_tree");

  // Add the cloned layer tree in to the root, so it will be rendered.
  root_window->layer()->Add(old_layer_tree->root());
  root_window->layer()->StackAtTop(old_layer_tree->root());

  old_layer_tree_owner_ = std::move(old_layer_tree);
  std::unique_ptr<LayerCleanupObserver> old_layer_cleanup_observer(
      new LayerCleanupObserver(weak_factory_.GetWeakPtr()));

  Shell::GetInstance()->display_manager()->SetDisplayRotation(
      display_id_, rotation_request->new_rotation, rotation_request->source);

  const gfx::Rect rotated_screen_bounds = root_window->GetTargetBounds();
  const gfx::Point pivot = gfx::Point(rotated_screen_bounds.width() / 2,
                                      rotated_screen_bounds.height() / 2);

  ui::Layer* old_root_layer = old_layer_tree_owner_->root();
  // We must animate each non-cloned child layer individually because the cloned
  // layer was added as a child to |root_window|'s layer so that it will be
  // rendered.
  // TODO(bruthig): Add a NOT_DRAWN layer in between the root_window's layer and
  // its current children so that we only need to initiate two
  // LayerAnimationSequences. One for the new layers and one for the old layer.
  for (ui::Layer* child_layer : root_window->layer()->children()) {
    // Skip the cloned layer because it has a different animation.
    if (child_layer == old_root_layer)
      continue;

    std::unique_ptr<ScreenRotationAnimation> screen_rotation =
        base::MakeUnique<ScreenRotationAnimation>(
            child_layer, kRotationDegrees * rotation_factor,
            0 /* end_degrees */, child_layer->opacity(),
            1.0f /* target_opacity */, pivot, duration, tween_type);

    ui::LayerAnimator* animator = child_layer->GetAnimator();
    animator->set_preemption_strategy(
        ui::LayerAnimator::REPLACE_QUEUED_ANIMATIONS);
    std::unique_ptr<ui::LayerAnimationSequence> animation_sequence =
        base::MakeUnique<ui::LayerAnimationSequence>(
            std::move(screen_rotation));
    animator->StartAnimation(animation_sequence.release());
  }

  // The old layer will also be transformed into the new orientation. We will
  // translate it so that the old layer's center point aligns with the new
  // orientation's center point and use that center point as the pivot for the
  // rotation animation.
  gfx::Transform translate_transform;
  translate_transform.Translate(
      (rotated_screen_bounds.width() - original_screen_bounds.width()) / 2,
      (rotated_screen_bounds.height() - original_screen_bounds.height()) / 2);
  old_root_layer->SetTransform(translate_transform);

  std::unique_ptr<ScreenRotationAnimation> screen_rotation =
      base::MakeUnique<ScreenRotationAnimation>(
          old_root_layer, old_layer_initial_rotation_degrees * rotation_factor,
          (old_layer_initial_rotation_degrees - kRotationDegrees) *
              rotation_factor,
          old_root_layer->opacity(), 0.0f /* target_opacity */, pivot, duration,
          tween_type);

  ui::LayerAnimator* animator = old_root_layer->GetAnimator();
  animator->set_preemption_strategy(
      ui::LayerAnimator::REPLACE_QUEUED_ANIMATIONS);
  std::unique_ptr<ui::LayerAnimationSequence> animation_sequence =
      base::MakeUnique<ui::LayerAnimationSequence>(std::move(screen_rotation));
  // Add an observer so that the cloned layers can be cleaned up with the
  // animation completes/aborts.
  animation_sequence->AddObserver(old_layer_cleanup_observer.release());
  // In unit test, we can use ash::test::ScreenRotationAnimatorTestApi to
  // control the animation.
  if (disable_animation_timers_for_test_)
    animator->set_disable_timer_for_test(true);
  animator->StartAnimation(animation_sequence.release());

  rotation_request.reset();
}

void ScreenRotationAnimator::Rotate(display::Display::Rotation new_rotation,
                                    display::Display::RotationSource source) {
  if (GetCurrentScreenRotation(display_id_) == new_rotation)
    return;

  std::unique_ptr<ScreenRotationRequest> rotation_request =
      base::MakeUnique<ScreenRotationRequest>(new_rotation, source);

  if (is_rotating_) {
    last_pending_request_ = std::move(rotation_request);
    // The pending request will be processed when the
    // OnLayerAnimation(Ended|Aborted) methods should be called after
    // StopAnimating().
    StopAnimating();
  } else {
    is_rotating_ = true;
    AnimateRotation(std::move(rotation_request));
  }
}

void ScreenRotationAnimator::AddScreenRotationAnimatorObserver(
    ScreenRotationAnimatorObserver* observer) {
  screen_rotation_animator_observers_.AddObserver(observer);
}

void ScreenRotationAnimator::RemoveScreenRotationAnimatorObserver(
    ScreenRotationAnimatorObserver* observer) {
  screen_rotation_animator_observers_.RemoveObserver(observer);
}

void ScreenRotationAnimator::OnLayerAnimationEnded() {
  ProcessAnimationQueue();
}

void ScreenRotationAnimator::OnLayerAnimationAborted() {
  AbortAnimations(old_layer_tree_owner_->root());
  ProcessAnimationQueue();
}

void ScreenRotationAnimator::ProcessAnimationQueue() {
  is_rotating_ = false;
  old_layer_tree_owner_.reset();
  if (last_pending_request_ && IsDisplayIdValid(display_id_)) {
    std::unique_ptr<ScreenRotationRequest> rotation_request =
        std::move(last_pending_request_);
    Rotate(rotation_request->new_rotation, rotation_request->source);
    rotation_request.reset();
    return;
  }

  for (auto& observer : screen_rotation_animator_observers_)
    observer.OnScreenRotationAnimationFinished(this);
}

void ScreenRotationAnimator::set_disable_animation_timers_for_test(
    bool disable_timers) {
  disable_animation_timers_for_test_ = disable_timers;
}

void ScreenRotationAnimator::StopAnimating() {
  aura::Window* root_window = GetRootWindow(display_id_);
  for (ui::Layer* child_layer : root_window->layer()->children()) {
    if (child_layer == old_layer_tree_owner_->root())
      continue;

    child_layer->GetAnimator()->StopAnimating();
  }

  old_layer_tree_owner_->root()->GetAnimator()->StopAnimating();
}

}  // namespace ash
