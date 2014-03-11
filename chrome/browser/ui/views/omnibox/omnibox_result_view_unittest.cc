// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_result_view.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(OmniboxResultViewTest, CheckComputeMatchWidths) {
    int contents_max_width, description_max_width;
    const int separator_width = 10;

    // Both contents and description fit fine.
    OmniboxResultView::ComputeMatchMaxWidths(
        100, separator_width, 50, 200, true, &contents_max_width,
        &description_max_width);
    EXPECT_EQ(-1, contents_max_width);
    EXPECT_EQ(-1, description_max_width);

    // Contents should be given as much space as it wants up to 300 pixels.
    OmniboxResultView::ComputeMatchMaxWidths(
        100, separator_width, 50, 100, true, &contents_max_width,
        &description_max_width);
    EXPECT_EQ(-1, contents_max_width);
    EXPECT_EQ(0, description_max_width);

    // Description should be hidden if it's at least 75 pixels wide but doesn't
    // get 75 pixels of space.
    OmniboxResultView::ComputeMatchMaxWidths(
        300, separator_width, 75, 384, true, &contents_max_width,
        &description_max_width);
    EXPECT_EQ(-1, contents_max_width);
    EXPECT_EQ(0, description_max_width);

    // Both contents and description will be limited.
    OmniboxResultView::ComputeMatchMaxWidths(
        310, separator_width, 150, 400, true, &contents_max_width,
        &description_max_width);
    EXPECT_EQ(300, contents_max_width);
    EXPECT_EQ(400 - 300 - separator_width, description_max_width);

    // Contents takes all available space.
    OmniboxResultView::ComputeMatchMaxWidths(
        400, separator_width, 0, 200, true, &contents_max_width,
        &description_max_width);
    EXPECT_EQ(-1, contents_max_width);
    EXPECT_EQ(0, description_max_width);

    // Half and half.
    OmniboxResultView::ComputeMatchMaxWidths(
        395, separator_width, 395, 700, true, &contents_max_width,
        &description_max_width);
    EXPECT_EQ((700 - separator_width) / 2, contents_max_width);
    EXPECT_EQ((700 - separator_width) / 2, description_max_width);

    // When we disallow shrinking the contents, it should get as much space as
    // it wants.
    OmniboxResultView::ComputeMatchMaxWidths(
        395, separator_width, 395, 700, false, &contents_max_width,
        &description_max_width);
    EXPECT_EQ(-1, contents_max_width);
    EXPECT_EQ(295, description_max_width);

    // (available_width - separator_width) is odd, so contents gets the extra
    // pixel.
    OmniboxResultView::ComputeMatchMaxWidths(
        395, separator_width, 395, 699, true, &contents_max_width,
        &description_max_width);
    EXPECT_EQ((700 - separator_width) / 2, contents_max_width);
    EXPECT_EQ((700 - separator_width) / 2 - 1, description_max_width);

    // Not enough space to draw anything.
    OmniboxResultView::ComputeMatchMaxWidths(
        1, 1, 1, 0, true, &contents_max_width, &description_max_width);
    EXPECT_EQ(0, contents_max_width);
    EXPECT_EQ(0, description_max_width);
}
