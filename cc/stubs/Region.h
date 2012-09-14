// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_STUBS_REGION_H_
#define CC_STUBS_REGION_H_

#include "IntRect.h"
#if INSIDE_WEBKIT_BUILD
#include "Source/WebCore/platform/graphics/Region.h"
#else
#include "third_party/WebKit/Source/WebCore/platform/graphics/Region.h"
#endif

namespace cc {

class Region : public WebCore::Region {
public:
    Region() { }

    Region(const IntRect& rect)
        : WebCore::Region(rect)
    {
    }

    Region(const WebCore::IntRect& rect)
        : WebCore::Region(rect)
    {
    }

    Region(const WebCore::Region& region)
        : WebCore::Region(region)
    {
    }
};

}

#endif  // CC_STUBS_REGION_H_
