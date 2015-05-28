// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/omnibox/omnibox_popup_cell.h"

#include "base/json/json_reader.h"
#include "base/mac/scoped_nsobject.h"
#include "base/values.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#include "components/omnibox/suggestion_answer.h"
#import "testing/gtest_mac.h"

namespace {

class OmniboxPopupCellTest : public CocoaTest {
 public:
  OmniboxPopupCellTest() {
  }

  void SetUp() override {
    CocoaTest::SetUp();
    cell_.reset([[OmniboxPopupCell alloc] initTextCell:@""]);
    button_.reset([[NSButton alloc] initWithFrame:NSMakeRect(0, 0, 200, 20)]);
    [button_ setCell:cell_];
    [[test_window() contentView] addSubview:button_];
  };

 protected:
  base::scoped_nsobject<OmniboxPopupCell> cell_;
  base::scoped_nsobject<NSButton> button_;

 private:
  DISALLOW_COPY_AND_ASSIGN(OmniboxPopupCellTest);
};

TEST_VIEW(OmniboxPopupCellTest, button_);

TEST_F(OmniboxPopupCellTest, Image) {
  [cell_ setImage:[NSImage imageNamed:NSImageNameInfo]];
  [button_ display];
}

TEST_F(OmniboxPopupCellTest, Title) {
  base::scoped_nsobject<NSAttributedString> text([[NSAttributedString alloc]
      initWithString:@"The quick brown fox jumps over the lazy dog."]);
  [cell_ setAttributedTitle:text];
  [button_ display];
}

TEST_F(OmniboxPopupCellTest, AnswerStyle) {
  const char* weatherJson =
      "{\"l\": [ {\"il\": {\"t\": [ {"
      "\"t\": \"weather in pari&lt;b&gt;s&lt;/b&gt;\", \"tt\": 8} ]}}, {"
      "\"il\": {\"at\": {\"t\": \"Thu\",\"tt\": 12}, "
      "\"i\": {\"d\": \"//ssl.gstatic.com/onebox/weather/64/cloudy.png\","
      "\"t\": 3}, \"t\": [ {\"t\": \"46\",\"tt\": 1}, {"
      "\"t\": \"°F\",\"tt\": 3} ]}} ]}";
  NSString* finalString = @"46°F Thu";

  scoped_ptr<base::Value> root(base::JSONReader::Read(weatherJson));
  ASSERT_NE(root, nullptr);
  base::DictionaryValue* dictionary;
  root->GetAsDictionary(&dictionary);
  ASSERT_NE(dictionary, nullptr);
  AutocompleteMatch match;
  match.answer = SuggestionAnswer::ParseAnswer(dictionary);
  EXPECT_TRUE(match.answer);
  [cell_ setMatch:match];
  EXPECT_NSEQ([[cell_ description] string], finalString);
  size_t length = [[cell_ description] length];
  const NSRange checkValues[] = {{0, 2}, {2, 2}, {4, 4}};
  EXPECT_EQ(length, 8UL);
  NSDictionary* lastAttributes = nil;
  for (const NSRange& value : checkValues) {
    NSRange range;
    NSDictionary* currentAttributes =
        [[cell_ description] attributesAtIndex:value.location
                                effectiveRange:&range];
    EXPECT_TRUE(NSEqualRanges(value, range));
    EXPECT_FALSE([currentAttributes isEqualToDictionary:lastAttributes]);
    lastAttributes = currentAttributes;
  }
}

}  // namespace
