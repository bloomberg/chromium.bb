// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "CCAnimationCurve.h"

#include "base/logging.h"

namespace cc {

const CCFloatAnimationCurve* CCAnimationCurve::toFloatAnimationCurve() const
{
    DCHECK(type() == CCAnimationCurve::Float);
    return static_cast<const CCFloatAnimationCurve*>(this);
}

CCAnimationCurve::Type CCFloatAnimationCurve::type() const
{
    return Float;
}

const CCTransformAnimationCurve* CCAnimationCurve::toTransformAnimationCurve() const
{
    DCHECK(type() == CCAnimationCurve::Transform);
    return static_cast<const CCTransformAnimationCurve*>(this);
}

CCAnimationCurve::Type CCTransformAnimationCurve::type() const
{
    return Transform;
}

} // namespace cc
