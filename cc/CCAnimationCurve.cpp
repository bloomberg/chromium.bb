// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "CCAnimationCurve.h"

namespace cc {

const CCFloatAnimationCurve* CCAnimationCurve::toFloatAnimationCurve() const
{
    ASSERT(type() ==  CCAnimationCurve::Float);
    return static_cast<const CCFloatAnimationCurve*>(this);
}

const CCTransformAnimationCurve* CCAnimationCurve::toTransformAnimationCurve() const
{
    ASSERT(type() ==  CCAnimationCurve::Transform);
    return static_cast<const CCTransformAnimationCurve*>(this);
}

} // namespace cc
