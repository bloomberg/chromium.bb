// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/omnibox_formatting.h"

#include "chrome/browser/vr/model/color_scheme.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace vr {

namespace {

constexpr SkColor kDefaultColor = 0xFF000001;
constexpr SkColor kDimColor = 0xFF000002;
constexpr SkColor kUrlColor = 0xFF000003;

}  // namespace

TEST(OmniboxFormatting, MultiLine) {
  ACMatchClassifications classifications = {
      ACMatchClassification(0, ACMatchClassification::NONE),
      ACMatchClassification(1, ACMatchClassification::URL),
      ACMatchClassification(2, ACMatchClassification::MATCH),
      ACMatchClassification(3, ACMatchClassification::DIM),
      ACMatchClassification(4, ACMatchClassification::INVISIBLE),
  };
  size_t string_length = classifications.size();

  ColorScheme color_scheme;
  memset(&color_scheme, 0, sizeof(color_scheme));
  color_scheme.suggestion_text = kDefaultColor;
  color_scheme.suggestion_dim_text = kDimColor;
  color_scheme.suggestion_url_text = kUrlColor;

  TextFormatting formatting =
      ConvertClassification(classifications, string_length, color_scheme);

  ASSERT_EQ(formatting.size(), 6u);

  EXPECT_EQ(formatting[0].type(), TextFormattingAttribute::COLOR);
  EXPECT_EQ(formatting[0].color(), kDefaultColor);
  EXPECT_EQ(formatting[0].range(), gfx::Range(0, string_length));

  EXPECT_EQ(formatting[1].type(), TextFormattingAttribute::DIRECTIONALITY);
  EXPECT_EQ(formatting[1].directionality(), gfx::DIRECTIONALITY_AS_URL);
  EXPECT_EQ(formatting[1].range(), gfx::Range(0, 0));

  EXPECT_EQ(formatting[2].type(), TextFormattingAttribute::COLOR);
  EXPECT_EQ(formatting[2].color(), kUrlColor);
  EXPECT_EQ(formatting[2].range(), gfx::Range(1, 2));

  EXPECT_EQ(formatting[3].type(), TextFormattingAttribute::WEIGHT);
  EXPECT_EQ(formatting[3].weight(), gfx::Font::Weight::BOLD);
  EXPECT_EQ(formatting[3].range(), gfx::Range(2, 3));

  EXPECT_EQ(formatting[4].type(), TextFormattingAttribute::COLOR);
  EXPECT_EQ(formatting[4].color(), kDimColor);
  EXPECT_EQ(formatting[4].range(), gfx::Range(3, 4));

  EXPECT_EQ(formatting[5].type(), TextFormattingAttribute::COLOR);
  EXPECT_EQ(formatting[5].color(), SK_ColorTRANSPARENT);
  EXPECT_EQ(formatting[5].range(), gfx::Range(4, 5));
}

}  // namespace vr
