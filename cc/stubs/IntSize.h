// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_STUBS_INTSIZE_H_
#define CC_STUBS_INTSIZE_H_

#if INSIDE_WEBKIT_BUILD
#include "Source/WebCore/platform/graphics/IntSize.h"
#else
#include "third_party/WebKit/Source/WebCore/platform/graphics/IntSize.h"
#endif

namespace cc {

class IntSize : public WebCore::IntSize {
public:
    IntSize() { }

    IntSize(int width, int height)
        : WebCore::IntSize(width, height)
    {
    }

    IntSize(WebCore::IntSize size)
        : WebCore::IntSize(size.width(), size.height())
    {

    }
};

}

#endif
