// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "cc/test/layer_test_common.h"

#include "cc/draw_quad.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace LayerTestCommon {

// Align with expected and actual output
const char* quadString = "    Quad: ";

void verifyQuadsExactlyCoverRect(const cc::QuadList& quads,
                                 const cc::IntRect& rect) {
    cc::Region remaining(rect);

    for (size_t i = 0; i < quads.size(); ++i) {
        cc::DrawQuad* quad = quads[i];
        cc::IntRect quadRect = cc::IntRect(quad->quadRect());

        EXPECT_TRUE(rect.contains(quadRect)) << quadString << i;
        EXPECT_TRUE(remaining.contains(quadRect)) << quadString << i;
        remaining.subtract(cc::Region(quadRect));
    }

    EXPECT_TRUE(remaining.isEmpty());
}

}  // namespace LayerTestCommon
