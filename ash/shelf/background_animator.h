// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_BACKGROUND_ANIMATOR_H_
#define ASH_SHELF_BACKGROUND_ANIMATOR_H_

#include "ash/ash_export.h"
#include "base/basictypes.h"
#include "ui/base/animation/animation_delegate.h"
#include "ui/base/animation/slide_animation.h"

namespace ash {
namespace internal {

// Delegate is notified any time the background changes.
class ASH_EXPORT BackgroundAnimatorDelegate {
 public:
  virtual void UpdateBackground(int alpha) = 0;

 protected:
  virtual ~BackgroundAnimatorDelegate() {}
};

// BackgroundAnimator is used by the shelf to animate the background (alpha).
class ASH_EXPORT BackgroundAnimator : public ui::AnimationDelegate {
 public:
  // How the background can be changed.
  enum ChangeType {
    CHANGE_ANIMATE,
    CHANGE_IMMEDIATE
  };

  BackgroundAnimator(BackgroundAnimatorDelegate* delegate,
                     int min_alpha,
                     int max_alpha);
  virtual ~BackgroundAnimator();

  // Sets the transition time in ms.
  void SetDuration(int time_in_ms);

  // Sets whether a background is rendered. Initial value is false. If |type|
  // is |CHANGE_IMMEDIATE| and an animation is not in progress this notifies
  // the delegate immediately (synchronously from this method).
  void SetPaintsBackground(bool value, ChangeType type);
  bool paints_background() const { return paints_background_; }

  // Current alpha.
  int alpha() const { return alpha_; }

  // ui::AnimationDelegate overrides:
  virtual void AnimationProgressed(const ui::Animation* animation) OVERRIDE;

 private:
  BackgroundAnimatorDelegate* delegate_;

  const int min_alpha_;
  const int max_alpha_;

  ui::SlideAnimation animation_;

  // Whether the background is painted.
  bool paints_background_;

  // Current alpha value of the background.
  int alpha_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundAnimator);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_SHELF_BACKGROUND_ANIMATOR_H_
