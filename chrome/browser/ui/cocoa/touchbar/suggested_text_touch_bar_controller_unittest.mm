// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/strings/sys_string_conversions.h"
#import "chrome/browser/ui/cocoa/test/cocoa_test_helper.h"
#import "chrome/browser/ui/cocoa/touchbar/suggested_text_touch_bar_controller.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/range/range.h"

const std::string kEmptyText = "";
const std::string kWord = "hello";
const std::string kWordWithTrailingWhitespace = "hello ";
const std::string kWordWithLeadingWhitespace = " hello";
const std::string kMultipleWords = "hello world";

class SuggestedTextTouchBarControllerUnitTest : public CocoaTest {
 public:
  void SetUp() override {
    CocoaTest::SetUp();
    controller_ = [[SuggestedTextTouchBarController alloc] init];
  }

  gfx::Range GetEditingWordRange(std::string text, size_t cursor) {
    NSRange range =
        [controller_ editingWordRangeFromText:base::SysUTF8ToNSString(text)
                               cursorPosition:cursor];
    return gfx::Range(range);
  }

 private:
  SuggestedTextTouchBarController* controller_;  // weak.
};

// Tests that the proper range representing the location of the editing word is
// calculated for a given text and cursor position.
TEST_F(SuggestedTextTouchBarControllerUnitTest, EditingWordRangeTest) {
  // Test that the editing word range is simply the cursor position if there
  // is no text.
  EXPECT_EQ(gfx::Range(0, 0), GetEditingWordRange(kEmptyText, 0));

  // Test that the editing word range contains the full word as the cursor moves
  // through a word without breaks.
  EXPECT_EQ(gfx::Range(0, 0), GetEditingWordRange(kWord, 0));
  EXPECT_EQ(gfx::Range(0, 5), GetEditingWordRange(kWord, 1));
  EXPECT_EQ(gfx::Range(0, 5), GetEditingWordRange(kWord, 2));
  EXPECT_EQ(gfx::Range(0, 5), GetEditingWordRange(kWord, 3));
  EXPECT_EQ(gfx::Range(0, 5), GetEditingWordRange(kWord, 4));
  EXPECT_EQ(gfx::Range(0, 5), GetEditingWordRange(kWord, 5));

  // Tests that the editing word range changes properly as the cursor moves from
  // non-word to word characters.
  EXPECT_EQ(gfx::Range(0, 0),
            GetEditingWordRange(kWordWithLeadingWhitespace, 0));
  EXPECT_EQ(gfx::Range(1, 1),
            GetEditingWordRange(kWordWithLeadingWhitespace, 1));
  EXPECT_EQ(gfx::Range(1, 6),
            GetEditingWordRange(kWordWithLeadingWhitespace, 2));
  EXPECT_EQ(gfx::Range(1, 6),
            GetEditingWordRange(kWordWithLeadingWhitespace, 3));
  EXPECT_EQ(gfx::Range(1, 6),
            GetEditingWordRange(kWordWithLeadingWhitespace, 4));
  EXPECT_EQ(gfx::Range(1, 6),
            GetEditingWordRange(kWordWithLeadingWhitespace, 5));
  EXPECT_EQ(gfx::Range(1, 6),
            GetEditingWordRange(kWordWithLeadingWhitespace, 6));

  // Tests that the editing word range changes properly as the cursor moves from
  // word to non-word characters.
  EXPECT_EQ(gfx::Range(0, 0),
            GetEditingWordRange(kWordWithTrailingWhitespace, 0));
  EXPECT_EQ(gfx::Range(0, 5),
            GetEditingWordRange(kWordWithTrailingWhitespace, 1));
  EXPECT_EQ(gfx::Range(0, 5),
            GetEditingWordRange(kWordWithTrailingWhitespace, 2));
  EXPECT_EQ(gfx::Range(0, 5),
            GetEditingWordRange(kWordWithTrailingWhitespace, 3));
  EXPECT_EQ(gfx::Range(0, 5),
            GetEditingWordRange(kWordWithTrailingWhitespace, 4));
  EXPECT_EQ(gfx::Range(0, 5),
            GetEditingWordRange(kWordWithTrailingWhitespace, 5));
  EXPECT_EQ(gfx::Range(6, 6),
            GetEditingWordRange(kWordWithTrailingWhitespace, 6));

  // Tests that the editing word range changes properly as the cursor moves from
  // word to non-word and back to word characters.
  EXPECT_EQ(gfx::Range(0, 0), GetEditingWordRange(kMultipleWords, 0));
  EXPECT_EQ(gfx::Range(0, 5), GetEditingWordRange(kMultipleWords, 1));
  EXPECT_EQ(gfx::Range(0, 5), GetEditingWordRange(kMultipleWords, 2));
  EXPECT_EQ(gfx::Range(0, 5), GetEditingWordRange(kMultipleWords, 3));
  EXPECT_EQ(gfx::Range(0, 5), GetEditingWordRange(kMultipleWords, 4));
  EXPECT_EQ(gfx::Range(0, 5), GetEditingWordRange(kMultipleWords, 5));
  EXPECT_EQ(gfx::Range(6, 6), GetEditingWordRange(kMultipleWords, 6));
  EXPECT_EQ(gfx::Range(6, 11), GetEditingWordRange(kMultipleWords, 7));
  EXPECT_EQ(gfx::Range(6, 11), GetEditingWordRange(kMultipleWords, 8));
  EXPECT_EQ(gfx::Range(6, 11), GetEditingWordRange(kMultipleWords, 9));
  EXPECT_EQ(gfx::Range(6, 11), GetEditingWordRange(kMultipleWords, 10));
  EXPECT_EQ(gfx::Range(6, 11), GetEditingWordRange(kMultipleWords, 11));
}