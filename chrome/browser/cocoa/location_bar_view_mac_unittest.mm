// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_nsobject.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "chrome/browser/cocoa/autocomplete_text_field.h"
#include "chrome/browser/cocoa/autocomplete_text_field_cell.h"
#include "chrome/browser/cocoa/browser_test_helper.h"
#include "chrome/browser/cocoa/location_bar_view_mac.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

// TODO(shess): Figure out how to unittest this.  The code below was
// testing the hacked-up behavior so you didn't have to be pedantic
// WRT http://.  But that approach is completely and utterly wrong in
// a world where omnibox is running.
// http://code.google.com/p/chromium/issues/detail?id=9977

#if 0
class LocationBarViewMacTest : public testing::Test {
 public:
  LocationBarViewMacTest()
      : field_([[NSTextField alloc] init]),
        locationBarView_(new LocationBarViewMac(field_, NULL, NULL)) {
  }

  scoped_nsobject<NSTextField> field_;
  scoped_ptr<LocationBarViewMac> locationBarView_;
};

TEST_F(LocationBarViewMacTest, GetInputString) {
  // Test a few obvious cases to make sure things work end-to-end, but
  // trust url_fixer_upper_unittest.cc to do the bulk of the work.
  [field_ setStringValue:@"ahost"];
  EXPECT_EQ(locationBarView_->GetInputString(), ASCIIToWide("http://ahost/"));

  [field_ setStringValue:@"bhost\n"];
  EXPECT_EQ(locationBarView_->GetInputString(), ASCIIToWide("http://bhost/"));

  [field_ setStringValue:@"chost/"];
  EXPECT_EQ(locationBarView_->GetInputString(), ASCIIToWide("http://chost/"));

  [field_ setStringValue:@"www.example.com"];
  EXPECT_EQ(locationBarView_->GetInputString(),
            ASCIIToWide("http://www.example.com/"));

  [field_ setStringValue:@"http://example.com"];
  EXPECT_EQ(locationBarView_->GetInputString(),
            ASCIIToWide("http://example.com/"));

  [field_ setStringValue:@"https://www.example.com"];
  EXPECT_EQ(locationBarView_->GetInputString(),
            ASCIIToWide("https://www.example.com/"));
}
#endif

namespace {

class LocationBarViewMacTest : public testing::Test {
 public:
  LocationBarViewMacTest() {
    // Make sure this is wide enough to play games with the cell
    // decorations.
    NSRect frame = NSMakeRect(0, 0, 300, 30);
    field_.reset([[AutocompleteTextField alloc] initWithFrame:frame]);
    [field_ setStringValue:@"Testing"];
    [cocoa_helper_.contentView() addSubview:field_.get()];
  }

  CocoaTestHelper cocoa_helper_;  // Inits Cocoa, creates window, etc...
  BrowserTestHelper helper_;
  scoped_nsobject<AutocompleteTextField> field_;
};

TEST_F(LocationBarViewMacTest, OnChangedImpl) {
  AutocompleteTextFieldCell* cell = [field_ autocompleteTextFieldCell];
  NSImage* image = [NSImage imageNamed:@"NSApplicationIcon"];

  const std::wstring kKeyword(L"Google");
  const NSString* kSearchHint = @"Type to search";
  const NSString* kKeywordPrefix = @"Press ";
  const NSString* kKeywordSuffix = @" to search Google";
  const NSString* kKeywordString = @"Search Google:";

  // With no special hints requested, none set.
  LocationBarViewMac::OnChangedImpl(
      field_.get(), std::wstring(), std::wstring(), false, false, image);
  EXPECT_FALSE([cell keywordString]);
  EXPECT_FALSE([cell hintString]);

  // Request only a search hint.
  LocationBarViewMac::OnChangedImpl(
      field_.get(), std::wstring(), std::wstring(), false, true, image);
  EXPECT_FALSE([cell keywordString]);
  EXPECT_TRUE([[[cell hintString] string] isEqualToString:kSearchHint]);

  // Request a keyword hint, same results whether |search_hint|
  // parameter is true or false.
  LocationBarViewMac::OnChangedImpl(
      field_.get(), kKeyword, kKeyword, true, true, image);
  EXPECT_FALSE([cell keywordString]);
  EXPECT_TRUE([[[cell hintString] string] hasPrefix:kKeywordPrefix]);
  EXPECT_TRUE([[[cell hintString] string] hasSuffix:kKeywordSuffix]);
  LocationBarViewMac::OnChangedImpl(
      field_.get(), kKeyword, kKeyword, true, false, image);
  EXPECT_FALSE([cell keywordString]);
  EXPECT_TRUE([[[cell hintString] string] hasPrefix:kKeywordPrefix]);
  EXPECT_TRUE([[[cell hintString] string] hasSuffix:kKeywordSuffix]);

  // Request keyword-search mode, same results whether |search_hint|
  // parameter is true or false.
  LocationBarViewMac::OnChangedImpl(
      field_.get(), kKeyword, kKeyword, false, true, image);
  EXPECT_TRUE([[[cell keywordString] string] isEqualToString:kKeywordString]);
  EXPECT_FALSE([cell hintString]);
  LocationBarViewMac::OnChangedImpl(
      field_.get(), kKeyword, kKeyword, false, false, image);
  EXPECT_TRUE([[[cell keywordString] string] isEqualToString:kKeywordString]);
  EXPECT_FALSE([cell hintString]);

  // Transition back to baseline.
  LocationBarViewMac::OnChangedImpl(
      field_.get(), std::wstring(), std::wstring(), false, false, image);
  EXPECT_FALSE([cell keywordString]);
  EXPECT_FALSE([cell hintString]);
}

}  // namespace
