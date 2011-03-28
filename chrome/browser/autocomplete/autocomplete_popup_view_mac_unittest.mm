// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/autocomplete/autocomplete_popup_view_mac.h"

#include "base/memory/scoped_ptr.h"
#include "base/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete.h"
#include "testing/platform_test.h"
#include "ui/base/text/text_elider.h"

namespace {

const float kLargeWidth = 10000;

class AutocompletePopupViewMacTest : public PlatformTest {
 public:
  AutocompletePopupViewMacTest() {}

  virtual void SetUp() {
    PlatformTest::SetUp();

    // These are here because there is no autorelease pool for the
    // constructor.
    color_ = [NSColor blackColor];
    dimColor_ = [NSColor darkGrayColor];
    font_ = gfx::Font(
        base::SysNSStringToUTF16([[NSFont userFontOfSize:12] fontName]), 12);
  }

  // Returns the length of the run starting at |location| for which
  // |attributeName| remains the same.
  static NSUInteger RunLengthForAttribute(NSAttributedString* string,
                                          NSUInteger location,
                                          NSString* attributeName) {
    const NSRange fullRange = NSMakeRange(0, [string length]);
    NSRange range;
    [string attribute:attributeName
              atIndex:location longestEffectiveRange:&range inRange:fullRange];

    // In order to signal when the run doesn't start exactly at
    // location, return a weirdo length.  This causes the incorrect
    // expectation to manifest at the calling location, which is more
    // useful than an EXPECT_EQ() would be here.
    if (range.location != location) {
      return -1;
    }

    return range.length;
  }

  // Return true if the run starting at |location| has |color| for
  // attribute NSForegroundColorAttributeName.
  static bool RunHasColor(NSAttributedString* string,
                          NSUInteger location, NSColor* color) {
    const NSRange fullRange = NSMakeRange(0, [string length]);
    NSRange range;
    NSColor* runColor = [string attribute:NSForegroundColorAttributeName
                                  atIndex:location
                    longestEffectiveRange:&range inRange:fullRange];

    // According to one "Ali Ozer", you can compare objects within the
    // same color space using -isEqual:.  Converting color spaces
    // seems too heavyweight for these tests.
    // http://lists.apple.com/archives/cocoa-dev/2005/May/msg00186.html
    return [runColor isEqual:color] ? true : false;
  }

  // Return true if the run starting at |location| has the font
  // trait(s) in |mask| font in NSFontAttributeName.
  static bool RunHasFontTrait(NSAttributedString* string, NSUInteger location,
                              NSFontTraitMask mask) {
    const NSRange fullRange = NSMakeRange(0, [string length]);
    NSRange range;
    NSFont* runFont = [string attribute:NSFontAttributeName
                                atIndex:location
                  longestEffectiveRange:&range inRange:fullRange];
    NSFontManager* fontManager = [NSFontManager sharedFontManager];
    if (runFont && (mask == ([fontManager traitsOfFont:runFont]&mask))) {
      return true;
    }
    return false;
  }

  // AutocompleteMatch doesn't really have the right constructor for our
  // needs.  Fake one for us to use.
  static AutocompleteMatch MakeMatch(const string16 &contents,
                                     const string16 &description) {
    AutocompleteMatch m(NULL, 1, true, AutocompleteMatch::URL_WHAT_YOU_TYPED);
    m.contents = contents;
    m.description = description;
    return m;
  }

  NSColor* color_;  // weak
  NSColor* dimColor_;  // weak
  gfx::Font font_;
};

// Simple inputs with no matches should result in styled output who's
// text matches the input string, with the passed-in color, and
// nothing bolded.
TEST_F(AutocompletePopupViewMacTest, DecorateMatchedStringNoMatch) {
  NSString* const string = @"This is a test";
  AutocompleteMatch::ACMatchClassifications classifications;

  NSAttributedString* decorated =
      AutocompletePopupViewMac::DecorateMatchedString(
          base::SysNSStringToUTF16(string), classifications,
          color_, dimColor_, font_);

  // Result has same characters as the input.
  EXPECT_EQ([decorated length], [string length]);
  EXPECT_TRUE([[decorated string] isEqualToString:string]);

  // Our passed-in color for the entire string.
  EXPECT_EQ(RunLengthForAttribute(decorated, 0U,
                                  NSForegroundColorAttributeName),
            [string length]);
  EXPECT_TRUE(RunHasColor(decorated, 0U, color_));

  // An unbolded font for the entire string.
  EXPECT_EQ(RunLengthForAttribute(decorated, 0U,
                                  NSFontAttributeName), [string length]);
  EXPECT_FALSE(RunHasFontTrait(decorated, 0U, NSBoldFontMask));
}

// Identical to DecorateMatchedStringNoMatch, except test that URL
// style gets a different color than we passed in.
TEST_F(AutocompletePopupViewMacTest, DecorateMatchedStringURLNoMatch) {
  NSString* const string = @"This is a test";
  AutocompleteMatch::ACMatchClassifications classifications;

  classifications.push_back(
      ACMatchClassification(0, ACMatchClassification::URL));

  NSAttributedString* decorated =
      AutocompletePopupViewMac::DecorateMatchedString(
          base::SysNSStringToUTF16(string), classifications,
          color_, dimColor_, font_);

  // Result has same characters as the input.
  EXPECT_EQ([decorated length], [string length]);
  EXPECT_TRUE([[decorated string] isEqualToString:string]);

  // One color for the entire string, and it's not the one we passed in.
  EXPECT_EQ(RunLengthForAttribute(decorated, 0U,
                                  NSForegroundColorAttributeName),
            [string length]);
  EXPECT_FALSE(RunHasColor(decorated, 0U, color_));

  // An unbolded font for the entire string.
  EXPECT_EQ(RunLengthForAttribute(decorated, 0U,
                                  NSFontAttributeName), [string length]);
  EXPECT_FALSE(RunHasFontTrait(decorated, 0U, NSBoldFontMask));
}

// Test that DIM works as expected.
TEST_F(AutocompletePopupViewMacTest, DecorateMatchedStringDimNoMatch) {
  NSString* const string = @"This is a test";
  // Dim "is".
  const NSUInteger runLength1 = 5, runLength2 = 2, runLength3 = 7;
  // Make sure nobody messed up the inputs.
  EXPECT_EQ(runLength1 + runLength2 + runLength3, [string length]);

  // Push each run onto classifications.
  AutocompleteMatch::ACMatchClassifications classifications;
  classifications.push_back(
      ACMatchClassification(0, ACMatchClassification::NONE));
  classifications.push_back(
      ACMatchClassification(runLength1, ACMatchClassification::DIM));
  classifications.push_back(
      ACMatchClassification(runLength1 + runLength2,
                            ACMatchClassification::NONE));

  NSAttributedString* decorated =
      AutocompletePopupViewMac::DecorateMatchedString(
          base::SysNSStringToUTF16(string), classifications,
          color_, dimColor_, font_);

  // Result has same characters as the input.
  EXPECT_EQ([decorated length], [string length]);
  EXPECT_TRUE([[decorated string] isEqualToString:string]);

  // Should have three font runs, normal, dim, normal.
  EXPECT_EQ(RunLengthForAttribute(decorated, 0U,
                                  NSForegroundColorAttributeName),
            runLength1);
  EXPECT_TRUE(RunHasColor(decorated, 0U, color_));

  EXPECT_EQ(RunLengthForAttribute(decorated, runLength1,
                                  NSForegroundColorAttributeName),
            runLength2);
  EXPECT_TRUE(RunHasColor(decorated, runLength1, dimColor_));

  EXPECT_EQ(RunLengthForAttribute(decorated, runLength1 + runLength2,
                                  NSForegroundColorAttributeName),
            runLength3);
  EXPECT_TRUE(RunHasColor(decorated, runLength1 + runLength2, color_));

  // An unbolded font for the entire string.
  EXPECT_EQ(RunLengthForAttribute(decorated, 0U,
                                  NSFontAttributeName), [string length]);
  EXPECT_FALSE(RunHasFontTrait(decorated, 0U, NSBoldFontMask));
}

// Test that the matched run gets bold-faced, but keeps the same
// color.
TEST_F(AutocompletePopupViewMacTest, DecorateMatchedStringMatch) {
  NSString* const string = @"This is a test";
  // Match "is".
  const NSUInteger runLength1 = 5, runLength2 = 2, runLength3 = 7;
  // Make sure nobody messed up the inputs.
  EXPECT_EQ(runLength1 + runLength2 + runLength3, [string length]);

  // Push each run onto classifications.
  AutocompleteMatch::ACMatchClassifications classifications;
  classifications.push_back(
      ACMatchClassification(0, ACMatchClassification::NONE));
  classifications.push_back(
      ACMatchClassification(runLength1, ACMatchClassification::MATCH));
  classifications.push_back(
      ACMatchClassification(runLength1 + runLength2,
                            ACMatchClassification::NONE));

  NSAttributedString* decorated =
      AutocompletePopupViewMac::DecorateMatchedString(
          base::SysNSStringToUTF16(string), classifications,
          color_, dimColor_, font_);

  // Result has same characters as the input.
  EXPECT_EQ([decorated length], [string length]);
  EXPECT_TRUE([[decorated string] isEqualToString:string]);

  // Our passed-in color for the entire string.
  EXPECT_EQ(RunLengthForAttribute(decorated, 0U,
                                  NSForegroundColorAttributeName),
            [string length]);
  EXPECT_TRUE(RunHasColor(decorated, 0U, color_));

  // Should have three font runs, not bold, bold, then not bold again.
  EXPECT_EQ(RunLengthForAttribute(decorated, 0U,
                                  NSFontAttributeName), runLength1);
  EXPECT_FALSE(RunHasFontTrait(decorated, 0U, NSBoldFontMask));

  EXPECT_EQ(RunLengthForAttribute(decorated, runLength1,
                                  NSFontAttributeName), runLength2);
  EXPECT_TRUE(RunHasFontTrait(decorated, runLength1, NSBoldFontMask));

  EXPECT_EQ(RunLengthForAttribute(decorated, runLength1 + runLength2,
                                  NSFontAttributeName), runLength3);
  EXPECT_FALSE(RunHasFontTrait(decorated, runLength1 + runLength2,
                               NSBoldFontMask));
}

// Just like DecorateMatchedStringURLMatch, this time with URL style.
TEST_F(AutocompletePopupViewMacTest, DecorateMatchedStringURLMatch) {
  NSString* const string = @"http://hello.world/";
  // Match "hello".
  const NSUInteger runLength1 = 7, runLength2 = 5, runLength3 = 7;
  // Make sure nobody messed up the inputs.
  EXPECT_EQ(runLength1 + runLength2 + runLength3, [string length]);

  // Push each run onto classifications.
  AutocompleteMatch::ACMatchClassifications classifications;
  classifications.push_back(
      ACMatchClassification(0, ACMatchClassification::URL));
  const int kURLMatch = ACMatchClassification::URL|ACMatchClassification::MATCH;
  classifications.push_back(ACMatchClassification(runLength1, kURLMatch));
  classifications.push_back(
      ACMatchClassification(runLength1 + runLength2,
                            ACMatchClassification::URL));

  NSAttributedString* decorated =
      AutocompletePopupViewMac::DecorateMatchedString(
          base::SysNSStringToUTF16(string), classifications,
          color_, dimColor_, font_);

  // Result has same characters as the input.
  EXPECT_EQ([decorated length], [string length]);
  EXPECT_TRUE([[decorated string] isEqualToString:string]);

  // One color for the entire string, and it's not the one we passed in.
  EXPECT_EQ(RunLengthForAttribute(decorated, 0U,
                                  NSForegroundColorAttributeName),
            [string length]);
  EXPECT_FALSE(RunHasColor(decorated, 0U, color_));

  // Should have three font runs, not bold, bold, then not bold again.
  EXPECT_EQ(RunLengthForAttribute(decorated, 0U,
                                  NSFontAttributeName), runLength1);
  EXPECT_FALSE(RunHasFontTrait(decorated, 0U, NSBoldFontMask));

  EXPECT_EQ(RunLengthForAttribute(decorated, runLength1,
                                  NSFontAttributeName), runLength2);
  EXPECT_TRUE(RunHasFontTrait(decorated, runLength1, NSBoldFontMask));

  EXPECT_EQ(RunLengthForAttribute(decorated, runLength1 + runLength2,
                                  NSFontAttributeName), runLength3);
  EXPECT_FALSE(RunHasFontTrait(decorated, runLength1 + runLength2,
                               NSBoldFontMask));
}

// Check that matches with both contents and description come back
// with contents at the beginning, description at the end, and
// something separating them.  Not being specific about the separator
// on purpose, in case it changes.
TEST_F(AutocompletePopupViewMacTest, MatchText) {
  NSString* const contents = @"contents";
  NSString* const description = @"description";
  AutocompleteMatch m = MakeMatch(base::SysNSStringToUTF16(contents),
                                  base::SysNSStringToUTF16(description));

  NSAttributedString* decorated =
      AutocompletePopupViewMac::MatchText(m, font_, kLargeWidth);

  // Result contains the characters of the input in the right places.
  EXPECT_GT([decorated length], [contents length] + [description length]);
  EXPECT_TRUE([[decorated string] hasPrefix:contents]);
  EXPECT_TRUE([[decorated string] hasSuffix:description]);

  // Check that the description is a different color from the
  // contents.
  const NSUInteger descriptionLocation =
      [decorated length] - [description length];
  EXPECT_EQ(RunLengthForAttribute(decorated, 0U,
                                  NSForegroundColorAttributeName),
            descriptionLocation);
  EXPECT_EQ(RunLengthForAttribute(decorated, descriptionLocation,
                                  NSForegroundColorAttributeName),
            [description length]);

  // Same font all the way through, nothing bold.
  EXPECT_EQ(RunLengthForAttribute(decorated, 0U,
                                  NSFontAttributeName), [decorated length]);
  EXPECT_FALSE(RunHasFontTrait(decorated, 0, NSBoldFontMask));
}

// Check that MatchText() styles content matches as expected.
TEST_F(AutocompletePopupViewMacTest, MatchTextContentsMatch) {
  NSString* const contents = @"This is a test";
  // Match "is".
  const NSUInteger runLength1 = 5, runLength2 = 2, runLength3 = 7;
  // Make sure nobody messed up the inputs.
  EXPECT_EQ(runLength1 + runLength2 + runLength3, [contents length]);

  AutocompleteMatch m = MakeMatch(base::SysNSStringToUTF16(contents),
                                  string16());

  // Push each run onto contents classifications.
  m.contents_class.push_back(
      ACMatchClassification(0, ACMatchClassification::NONE));
  m.contents_class.push_back(
      ACMatchClassification(runLength1, ACMatchClassification::MATCH));
  m.contents_class.push_back(
      ACMatchClassification(runLength1 + runLength2,
                            ACMatchClassification::NONE));

  NSAttributedString* decorated =
      AutocompletePopupViewMac::MatchText(m, font_, kLargeWidth);

  // Result has same characters as the input.
  EXPECT_EQ([decorated length], [contents length]);
  EXPECT_TRUE([[decorated string] isEqualToString:contents]);

  // Result is all one color.
  EXPECT_EQ(RunLengthForAttribute(decorated, 0U,
                                  NSForegroundColorAttributeName),
            [contents length]);

  // Should have three font runs, not bold, bold, then not bold again.
  EXPECT_EQ(RunLengthForAttribute(decorated, 0U,
                                  NSFontAttributeName), runLength1);
  EXPECT_FALSE(RunHasFontTrait(decorated, 0U, NSBoldFontMask));

  EXPECT_EQ(RunLengthForAttribute(decorated, runLength1,
                                  NSFontAttributeName), runLength2);
  EXPECT_TRUE(RunHasFontTrait(decorated, runLength1, NSBoldFontMask));

  EXPECT_EQ(RunLengthForAttribute(decorated, runLength1 + runLength2,
                                  NSFontAttributeName), runLength3);
  EXPECT_FALSE(RunHasFontTrait(decorated, runLength1 + runLength2,
                               NSBoldFontMask));
}

// Check that MatchText() styles description matches as expected.
TEST_F(AutocompletePopupViewMacTest, MatchTextDescriptionMatch) {
  NSString* const contents = @"This is a test";
  NSString* const description = @"That was a test";
  // Match "That was".
  const NSUInteger runLength1 = 8, runLength2 = 7;
  // Make sure nobody messed up the inputs.
  EXPECT_EQ(runLength1 + runLength2, [description length]);

  AutocompleteMatch m = MakeMatch(base::SysNSStringToUTF16(contents),
                                  base::SysNSStringToUTF16(description));

  // Push each run onto contents classifications.
  m.description_class.push_back(
      ACMatchClassification(0, ACMatchClassification::MATCH));
  m.description_class.push_back(
      ACMatchClassification(runLength1, ACMatchClassification::NONE));

  NSAttributedString* decorated =
      AutocompletePopupViewMac::MatchText(m, font_, kLargeWidth);

  // Result contains the characters of the input.
  EXPECT_GT([decorated length], [contents length] + [description length]);
  EXPECT_TRUE([[decorated string] hasPrefix:contents]);
  EXPECT_TRUE([[decorated string] hasSuffix:description]);

  // Check that the description is a different color from the
  // contents.
  const NSUInteger descriptionLocation =
      [decorated length] - [description length];
  EXPECT_EQ(RunLengthForAttribute(decorated, 0U,
                                  NSForegroundColorAttributeName),
            descriptionLocation);
  EXPECT_EQ(RunLengthForAttribute(decorated, descriptionLocation,
                                  NSForegroundColorAttributeName),
            [description length]);

  // Should have three font runs, not bold, bold, then not bold again.
  // The first run is the contents and the separator, the second run
  // is the first run of the description.
  EXPECT_EQ(RunLengthForAttribute(decorated, 0U,
                                  NSFontAttributeName), descriptionLocation);
  EXPECT_FALSE(RunHasFontTrait(decorated, 0U, NSBoldFontMask));

  EXPECT_EQ(RunLengthForAttribute(decorated, descriptionLocation,
                                  NSFontAttributeName), runLength1);
  EXPECT_TRUE(RunHasFontTrait(decorated, descriptionLocation, NSBoldFontMask));

  EXPECT_EQ(RunLengthForAttribute(decorated, descriptionLocation + runLength1,
                                  NSFontAttributeName), runLength2);
  EXPECT_FALSE(RunHasFontTrait(decorated, descriptionLocation + runLength1,
                               NSBoldFontMask));
}

TEST_F(AutocompletePopupViewMacTest, ElideString) {
  NSString* const contents = @"This is a test with long contents";
  const string16 contents16(base::SysNSStringToUTF16(contents));

  const float kWide = 1000.0;
  const float kNarrow = 20.0;

  NSDictionary* attributes =
      [NSDictionary dictionaryWithObject:font_.GetNativeFont()
                                  forKey:NSFontAttributeName];
  scoped_nsobject<NSMutableAttributedString> as(
      [[NSMutableAttributedString alloc] initWithString:contents
                                             attributes:attributes]);

  // Nothing happens if the space is really wide.
  NSMutableAttributedString* ret =
      AutocompletePopupViewMac::ElideString(as, contents16, font_, kWide);
  EXPECT_TRUE(ret == as);
  EXPECT_TRUE([[as string] isEqualToString:contents]);

  // When elided, result is the same as ElideText().
  ret = AutocompletePopupViewMac::ElideString(as, contents16, font_, kNarrow);
  string16 elided = ui::ElideText(contents16, font_, kNarrow, false);
  EXPECT_TRUE(ret == as);
  EXPECT_FALSE([[as string] isEqualToString:contents]);
  EXPECT_TRUE([[as string] isEqualToString:base::SysUTF16ToNSString(elided)]);

  // When elided, result is the same as ElideText().
  ret = AutocompletePopupViewMac::ElideString(as, contents16, font_, 0.0);
  elided = ui::ElideText(contents16, font_, 0.0, false);
  EXPECT_TRUE(ret == as);
  EXPECT_FALSE([[as string] isEqualToString:contents]);
  EXPECT_TRUE([[as string] isEqualToString:base::SysUTF16ToNSString(elided)]);
}

TEST_F(AutocompletePopupViewMacTest, MatchTextElide) {
  NSString* const contents = @"This is a test with long contents";
  NSString* const description = @"That was a test";
  // Match "long".
  const NSUInteger runLength1 = 20, runLength2 = 4, runLength3 = 9;
  // Make sure nobody messed up the inputs.
  EXPECT_EQ(runLength1 + runLength2 + runLength3, [contents length]);

  AutocompleteMatch m = MakeMatch(base::SysNSStringToUTF16(contents),
                                  base::SysNSStringToUTF16(description));

  // Push each run onto contents classifications.
  m.contents_class.push_back(
      ACMatchClassification(0, ACMatchClassification::NONE));
  m.contents_class.push_back(
      ACMatchClassification(runLength1, ACMatchClassification::MATCH));
  m.contents_class.push_back(
      ACMatchClassification(runLength1 + runLength2,
                            ACMatchClassification::URL));

  // Figure out the width of the contents.
  NSDictionary* attributes =
      [NSDictionary dictionaryWithObject:font_.GetNativeFont()
                                  forKey:NSFontAttributeName];
  const float contentsWidth = [contents sizeWithAttributes:attributes].width;

  // After accounting for the width of the image, this will force us
  // to elide the contents.
  float cellWidth = ceil(contentsWidth / 0.7);

  NSAttributedString* decorated =
      AutocompletePopupViewMac::MatchText(m, font_, cellWidth);

  // Results contain a prefix of the contents and all of description.
  NSString* commonPrefix =
      [[decorated string] commonPrefixWithString:contents options:0];
  EXPECT_GT([commonPrefix length], 0U);
  EXPECT_LT([commonPrefix length], [contents length]);
  EXPECT_TRUE([[decorated string] hasSuffix:description]);

  // At one point the code had a bug where elided text was being
  // marked up using pre-elided offsets, resulting in out-of-range
  // values being passed to NSAttributedString.  Push the ellipsis
  // through part of each run to verify that we don't continue to see
  // such things.
  while([commonPrefix length] > runLength1 - 3) {
    EXPECT_GT(cellWidth, 0.0);
    cellWidth -= 1.0;
    decorated = AutocompletePopupViewMac::MatchText(m, font_, cellWidth);
    commonPrefix =
        [[decorated string] commonPrefixWithString:contents options:0];
    ASSERT_GT([commonPrefix length], 0U);
  }
}

// TODO(shess): Test that
// AutocompletePopupViewMac::UpdatePopupAppearance() creates/destroys
// the popup according to result contents.  Test that the matrix gets
// the right number of results.  Test the contents of the cells for
// the right strings.  Icons?  Maybe, that seems harder to test.
// Styling seems almost impossible.

// TODO(shess): Test that AutocompletePopupViewMac::PaintUpdatesNow()
// updates the matrix selection.

// TODO(shess): Test that AutocompletePopupViewMac::AcceptInput()
// updates the model's selection from the matrix before returning.
// Could possibly test that via -select:.

// TODO(shess): Test that AutocompleteButtonCell returns the right
// background colors for on, highlighted, and neither.

// TODO(shess): Test that AutocompleteMatrixTarget can be initialized
// and then sends -select: to the view.

}  // namespace
