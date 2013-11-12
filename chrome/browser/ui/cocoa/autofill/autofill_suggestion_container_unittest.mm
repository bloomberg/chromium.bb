// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/autofill/autofill_suggestion_container.h"

#import "chrome/browser/ui/cocoa/autofill/autofill_textfield.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "ui/base/test/ui_cocoa_test_helper.h"

namespace {

class AutofillSuggestionContainerTest : public ui::CocoaTest {
 public:
  virtual void SetUp() {
    CocoaTest::SetUp();
    container_.reset([[AutofillSuggestionContainer alloc] init]);
    [[test_window() contentView] addSubview:[container_ view]];
  }

 protected:
  base::scoped_nsobject<AutofillSuggestionContainer> container_;
};

}  // namespace

TEST_VIEW(AutofillSuggestionContainerTest, [container_ view])

TEST_F(AutofillSuggestionContainerTest, HasSubviews) {
  ASSERT_EQ(5U, [[[container_ view] subviews] count]);

  bool has_text_view = false;
  bool has_text_field = false;
  bool has_edit_field = false;
  bool has_icon = false;

  // Ignore |spacer_| NSBox in this test.
  for (id view in [[container_ view] subviews]) {
    if ([view isKindOfClass:[NSImageView class]]) {
      has_icon = true;
    } else if ([view isKindOfClass:[AutofillTextField class]]) {
      has_edit_field = true;
    } else if ([view isKindOfClass:[NSTextField class]]) {
      has_text_field = true;
    } else if ([view isKindOfClass:[NSTextView class]]) {
      has_text_view = true;
    }
  }

  EXPECT_TRUE(has_text_field);
  EXPECT_TRUE(has_text_view);
  EXPECT_TRUE(has_edit_field);
  EXPECT_TRUE(has_icon);
}
