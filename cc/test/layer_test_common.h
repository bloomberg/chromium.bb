// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_LAYER_TEST_COMMON_H_
#define CC_TEST_LAYER_TEST_COMMON_H_

namespace cc {
class QuadList;
}

namespace gfx {
class Rect;
}

namespace cc {
namespace LayerTestCommon {

extern const char* quadString;

void verifyQuadsExactlyCoverRect(const cc::QuadList&, const gfx::Rect&);

} // namespace LayerTestCommon
} // namespace cc

#endif  // CC_TEST_LAYER_TEST_COMMON_H_
