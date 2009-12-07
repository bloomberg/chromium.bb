// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_nsobject.h"
#include "chrome/browser/character_encoding.h"
#include "chrome/browser/cocoa/browser_test_helper.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#import "chrome/browser/cocoa/font_language_settings_controller.h"
#include "chrome/browser/profile.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

@interface FontLanguageSettingsController (Testing)
- (void)updateDisplayField:(NSTextField*)field withFont:(NSFont*)font;
@end

class FontLanguageSettingsControllerTest : public CocoaTest {
 public:
  FontLanguageSettingsControllerTest() {
    Profile* profile = helper_.profile();
    font_controller_.reset(
        [[FontLanguageSettingsController alloc] initWithProfile:profile]);
   }
  ~FontLanguageSettingsControllerTest() {}

  BrowserTestHelper helper_;
  scoped_nsobject<FontLanguageSettingsController> font_controller_;
};

TEST_F(FontLanguageSettingsControllerTest, Init) {
  ASSERT_EQ(CharacterEncoding::GetSupportCanonicalEncodingCount(),
      static_cast<int>([[font_controller_ encodings] count]));
}

TEST_F(FontLanguageSettingsControllerTest, UpdateDisplayField) {
  NSFont* font = [NSFont fontWithName:@"Times-Roman" size:12.0];
  scoped_nsobject<NSTextField> field(
      [[NSTextField alloc] initWithFrame:NSMakeRect(100, 100, 100, 100)]);

  [font_controller_ updateDisplayField:field.get() withFont:font];

  ASSERT_TRUE([[font fontName] isEqualToString:[[field font] fontName]]);
  ASSERT_TRUE([@"Times-Roman, 12" isEqualToString:[field stringValue]]);
}

TEST_F(FontLanguageSettingsControllerTest, UpdateDisplayFieldNilFont) {
  scoped_nsobject<NSTextField> field(
      [[NSTextField alloc] initWithFrame:NSMakeRect(100, 100, 100, 100)]);
  [field setStringValue:@"foo"];

  [font_controller_ updateDisplayField:field.get() withFont:nil];

  ASSERT_TRUE([@"foo" isEqualToString:[field stringValue]]);
}

TEST_F(FontLanguageSettingsControllerTest, UpdateDisplayFieldNilField) {
  // Don't crash.
  NSFont* font = [NSFont fontWithName:@"Times-Roman" size:12.0];
  [font_controller_ updateDisplayField:nil withFont:font];
}
