// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_STUBS_TILINGDATA_H_
#define CC_STUBS_TILINGDATA_H_

#if INSIDE_WEBKIT_BUILD
#include "Source/WebCore/platform/graphics/gpu/TilingData.h"
#else
#include "third_party/WebKit/Source/WebCore/platform/graphics/gpu/TilingData.h"
#endif

namespace cc {
    typedef WebCore::TilingData TilingData;
}

#endif  // CC_STUBS_TILINGDATA_H_
