// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SHARED_QUAD_STATE_H_
#define CC_SHARED_QUAD_STATE_H_

#include "base/memory/scoped_ptr.h"
#include "ui/gfx/rect.h"
#include <public/WebTransformationMatrix.h>
#include "cc/cc_export.h"

namespace cc {

struct CC_EXPORT SharedQuadState {
    int id;

    // Transforms from quad's original content space to its target content space.
    WebKit::WebTransformationMatrix quadTransform;
    // This rect lives in the content space for the quad's originating layer.
    gfx::Rect visibleContentRect;
    gfx::Rect clippedRectInTarget;
    float opacity;

    static scoped_ptr<SharedQuadState> create(const WebKit::WebTransformationMatrix& quadTransform, const gfx::Rect& visibleContentRect, const gfx::Rect& clippedRectInTarget, float opacity);
    SharedQuadState(const WebKit::WebTransformationMatrix& quadTransform, const gfx::Rect& visibleContentRect, const gfx::Rect& clippedRectInTarget, float opacity);

    scoped_ptr<SharedQuadState> copy() const;
};

}

#endif  // CC_SHARED_QUAD_STATE_H_
