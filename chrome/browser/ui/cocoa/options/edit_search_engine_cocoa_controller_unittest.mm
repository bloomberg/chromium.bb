// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_nsobject.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/ui/cocoa/browser_test_helper.h"
#include "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#import "chrome/browser/ui/cocoa/options/edit_search_engine_cocoa_controller.h"
#include "chrome/test/testing_profile.h"
#include "grit/generated_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "ui/base/l10n/l10n_util_mac.h"

@interface FakeEditSearchEngineController : EditSearchEngineCocoaController {
}
@property (nonatomic, readonly) NSTextField* nameField;
@property (nonatomic, readonly) NSTextField* keywordField;
@property (nonatomic, readonly) NSTextField* urlField;
@property (nonatomic, readonly) NSImageView* nameImage;
@property (nonatomic, readonly) NSImageView* keywordImage;
@property (nonatomic, readonly) NSImageView* urlImage;
@property (nonatomic, readonly) NSButton* doneButton;
@property (nonatomic, readonly) NSImage* goodImage;
@property (nonatomic, readonly) NSImage* badImage;
@end

@implementation FakeEditSearchEngineController
@synthesize nameField = nameField_;
@synthesize keywordField = keywordField_;
@synthesize urlField = urlField_;
@synthesize nameImage = nameImage_;
@synthesize keywordImage = keywordImage_;
@synthesize urlImage = urlImage_;
@synthesize doneButton = doneButton_;
- (NSImage*)goodImage {
  return goodImage_.get();
}
- (NSImage*)badImage {
  return badImage_.get();
}
@end

namespace {

class EditSearchEngineControllerTest : public CocoaTest {
 public:
   virtual void SetUp() {
     CocoaTest::SetUp();
     TestingProfile* profile =
        static_cast<TestingProfile*>(browser_helper_.profile());
     profile->CreateTemplateURLModel();
     controller_ = [[FakeEditSearchEngineController alloc]
                    initWithProfile:profile
                    delegate:nil
                    templateURL:nil];
   }

  virtual void TearDown() {
    // Force the window to load so we hit |-awakeFromNib| to register as the
    // window's delegate so that the controller can clean itself up in
    // |-windowWillClose:|.
    ASSERT_TRUE([controller_ window]);

    [controller_ close];
    CocoaTest::TearDown();
  }

  BrowserTestHelper browser_helper_;
  FakeEditSearchEngineController* controller_;
};

TEST_F(EditSearchEngineControllerTest, ValidImageOriginals) {
  EXPECT_FALSE([controller_ goodImage]);
  EXPECT_FALSE([controller_ badImage]);

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
  TemplateURL url;
  url.set_short_name(ASCIIToUTF16("Foobar"));
  url.set_keyword(ASCIIToUTF16("keyword"));
  std::string urlString = TemplateURLRef::DisplayURLToURLRef(
      ASCIIToUTF16("http://foo-bar.com"));
  url.SetURL(urlString, 0, 1);
  TestingProfile* profile = browser_helper_.profile();
  FakeEditSearchEngineController *controller =
      [[FakeEditSearchEngineController alloc] initWithProfile:profile
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
