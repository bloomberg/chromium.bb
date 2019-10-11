// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_BACK_GESTURE_AFFORDANCE_H_
#define ASH_WM_BACK_GESTURE_AFFORDANCE_H_

#include <memory>

#include "ash/ash_export.h"
#include "base/macros.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/views/widget/widget.h"

namespace ash {

// This class is responsible for creating, painting, and positioning the back
// gesture affordance.
class ASH_EXPORT BackGestureAffordance : public gfx::AnimationDelegate {
 public:
  explicit BackGestureAffordance(const gfx::Point& location);
  ~BackGestureAffordance() override;

  // Sets drag progress. 0 means no progress. 1 means full progress and drag
  // will be completed with full progress. Values more than 1 are also allowed
  // for achieving burst ripple. But the maximium value is limited as
  // |kBurstTransform| / |kMaxTransform|.
  void SetDragProgress(int x_location);

  // Aborts the affordance and animates it back.
  void Abort();

  // Completes the affordance and fading it out.
  void Complete();

  gfx::Rect affordance_widget_bounds_for_testing() {
    return affordance_widget_->GetWindowBoundsInScreen();
  }

 private:
  enum class State { DRAGGING, ABORTING, COMPLETING };

  void CreateAffordanceWidget(const gfx::Point& location);

  void UpdateTransform();
  void SchedulePaint();
  void SetAbortProgress(float progress);
  void SetCompleteProgress(float progress);

  // gfx::AnimationDelegate:
  void AnimationEnded(const gfx::Animation* animation) override;
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationCanceled(const gfx::Animation* animation) override;

  // Widget of the affordance with AffordanceView as the content.
  std::unique_ptr<views::Widget> affordance_widget_;

  // Values that determine current state of the affordance.
  State state_ = State::DRAGGING;
  float drag_progress_ = 0.f;
  float abort_progress_ = 0.f;
  float complete_progress_ = 0.f;

  std::unique_ptr<gfx::LinearAnimation> animation_;

  DISALLOW_COPY_AND_ASSIGN(BackGestureAffordance);
};

}  // namespace ash

#endif  // ASH_WM_BACK_GESTURE_AFFORDANCE_H_
