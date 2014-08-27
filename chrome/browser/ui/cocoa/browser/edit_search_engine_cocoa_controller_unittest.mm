// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/browser/edit_search_engine_cocoa_controller.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/cocoa/cocoa_profile_test.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "components/search_engines/template_url.h"
#include "grit/theme_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"

@interface FakeEditSearchEngineController : EditSearchEngineCocoaController

- (NSTextField*)nameField;
- (NSTextField*)keywordField;
- (NSTextField*)urlField;
- (NSImageView*)nameImage;
- (NSImageView*)keywordImage;
- (NSImageView*)urlImage;
- (NSButton*)doneButton;

- (NSImage*)goodImage;
- (NSImage*)badImage;

@end

@implementation FakeEditSearchEngineController

- (NSTextField*)nameField {
  return nameField_;
}

- (NSTextField*)keywordField {
  return keywordField_;
}

- (NSTextField*)urlField {
  return urlField_;
}

- (NSImageView*)nameImage {
  return nameImage_;
}

- (NSImageView*)keywordImage {
  return keywordImage_;
}

- (NSImageView*)urlImage {
  return urlImage_;
}

- (NSButton*)doneButton {
  return doneButton_;
}

- (NSImage*)goodImage {
  ui::ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  return rb.GetNativeImageNamed(IDR_INPUT_GOOD).ToNSImage();
}

- (NSImage*)badImage {
  ui::ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  return rb.GetNativeImageNamed(IDR_INPUT_ALERT).ToNSImage();
}

@end

namespace {

class EditSearchEngineControllerTest : public CocoaProfileTest {
 public:
  virtual void SetUp() {
    CocoaProfileTest::SetUp();
    ASSERT_TRUE(profile());

    controller_ =
       [[FakeEditSearchEngineController alloc] initWithProfile:profile()
                                                      delegate:nil
                                                   templateURL:nil];
  }

  virtual void TearDown() {
    // Force the window to load so we hit |-awakeFromNib| to register as the
    // window's delegate so that the controller can clean itself up in
    // |-windowWillClose:|.
    ASSERT_TRUE([controller_ window]);

    [controller_ close];
    CocoaProfileTest::TearDown();
  }

  FakeEditSearchEngineController* controller_;
};

TEST_F(EditSearchEngineControllerTest, ValidImageOriginals) {
  EXPECT_TRUE([controller_ window]);  // Force the window to load.

  EXPECT_TRUE([[controller_ goodImage] isKindOfClass:[NSImage class]]);
  EXPECT_TRUE([[controller_ badImage] isKindOfClass:[NSImage class]]);

  // Test window title is set correctly.
  NSString* title = l10n_util::GetNSString(
      IDS_SEARCH_ENGINES_EDITOR_NEW_WINDOW_TITLE);
  EXPECT_NSEQ(title, [[controller_ window] title]);
}

TEST_F(EditSearchEngineControllerTest, SetImageViews) {
  EXPECT_TRUE([controller_ window]);  // Force the window to load.
  EXPECT_EQ([controller_ badImage], [[controller_ nameImage] image]);
  // An empty keyword is not OK.
  EXPECT_EQ([controller_ badImage], [[controller_ keywordImage] image]);
  EXPECT_EQ([controller_ badImage], [[controller_ urlImage] image]);
}

// This test ensures that on creating a new keyword, we are in an "invalid"
// state that cannot save.
TEST_F(EditSearchEngineControllerTest, InvalidState) {
  EXPECT_TRUE([controller_ window]);  // Force window to load.
  NSString* toolTip = nil;
  EXPECT_FALSE([controller_ validateFields]);

  EXPECT_NSEQ(@"", [[controller_ nameField] stringValue]);
  EXPECT_EQ([controller_ badImage], [[controller_ nameImage] image]);
  toolTip = l10n_util::GetNSString(IDS_SEARCH_ENGINES_INVALID_TITLE_TT);
  EXPECT_NSEQ(toolTip, [[controller_ nameField] toolTip]);
  EXPECT_NSEQ(toolTip, [[controller_ nameImage] toolTip]);

  // Keywords can not be empty strings.
  EXPECT_NSEQ(@"", [[controller_ keywordField] stringValue]);
  EXPECT_EQ([controller_ badImage], [[controller_ keywordImage] image]);
  EXPECT_TRUE([[controller_ keywordField] toolTip]);
  EXPECT_TRUE([[controller_ keywordImage] toolTip]);

  EXPECT_NSEQ(@"", [[controller_ urlField] stringValue]);
  EXPECT_EQ([controller_ badImage], [[controller_ urlImage] image]);
  toolTip = l10n_util::GetNSString(IDS_SEARCH_ENGINES_INVALID_URL_TT);
  EXPECT_NSEQ(toolTip, [[controller_ urlField] toolTip]);
  EXPECT_NSEQ(toolTip, [[controller_ urlImage] toolTip]);
}

// Tests that the single name field validates.
TEST_F(EditSearchEngineControllerTest, ValidateName) {
  EXPECT_TRUE([controller_ window]);  // Force window to load.

  EXPECT_EQ([controller_ badImage], [[controller_ nameImage] image]);
  EXPECT_FALSE([controller_ validateFields]);
  NSString* toolTip =
      l10n_util::GetNSString(IDS_SEARCH_ENGINES_INVALID_TITLE_TT);
  EXPECT_NSEQ(toolTip, [[controller_ nameField] toolTip]);
  EXPECT_NSEQ(toolTip, [[controller_ nameImage] toolTip]);
  [[controller_ nameField] setStringValue:@"Test Name"];
  EXPECT_FALSE([controller_ validateFields]);
  EXPECT_EQ([controller_ goodImage], [[controller_ nameImage] image]);
  EXPECT_FALSE([[controller_ nameField] toolTip]);
  EXPECT_FALSE([[controller_ nameImage] toolTip]);
  EXPECT_FALSE([[controller_ doneButton] isEnabled]);
}

// The keyword field is not valid if it is empty.
TEST_F(EditSearchEngineControllerTest, ValidateKeyword) {
  EXPECT_TRUE([controller_ window]);  // Force window load.

  EXPECT_EQ([controller_ badImage], [[controller_ keywordImage] image]);
  EXPECT_FALSE([controller_ validateFields]);
  EXPECT_TRUE([[controller_ keywordField] toolTip]);
  EXPECT_TRUE([[controller_ keywordImage] toolTip]);
  [[controller_ keywordField] setStringValue:@"foobar"];
  EXPECT_FALSE([controller_ validateFields]);
  EXPECT_EQ([controller_ goodImage], [[controller_ keywordImage] image]);
  EXPECT_FALSE([[controller_ keywordField] toolTip]);
  EXPECT_FALSE([[controller_ keywordImage] toolTip]);
  EXPECT_FALSE([[controller_ doneButton] isEnabled]);
}

// Tests that the URL field validates.
TEST_F(EditSearchEngineControllerTest, ValidateURL) {
  EXPECT_TRUE([controller_ window]);  // Force window to load.

  EXPECT_EQ([controller_ badImage], [[controller_ urlImage] image]);
  EXPECT_FALSE([controller_ validateFields]);
  NSString* toolTip =
      l10n_util::GetNSString(IDS_SEARCH_ENGINES_INVALID_URL_TT);
  EXPECT_NSEQ(toolTip, [[controller_ urlField] toolTip]);
  EXPECT_NSEQ(toolTip, [[controller_ urlImage] toolTip]);
  [[controller_ urlField] setStringValue:@"http://foo-bar.com"];
  EXPECT_FALSE([controller_ validateFields]);
  EXPECT_EQ([controller_ goodImage], [[controller_ urlImage] image]);
  EXPECT_FALSE([[controller_ urlField] toolTip]);
  EXPECT_FALSE([[controller_ urlImage] toolTip]);
  EXPECT_FALSE([[controller_ doneButton] isEnabled]);
}

// Tests that if the user enters all valid data that the UI reflects that
// and that they can save.
TEST_F(EditSearchEngineControllerTest, ValidateFields) {
  EXPECT_TRUE([controller_ window]);  // Force window to load.

  // State before entering data.
  EXPECT_EQ([controller_ badImage], [[controller_ nameImage] image]);
  EXPECT_EQ([controller_ badImage], [[controller_ keywordImage] image]);
  EXPECT_EQ([controller_ badImage], [[controller_ urlImage] image]);
  EXPECT_FALSE([[controller_ doneButton] isEnabled]);
  EXPECT_FALSE([controller_ validateFields]);

  [[controller_ nameField] setStringValue:@"Test Name"];
  EXPECT_FALSE([controller_ validateFields]);
  EXPECT_EQ([controller_ goodImage], [[controller_ nameImage] image]);
  EXPECT_FALSE([[controller_ doneButton] isEnabled]);

  [[controller_ keywordField] setStringValue:@"foobar"];
  EXPECT_FALSE([controller_ validateFields]);
  EXPECT_EQ([controller_ goodImage], [[controller_ keywordImage] image]);
  EXPECT_FALSE([[controller_ doneButton] isEnabled]);

  // Once the URL is entered, we should have all 3 valid fields.
  [[controller_ urlField] setStringValue:@"http://foo-bar.com"];
  EXPECT_TRUE([controller_ validateFields]);
  EXPECT_EQ([controller_ goodImage], [[controller_ urlImage] image]);
  EXPECT_TRUE([[controller_ doneButton] isEnabled]);
}

// Tests editing an existing TemplateURL.
TEST_F(EditSearchEngineControllerTest, EditTemplateURL) {
  TemplateURLData data;
  data.short_name = base::ASCIIToUTF16("Foobar");
  data.SetKeyword(base::ASCIIToUTF16("keyword"));
  std::string urlString = TemplateURLRef::DisplayURLToURLRef(
      base::ASCIIToUTF16("http://foo-bar.com"));
  data.SetURL(urlString);
  TemplateURL url(data);
  FakeEditSearchEngineController *controller =
      [[FakeEditSearchEngineController alloc] initWithProfile:profile()
                                                     delegate:nil
                                                  templateURL:&url];
  EXPECT_TRUE([controller window]);
  NSString* title = l10n_util::GetNSString(
      IDS_SEARCH_ENGINES_EDITOR_EDIT_WINDOW_TITLE);
  EXPECT_NSEQ(title, [[controller window] title]);
  NSString* nameString = [[controller nameField] stringValue];
  EXPECT_NSEQ(@"Foobar", nameString);
  NSString* keywordString = [[controller keywordField] stringValue];
  EXPECT_NSEQ(@"keyword", keywordString);
  NSString* urlValueString = [[controller urlField] stringValue];
  EXPECT_NSEQ(@"http://foo-bar.com", urlValueString);
  EXPECT_TRUE([controller validateFields]);
  [controller close];
}

}  // namespace
