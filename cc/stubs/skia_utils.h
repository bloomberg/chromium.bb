// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_STUBS_SKIAUTILS_H_
#define CC_STUBS_SKIAUTILS_H_

#include <limits>

#include "third_party/skia/include/core/SkScalar.h"

namespace cc {

// Skia has problems when passed infinite, etc floats, filter them to 0.
inline SkScalar FloatToSkScalar(float f)
{
    // This checks if |f| is NaN.
    if (f != f)
        return 0;
    if (f == std::numeric_limits<double>::infinity())
        return 0;
    if (f == -std::numeric_limits<double>::infinity())
        return 0;
    return SkFloatToScalar(f);
}

}

#endif  // CC_STUBS_SKIAUTILS_H_
