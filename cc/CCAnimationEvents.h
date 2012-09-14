// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCAnimationEvents_h
#define CCAnimationEvents_h

#include "CCActiveAnimation.h"

#include <wtf/PassOwnPtr.h>
#include <wtf/Vector.h>

namespace cc {

struct CCAnimationEvent {
    enum Type { Started, Finished };

    CCAnimationEvent(Type type, int layerId, int groupId, CCActiveAnimation::TargetProperty targetProperty, double monotonicTime)
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
    CCActiveAnimation::TargetProperty targetProperty;
    double monotonicTime;
};

typedef Vector<CCAnimationEvent> CCAnimationEventsVector;

} // namespace cc

#endif // CCAnimationEvents_h
