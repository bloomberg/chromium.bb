// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/autofill/autofill_suggestion_container.h"

#import "chrome/browser/ui/cocoa/autofill/autofill_textfield.h"
#import "chrome/browser/ui/cocoa/hyperlink_button_cell.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "ui/base/test/ui_cocoa_test_helper.h"

@interface AutofillSuggestionMockDelegate
  : NSObject<AutofillSuggestionEditDelegate>
@end

@implementation AutofillSuggestionMockDelegate

- (void)editLinkClicked {}
- (NSString*)editLinkTitle { return @"title"; }

@end

namespace {

class AutofillSuggestionContainerTest : public ui::CocoaTest {
 public:
  virtual void SetUp() {
    CocoaTest::SetUp();
    delegate_.reset([[AutofillSuggestionMockDelegate alloc] init]);
    container_.reset([[AutofillSuggestionContainer alloc] initWithDelegate:
        delegate_.get()]);
    [[test_window() contentView] addSubview:[container_ view]];
  }

 protected:
  scoped_nsobject<AutofillSuggestionContainer> container_;
  scoped_nsobject<AutofillSuggestionMockDelegate> delegate_;
};

}  // namespace

TEST_VIEW(AutofillSuggestionContainerTest, [container_ view])

TEST_F(AutofillSuggestionContainerTest, HasSubviews) {
  ASSERT_EQ(5U, [[[container_ view] subviews] count]);

  int num_text_fields = 0;
  bool has_link = false;
  bool has_edit_field = false;
  bool has_icon = false;

  for (id view in [[container_ view] subviews]) {
    if ([view isKindOfClass:[NSImageView class]]) {
      has_icon = true;
    } else if ([view isKindOfClass:[AutofillTextField class]]) {
      has_edit_field = true;
    } else if ([view isKindOfClass:[NSTextField class]]) {
      num_text_fields++;
    } else if ([[view cell] isKindOfClass:[HyperlinkButtonCell class]]) {
      has_link = true;
    }
  }

  EXPECT_EQ(2, num_text_fields);
  EXPECT_TRUE(has_edit_field);
  EXPECT_TRUE(has_link);
  EXPECT_TRUE(has_icon);
}