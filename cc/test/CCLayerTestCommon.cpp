// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "CCLayerTestCommon.h"
#include "CCDrawQuad.h"

#include <gtest/gtest.h>
#include <wtf/Vector.h>

using namespace cc;

namespace CCLayerTestCommon {

// Align with expected and actual output
const char* quadString = "    Quad: ";

void verifyQuadsExactlyCoverRect(const CCQuadList& quads, const IntRect& rect)
{
    Region remaining(rect);

    for (size_t i = 0; i < quads.size(); ++i) {
        CCDrawQuad* quad = quads[i].get();
        IntRect quadRect = quad->quadRect();

        EXPECT_TRUE(rect.contains(quadRect)) << quadString << i;
        EXPECT_TRUE(remaining.contains(quadRect)) << quadString << i;
        remaining.subtract(Region(quadRect));
    }

    EXPECT_TRUE(remaining.isEmpty());
}

} // namespace
