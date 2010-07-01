// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#import "base/scoped_nsobject.h"
#import "base/string_util.h"
#import "chrome/app/chrome_dll_resource.h"  // For translate menu command ids.
#import "chrome/browser/cocoa/browser_test_helper.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#import "chrome/browser/cocoa/infobar.h"
#import "chrome/browser/cocoa/translate/translate_infobar_base.h"
#import "chrome/browser/cocoa/translate/before_translate_infobar_controller.h"
#import "chrome/browser/cocoa/translate/after_translate_infobar_controller.h"
#import "chrome/browser/cocoa/translate/translate_message_infobar_controller.h"
#import "chrome/browser/renderer_host/site_instance.h"
#import "chrome/browser/tab_contents/tab_contents.h"
#import "chrome/browser/translate/translate_infobar_delegate.h"
#import "ipc/ipc_channel.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

// All states the translate toolbar can assume.
TranslateInfoBarDelegate::Type kTranslateToolbarStates[] = {
  TranslateInfoBarDelegate::kBeforeTranslate,
  TranslateInfoBarDelegate::kAfterTranslate,
  TranslateInfoBarDelegate::kTranslating,
  TranslateInfoBarDelegate::kTranslationError
};

class MockTranslateInfoBarDelegate : public TranslateInfoBarDelegate {
 public:
  MockTranslateInfoBarDelegate(TranslateInfoBarDelegate::Type type,
                               TranslateErrors::Type error,
                               TabContents* contents)
      : TranslateInfoBarDelegate(type, error, contents, "en", "es"){
    // Start out in the "Before Translate" state.
    type_ = type;

  }

  virtual string16 GetDisplayNameForLocale(const std::string& language_code) {
    return ASCIIToUTF16("Foo");
  }

  virtual bool IsLanguageBlacklisted() {
    return false;
  }

  virtual bool IsSiteBlacklisted() {
    return false;
  }

  virtual bool ShouldAlwaysTranslate() {
    return false;
  }

  MOCK_METHOD0(Translate, void());
  MOCK_METHOD0(RevertTranslation, void());
  MOCK_METHOD0(TranslationDeclined, void());
  MOCK_METHOD0(ToggleLanguageBlacklist, void());
  MOCK_METHOD0(ToggleSiteBlacklist, void());
  MOCK_METHOD0(ToggleAlwaysTranslate, void());
};

class TranslationInfoBarTest : public CocoaTest {
 public:
  scoped_ptr<MockTranslateInfoBarDelegate> infobar_delegate;
  scoped_nsobject<TranslateInfoBarControllerBase> infobar_controller;
  BrowserTestHelper browser_helper_;

 public:
  // Each test gets a single Mock translate delegate for the lifetime of
  // the test.
  virtual void SetUp() {
    CocoaTest::SetUp();
    CreateInfoBar();
  }

  void CreateInfoBar() {
    CreateInfoBar(TranslateInfoBarDelegate::kBeforeTranslate);
  }

  void CreateInfoBar(TranslateInfoBarDelegate::Type type) {
    SiteInstance* instance =
        SiteInstance::CreateSiteInstance(browser_helper_.profile());
    scoped_ptr<TabContents> tab_contents(
        new TabContents(browser_helper_.profile(),
                        instance,
                        MSG_ROUTING_NONE,
                        NULL));
    TranslateErrors::Type error = TranslateErrors::NONE;
    if (type == TranslateInfoBarDelegate::kTranslationError)
      error = TranslateErrors::NETWORK;
    infobar_delegate.reset(
        new MockTranslateInfoBarDelegate(type, error, tab_contents.get()));
    [[infobar_controller view] removeFromSuperview];
    scoped_ptr<InfoBar> infobar(infobar_delegate->CreateInfoBar());
    infobar_controller.reset(
        reinterpret_cast<TranslateInfoBarControllerBase*>(
            infobar->controller()));
    // Need to call this to get the view to load from nib.
    [[test_window() contentView] addSubview:[infobar_controller view]];
  }
};

// Check that we can instantiate a Translate Infobar correctly.
TEST_F(TranslationInfoBarTest, Instantiate) {
  CreateInfoBar();
  ASSERT_TRUE(infobar_controller.get());
}

// Check that clicking the Translate button calls Translate().
TEST_F(TranslationInfoBarTest, TranslateCalledOnButtonPress) {
  CreateInfoBar();

  EXPECT_CALL(*infobar_delegate, Translate()).Times(1);
  [infobar_controller ok:nil];
}

// Check that clicking the "Retry" button calls Translate() when we're
// in the error mode - http://crbug.com/41315 .
TEST_F(TranslationInfoBarTest, TranslateCalledInErrorMode) {
  CreateInfoBar(TranslateInfoBarDelegate::kTranslationError);

  EXPECT_CALL(*infobar_delegate, Translate()).Times(1);

  [infobar_controller ok:nil];
}

// Check that clicking the "Show Original button calls RevertTranslation().
TEST_F(TranslationInfoBarTest, RevertCalledOnButtonPress) {
  CreateInfoBar();

  EXPECT_CALL(*infobar_delegate, RevertTranslation()).Times(1);
  [infobar_controller showOriginal:nil];
}

// Check that items in the options menu are hooked up correctly.
TEST_F(TranslationInfoBarTest, OptionsMenuItemsHookedUp) {
  EXPECT_CALL(*infobar_delegate, Translate())
    .Times(0);

  [infobar_controller rebuildOptionsMenu];
  NSMenu* optionsMenu = [infobar_controller optionsMenu];
  NSArray* optionsMenuItems = [optionsMenu itemArray];

  EXPECT_EQ([optionsMenuItems count], 5U);

  // First item is the options menu button's title, so there's no need to test
  // that the target on that is setup correctly.
  for (NSUInteger i = 1; i < [optionsMenuItems count]; ++i) {
    NSMenuItem* item = [optionsMenuItems objectAtIndex:i];
    EXPECT_EQ([item target], infobar_controller.get());
  }
  NSMenuItem* alwaysTranslateLanguateItem = [optionsMenuItems objectAtIndex:1];
  NSMenuItem* neverTranslateLanguateItem = [optionsMenuItems objectAtIndex:2];
  NSMenuItem* neverTranslateSiteItem = [optionsMenuItems objectAtIndex:3];
  NSMenuItem* aboutTranslateItem = [optionsMenuItems objectAtIndex:4];

  {
    EXPECT_CALL(*infobar_delegate, ToggleAlwaysTranslate())
    .Times(1);
    [infobar_controller optionsMenuChanged:alwaysTranslateLanguateItem];
  }

  {
    EXPECT_CALL(*infobar_delegate, ToggleLanguageBlacklist())
    .Times(1);
    [infobar_controller optionsMenuChanged:neverTranslateLanguateItem];
  }

  {
    EXPECT_CALL(*infobar_delegate, ToggleSiteBlacklist())
    .Times(1);
    [infobar_controller optionsMenuChanged:neverTranslateSiteItem];
  }

  {
    // Can't mock this effectively, so just check that the tag is set correctly.
    EXPECT_EQ([aboutTranslateItem tag], IDC_TRANSLATE_OPTIONS_ABOUT);
  }
}

// Check that selecting a new item from the "Source Language" popup in "before
// translate" mode doesn't trigger a translation or change state.
// http://crbug.com/36666
TEST_F(TranslationInfoBarTest, Bug36666) {
  EXPECT_CALL(*infobar_delegate, Translate())
    .Times(0);

  CreateInfoBar();
  int arbitrary_index = 2;
  [infobar_controller sourceLanguageModified:arbitrary_index];
  EXPECT_CALL(*infobar_delegate, Translate())
    .Times(0);
}

// Check that the infobar lays itself out correctly when instantiated in
// each of the states.
// http://crbug.com/36895
TEST_F(TranslationInfoBarTest, Bug36895) {
  EXPECT_CALL(*infobar_delegate, Translate())
    .Times(0);

  for (size_t i = 0; i < arraysize(kTranslateToolbarStates); ++i) {
    CreateInfoBar(kTranslateToolbarStates[i]);
    EXPECT_TRUE(
        [infobar_controller verifyLayout]) << "Layout wrong, for state #" << i;
  }
}

}  // namespace
