// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gfx/font.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace {

using gfx::Font;

class FontTest : public testing::Test {
};

TEST_F(FontTest, LoadArial) {
  Font cf(L"Arial", 16);
  ASSERT_TRUE(cf.GetNativeFont());
  ASSERT_EQ(cf.GetStyle(), Font::NORMAL);
  ASSERT_EQ(cf.GetFontSize(), 16);
  ASSERT_EQ(cf.GetFontName(), L"Arial");
}

TEST_F(FontTest, LoadArialBold) {
  Font cf(L"Arial", 16);
  Font bold(cf.DeriveFont(0, Font::BOLD));
  ASSERT_TRUE(bold.GetNativeFont());
  ASSERT_EQ(bold.GetStyle(), Font::BOLD);
}

TEST_F(FontTest, Ascent) {
  Font cf(L"Arial", 16);
  ASSERT_GT(cf.GetBaseline(), 2);
  ASSERT_LE(cf.GetBaseline(), 22);
}

TEST_F(FontTest, Height) {
  Font cf(L"Arial", 16);
  ASSERT_GE(cf.GetHeight(), 16);
  // TODO(akalin): Figure out why height is so large on Linux.
  ASSERT_LE(cf.GetHeight(), 26);
}

TEST_F(FontTest, AvgWidths) {
  Font cf(L"Arial", 16);
  ASSERT_EQ(cf.GetExpectedTextWidth(0), 0);
  ASSERT_GT(cf.GetExpectedTextWidth(1), cf.GetExpectedTextWidth(0));
  ASSERT_GT(cf.GetExpectedTextWidth(2), cf.GetExpectedTextWidth(1));
  ASSERT_GT(cf.GetExpectedTextWidth(3), cf.GetExpectedTextWidth(2));
}

TEST_F(FontTest, Widths) {
  Font cf(L"Arial", 16);
  ASSERT_EQ(cf.GetStringWidth(L""), 0);
  ASSERT_GT(cf.GetStringWidth(L"a"), cf.GetStringWidth(L""));
  ASSERT_GT(cf.GetStringWidth(L"ab"), cf.GetStringWidth(L"a"));
  ASSERT_GT(cf.GetStringWidth(L"abc"), cf.GetStringWidth(L"ab"));
}

#if defined(OS_WIN)
// http://crbug.com/46733
TEST_F(FontTest, FAILS_DeriveFontResizesIfSizeTooSmall) {
  // This creates font of height -8.
  Font cf(L"Arial", 6);
  Font derived_font = cf.DeriveFont(-4);
  LOGFONT font_info;
  GetObject(derived_font.GetNativeFont(), sizeof(LOGFONT), &font_info);
  EXPECT_EQ(-5, font_info.lfHeight);
}

TEST_F(FontTest, DeriveFontKeepsOriginalSizeIfHeightOk) {
  // This creates font of height -8.
  Font cf(L"Arial", 6);
  Font derived_font = cf.DeriveFont(-2);
  LOGFONT font_info;
  GetObject(derived_font.GetNativeFont(), sizeof(LOGFONT), &font_info);
  EXPECT_EQ(-6, font_info.lfHeight);
}
#endif
}  // namespace
