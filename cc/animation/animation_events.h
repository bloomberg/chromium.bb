// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_ANIMATION_ANIMATION_EVENTS_H_
#define CC_ANIMATION_ANIMATION_EVENTS_H_

#include <vector>

#include "cc/animation/animation.h"
#include "ui/gfx/transform.h"

namespace cc {

struct AnimationEvent {
  enum Type { Started, Finished, PropertyUpdate };

  AnimationEvent(Type type,
                 int layer_id,
                 int group_id,
                 Animation::TargetProperty target_property,
                 double monotonic_time)
      : type(type),
        layer_id(layer_id),
        group_id(group_id),
        target_property(target_property),
        monotonic_time(monotonic_time),
        opacity(0.f) {}

  Type type;
  int layer_id;
  int group_id;
  Animation::TargetProperty target_property;
  double monotonic_time;
  float opacity;
  gfx::Transform transform;
};

typedef std::vector<AnimationEvent> AnimationEventsVector;

}  // namespace cc

#endif  // CC_ANIMATION_ANIMATION_EVENTS_H_
