// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_STUBS_FLOATSIZE_H_
#define CC_STUBS_FLOATSIZE_H_

#include "IntSize.h"
#if INSIDE_WEBKIT_BUILD
#include "Source/WebCore/platform/graphics/FloatSize.h"
#else
#include "third_party/WebKit/Source/WebCore/platform/graphics/FloatSize.h"
#endif

namespace cc {
class FloatSize : public WebCore::FloatSize {
public:
    FloatSize() { }

    FloatSize(float width, float height)
        : WebCore::FloatSize(width, height)
    {
    }

    FloatSize(IntSize size)
        : WebCore::FloatSize(size.width(), size.height())
    {
    }

    FloatSize(WebCore::FloatSize size)
        : WebCore::FloatSize(size.width(), size.height())
    {
    }

    FloatSize(WebCore::IntSize size)
        : WebCore::FloatSize(size.width(), size.height())
    {
    }
};

}

#endif
