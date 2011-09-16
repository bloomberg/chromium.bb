// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOUCH_ANIMATION_SCREEN_ROTATION_H_
#define CHROME_BROWSER_UI_TOUCH_ANIMATION_SCREEN_ROTATION_H_
#pragma once

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/bind.h"
#include "ui/base/animation/animation_delegate.h"
#include "ui/gfx/compositor/compositor_observer.h"
#include "ui/gfx/point.h"
#include "ui/gfx/size.h"

namespace ui {
class InterpolatedTransform;
class Layer;
class SlideAnimation;
class Transform;
}

namespace gfx {
class Rect;
}

namespace views {
class PaintLock;
class View;
class Widget;
}

// Classes that wish to be notified when a screen rotation completes must
// implement the screen rotation listener interface.
class ScreenRotationListener {
 public:
  virtual void OnScreenRotationCompleted(const ui::Transform& target_transform,
                                         const gfx::Rect& target_bounds) = 0;
 protected:
  virtual ~ScreenRotationListener();
};

// A screen rotation represents a single transition from one screen orientation
// to another. The  intended usage is that a new instance of the class is
// created for every transition. It is possible to update the target orientation
// in the middle of a transition.
class ScreenRotation : public ui::AnimationDelegate,
                       public ui::CompositorObserver {
 public:
  // The screen rotation does not own the view or the listener, and these
  // objects are required to outlive the Screen rotation object.
  ScreenRotation(views::View* view,
                 ScreenRotationListener* listener,
                 float old_degrees,
                 float new_degrees);
  virtual ~ScreenRotation();

  // Aborts the animation by skipping to the end and applying the final
  // transform before calling |Finalize|.
  void Stop();

 private:
  // Implementation of ui::AnimationDelegate
  virtual void AnimationEnded(const ui::Animation* anim) OVERRIDE;
  virtual void AnimationProgressed(const ui::Animation* anim) OVERRIDE;

  // Implementation of ui::CompositorObserver
  void OnCompositingEnded() OVERRIDE;

  // Initializes |interpolated_transform_|, |new_origin_|, |new_size_|, and
  // (if it has not already been initialized) |old_transform_|
  void Init();

  // Initializes the screen rotation and starts |animation_|.
  void Start();

  // Called after the animation is finished and the view has completed painting
  // in its final state.
  void Finalize();

  // Converts degrees to an angle in the range [-180, 180).
  int NormalizeAngle(int degrees);

  // We occasionally need to wait for a paint to finish before progressing.
  // This function (which is triggered by OnPainted and OnComposited) does
  // any pending work.
  void DoPendingWork();

  // This is the view that will be animated. The animation will operate mainly
  // on |view_|'s layer, but it is used upon completion of the rotation to
  // update the bounds.
  views::View* view_;

  // This widget will be used for scheduling paints.
  views::Widget* widget_;

  // A ScreenRotation may be associated with a listener that is notified when
  // the screen rotation completes.
  ScreenRotationListener* listener_;

  // The animation object that instigates stepping of the animation.
  scoped_ptr<ui::SlideAnimation> animation_;

  // Generates the intermediate transformation matrices used during the
  // animation.
  scoped_ptr<ui::InterpolatedTransform> interpolated_transform_;

  // Ensures that the view is not repainted during the screen rotation.
  scoped_ptr<views::PaintLock> paint_lock_;

  // The original orientation (not updated by |SetTarget|.
  float old_degrees_;

  // The target orientation.
  float new_degrees_;

  // The target size for |view_|
  gfx::Size new_size_;

  // The target origin for |view_|
  gfx::Point new_origin_;

  // This is the duration of the transition in milliseconds
  int duration_;

  // These are used by DoPendingWork to decide what needs to be done.
  bool animation_started_;
  bool animation_stopped_;

  DISALLOW_COPY_AND_ASSIGN(ScreenRotation);
};

#endif  // CHROME_BROWSER_UI_TOUCH_ANIMATION_SCREEN_ROTATION_H_
