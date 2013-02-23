// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SCREEN_ROTATION_H_
#define ASH_SCREEN_ROTATION_H_

#include "ash/ash_export.h"
#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/gfx/point.h"

namespace ui {
class InterpolatedTransform;
class Layer;
}

namespace aura {
class RootWindow;
}

namespace ash {

// A screen rotation represents a single transition from one screen orientation
// to another. The  intended usage is that a new instance of the class is
// created for every transition. It is possible to update the target orientation
// in the middle of a transition.
class ASH_EXPORT ScreenRotation : public ui::LayerAnimationElement {
 public:
  // |degrees| are clockwise. |layer| is the target of the animation. Does not
  // take ownership of |layer|.
  ScreenRotation(int degrees, ui::Layer* layer);
  virtual ~ScreenRotation();

 private:
  // Generates the intermediate transformation matrices used during the
  // animation.
  void InitTransform(ui::Layer* layer);

  // Implementation of ui::LayerAnimationDelegate
  virtual void OnStart(ui::LayerAnimationDelegate* delegate) OVERRIDE;
  virtual bool OnProgress(double t,
                          ui::LayerAnimationDelegate* delegate) OVERRIDE;
  virtual void OnGetTarget(TargetValue* target) const OVERRIDE;
  virtual void OnAbort(ui::LayerAnimationDelegate* delegate) OVERRIDE;

  static const ui::LayerAnimationElement::AnimatableProperties&
      GetProperties();

  scoped_ptr<ui::InterpolatedTransform> interpolated_transform_;

  // The number of degrees to rotate.
  int degrees_;

  // The target origin.
  gfx::Point new_origin_;

  DISALLOW_COPY_AND_ASSIGN(ScreenRotation);
};

}  // namespace ash

#endif  // ASH_SCREEN_ROTATION_H_
