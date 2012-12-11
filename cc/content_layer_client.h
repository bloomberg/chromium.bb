// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_CONTENT_LAYER_CLIENT_H_
#define CC_CONTENT_LAYER_CLIENT_H_

#include "cc/cc_export.h"

class SkCanvas;

namespace gfx {
class Rect;
class RectF;
}

namespace cc {

class CC_EXPORT ContentLayerClient {
public:
    virtual void paintContents(SkCanvas*, const gfx::Rect& clip, gfx::RectF& opaque) = 0;

protected:
    virtual ~ContentLayerClient() { }
};

}

#endif  // CC_CONTENT_LAYER_CLIENT_H_
