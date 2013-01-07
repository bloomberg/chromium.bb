// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_ANIMATION_EVENTS_H_
#define CC_ANIMATION_EVENTS_H_

#include <vector>

#include "cc/animation.h"

namespace cc {

struct AnimationEvent {
    enum Type { Started, Finished };

    AnimationEvent(Type type, int layerId, int groupId, Animation::TargetProperty targetProperty, double monotonicTime)
        : type(type)
        , layerId(layerId)
        , groupId(groupId)
        , targetProperty(targetProperty)
        , monotonicTime(monotonicTime)
    {
    }

    Type type;
    int layerId;
    int groupId;
    Animation::TargetProperty targetProperty;
    double monotonicTime;
};

typedef std::vector<AnimationEvent> AnimationEventsVector;

}  // namespace cc

#endif  // CC_ANIMATION_EVENTS_H_
