// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#import "chrome/browser/ui/cocoa/test/cocoa_test_helper.h"
#import "chrome/browser/ui/cocoa/touchbar/suggested_text_touch_bar_controller.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "ui/base/cocoa/touch_bar_util.h"
#include "ui/gfx/range/range.h"

const base::string16 kEmptyText(base::ASCIIToUTF16(""));
const base::string16 kWord(base::ASCIIToUTF16("hello"));
const base::string16 kWordWithTrailingWhitespace(base::ASCIIToUTF16("hello "));
const base::string16 kWordWithLeadingWhitespace(base::ASCIIToUTF16(" hello"));
const base::string16 kMultipleWords(base::ASCIIToUTF16("hello world"));
const base::string16 kWhitespace(base::ASCIIToUTF16("     "));

class SuggestedTextTouchBarControllerUnitTest : public CocoaTest {
 public:
  void SetUp() override {
    CocoaTest::SetUp();
    controller_.reset([[SuggestedTextTouchBarController alloc] init]);
  }

  gfx::Range GetEditingWordRange(const base::string16& text, size_t cursor) {
    NSRange range =
        [controller_.get() editingWordRangeFromText:text cursorPosition:cursor];
    return gfx::Range(range);
  }

  base::scoped_nsobject<SuggestedTextTouchBarController> controller_;
};

TEST_F(SuggestedTextTouchBarControllerUnitTest, CollapsedCandidateListTest) {
  if (@available(macOS 10.12.2, *)) {
    base::scoped_nsobject<NSCandidateListTouchBarItem> item;

    [controller_ setSelectionRange:NSMakeRange(0, 0)];
    item.reset([controller_ createCandidateListItem]);
    EXPECT_FALSE([item isCollapsed]);

    [controller_ setSelectionRange:NSMakeRange(0, 1)];
    item.reset([controller_ createCandidateListItem]);
    EXPECT_TRUE([item isCollapsed]);
  }
}

// Tests that the editing word range is simply the cursor position if there
// is no text.
TEST_F(SuggestedTextTouchBarControllerUnitTest, EmptyTextEditingWordRangeTest) {
  EXPECT_EQ(gfx::Range(0, 0), GetEditingWordRange(kEmptyText, 0));
}

// Tests that the editing word range contains the full word as the cursor
// moves through a word without breaks.
TEST_F(SuggestedTextTouchBarControllerUnitTest, WordEditingWordRangeTest) {
  EXPECT_EQ(gfx::Range(0, 0), GetEditingWordRange(kWord, 0));
  EXPECT_EQ(gfx::Range(0, 5), GetEditingWordRange(kWord, 1));
  EXPECT_EQ(gfx::Range(0, 5), GetEditingWordRange(kWord, 2));
  EXPECT_EQ(gfx::Range(0, 5), GetEditingWordRange(kWord, 3));
  EXPECT_EQ(gfx::Range(0, 5), GetEditingWordRange(kWord, 4));
  EXPECT_EQ(gfx::Range(0, 5), GetEditingWordRange(kWord, 5));
}

// Tests that the editing word range is properly calculated as the cursor moves
// through non-word characters.
TEST_F(SuggestedTextTouchBarControllerUnitTest,
       WhitespaceEditingWordRangeTest) {
  EXPECT_EQ(gfx::Range(0, 5), GetEditingWordRange(kWhitespace, 0));
  EXPECT_EQ(gfx::Range(1, 5), GetEditingWordRange(kWhitespace, 1));
  EXPECT_EQ(gfx::Range(2, 5), GetEditingWordRange(kWhitespace, 2));
  EXPECT_EQ(gfx::Range(3, 5), GetEditingWordRange(kWhitespace, 3));
  EXPECT_EQ(gfx::Range(4, 5), GetEditingWordRange(kWhitespace, 4));
  EXPECT_EQ(gfx::Range(5, 5), GetEditingWordRange(kWhitespace, 5));
}

// Tests that the editing word range changes properly as the cursor moves
// from word to non-word characters.
TEST_F(SuggestedTextTouchBarControllerUnitTest,
       TrailingWhitespaceEditingWordRangeTest) {
  EXPECT_EQ(gfx::Range(0, 0),
            GetEditingWordRange(kWordWithTrailingWhitespace, 0));
  EXPECT_EQ(gfx::Range(0, 6),
            GetEditingWordRange(kWordWithTrailingWhitespace, 1));
  EXPECT_EQ(gfx::Range(0, 6),
            GetEditingWordRange(kWordWithTrailingWhitespace, 2));
  EXPECT_EQ(gfx::Range(0, 6),
            GetEditingWordRange(kWordWithTrailingWhitespace, 3));
  EXPECT_EQ(gfx::Range(0, 6),
            GetEditingWordRange(kWordWithTrailingWhitespace, 4));
  EXPECT_EQ(gfx::Range(0, 6),
            GetEditingWordRange(kWordWithTrailingWhitespace, 5));
  EXPECT_EQ(gfx::Range(6, 6),
            GetEditingWordRange(kWordWithTrailingWhitespace, 6));
}

// Tests that the editing word range changes properly as the cursor moves
// from non-word to word characters.
TEST_F(SuggestedTextTouchBarControllerUnitTest,
       LeadingWhitespaceEditingWordRangeTest) {
  EXPECT_EQ(gfx::Range(0, 1),
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
}

// Tests that the editing word range is properly calculated as the cursor moves
// from word to non-word and back to word characters.
TEST_F(SuggestedTextTouchBarControllerUnitTest,
       MultipleWordsEditingWordRangeTest) {
  EXPECT_EQ(gfx::Range(0, 0), GetEditingWordRange(kMultipleWords, 0));
  EXPECT_EQ(gfx::Range(0, 6), GetEditingWordRange(kMultipleWords, 1));
  EXPECT_EQ(gfx::Range(0, 6), GetEditingWordRange(kMultipleWords, 2));
  EXPECT_EQ(gfx::Range(0, 6), GetEditingWordRange(kMultipleWords, 3));
  EXPECT_EQ(gfx::Range(0, 6), GetEditingWordRange(kMultipleWords, 4));
  EXPECT_EQ(gfx::Range(0, 6), GetEditingWordRange(kMultipleWords, 5));
  EXPECT_EQ(gfx::Range(6, 6), GetEditingWordRange(kMultipleWords, 6));
  EXPECT_EQ(gfx::Range(6, 11), GetEditingWordRange(kMultipleWords, 7));
  EXPECT_EQ(gfx::Range(6, 11), GetEditingWordRange(kMultipleWords, 8));
  EXPECT_EQ(gfx::Range(6, 11), GetEditingWordRange(kMultipleWords, 9));
  EXPECT_EQ(gfx::Range(6, 11), GetEditingWordRange(kMultipleWords, 10));
  EXPECT_EQ(gfx::Range(6, 11), GetEditingWordRange(kMultipleWords, 11));
}

TEST_F(SuggestedTextTouchBarControllerUnitTest, TouchBarMetricTest) {
  if (@available(macOS 10.12.2, *)) {
    base::HistogramTester histogram_tester;
    [controller_ candidateListTouchBarItem:nil endSelectingCandidateAtIndex:1];
    histogram_tester.ExpectBucketCount("TouchBar.Default.Metrics",
                                       ui::TouchBarAction::TEXT_SUGGESTION, 1);
  }
}