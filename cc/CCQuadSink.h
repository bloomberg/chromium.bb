// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCQuadSink_h
#define CCQuadSink_h

#include <wtf/PassOwnPtr.h>

namespace cc {

class CCDrawQuad;

struct CCAppendQuadsData;
struct CCSharedQuadState;

class CCQuadSink {
public:
    virtual ~CCQuadSink() { }

    // Call this to add a SharedQuadState before appending quads that refer to it. Returns a pointer
    // to the given SharedQuadState for convenience, that can be set on the quads to append.
    virtual CCSharedQuadState* useSharedQuadState(PassOwnPtr<CCSharedQuadState>) = 0;

    // Returns true if the quad is added to the list, and false if the quad is entirely culled.
    virtual bool append(PassOwnPtr<CCDrawQuad> passDrawQuad, CCAppendQuadsData&) = 0;
};

}
#endif // CCQuadCuller_h
