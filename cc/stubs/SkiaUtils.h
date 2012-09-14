// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_STUBS_SKIAUTILS_H_
#define CC_STUBS_SKIAUTILS_H_

namespace cc {

inline SkScalar CCFloatToSkScalar(float f)
{
    return SkFloatToScalar(isfinite(f) ? f : 0);
}

}

#endif  // CC_STUBS_SKIAUTILS_H_
