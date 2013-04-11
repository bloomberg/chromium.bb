// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_ANIMATION_ANIMATION_EVENTS_H_
#define CC_ANIMATION_ANIMATION_EVENTS_H_

#include <vector>

#include "cc/animation/animation.h"
#include "cc/base/cc_export.h"
#include "ui/gfx/transform.h"

namespace cc {

struct CC_EXPORT AnimationEvent {
  enum Type { Started, Finished, PropertyUpdate };

  AnimationEvent(Type type,
                 int layer_id,
                 int group_id,
                 Animation::TargetProperty target_property,
                 double monotonic_time);

  Type type;
  int layer_id;
  int group_id;
  Animation::TargetProperty target_property;
  double monotonic_time;
  bool is_impl_only;
  float opacity;
  gfx::Transform transform;
};

typedef std::vector<AnimationEvent> AnimationEventsVector;

}  // namespace cc

#endif  // CC_ANIMATION_ANIMATION_EVENTS_H_
