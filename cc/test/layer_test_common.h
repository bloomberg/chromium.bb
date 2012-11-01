// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCLayerTestCommon_h
#define CCLayerTestCommon_h

namespace cc {
class QuadList;
}

namespace gfx {
class Rect;
}

namespace LayerTestCommon {

extern const char* quadString;

void verifyQuadsExactlyCoverRect(const cc::QuadList&, const gfx::Rect&);

} // namespace LayerTestCommon
#endif // CCLayerTestCommon_h
