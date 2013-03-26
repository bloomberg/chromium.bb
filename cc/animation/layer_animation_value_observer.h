// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_ANIMATION_LAYER_ANIMATION_VALUE_OBSERVER_H_
#define CC_ANIMATION_LAYER_ANIMATION_VALUE_OBSERVER_H_

#include "cc/base/cc_export.h"

namespace cc {

class CC_EXPORT LayerAnimationValueObserver {
 public:
  virtual ~LayerAnimationValueObserver() {}

  virtual void OnOpacityAnimated(float opacity) = 0;
  virtual void OnTransformAnimated(const gfx::Transform& transform) = 0;
  virtual bool IsActive() const = 0;
};

}  // namespace cc

#endif  // CC_ANIMATION_LAYER_ANIMATION_VALUE_OBSERVER_H_

