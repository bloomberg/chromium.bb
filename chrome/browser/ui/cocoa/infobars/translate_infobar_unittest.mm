// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#import "base/memory/scoped_nsobject.h"
#import "base/string_util.h"
#include "base/utf_string_conversions.h"
#import "chrome/app/chrome_command_ids.h"  // For translate menu command ids.
#include "chrome/browser/api/infobars/infobar_service.h"
#import "chrome/browser/translate/translate_infobar_delegate.h"
#include "chrome/browser/ui/cocoa/cocoa_profile_test.h"
#import "chrome/browser/ui/cocoa/infobars/before_translate_infobar_controller.h"
#import "chrome/browser/ui/cocoa/infobars/infobar.h"
#import "chrome/browser/ui/cocoa/infobars/translate_infobar_base.h"
#import "content/public/browser/web_contents.h"
#include "ipc/ipc_message.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

using content::WebContents;

namespace {

// All states the translate toolbar can assume.
TranslateInfoBarDelegate::Type kTranslateToolbarStates[] = {
  TranslateInfoBarDelegate::BEFORE_TRANSLATE,
  TranslateInfoBarDelegate::AFTER_TRANSLATE,
  TranslateInfoBarDelegate::TRANSLATING,
  TranslateInfoBarDelegate::TRANSLATION_ERROR
};

class MockTranslateInfoBarDelegate : public TranslateInfoBarDelegate {
 public:
  MockTranslateInfoBarDelegate(TranslateInfoBarDelegate::Type type,
                               TranslateErrors::Type error,
                               InfoBarService* infobar_service,
                               PrefService* prefs)
      : TranslateInfoBarDelegate(type, error, infobar_service, prefs,
                                 "en", "es") {
  }

  MOCK_METHOD0(Translate, void());
  MOCK_METHOD0(RevertTranslation, void());

  MOCK_METHOD0(TranslationDeclined, void());

  virtual bool IsLanguageBlacklisted() OVERRIDE { return false; }
  MOCK_METHOD0(ToggleLanguageBlacklist, void());
  virtual bool IsSiteBlacklisted() OVERRIDE { return false; }
  MOCK_METHOD0(ToggleSiteBlacklist, void());
  virtual bool ShouldAlwaysTranslate() OVERRIDE { return false; }
  MOCK_METHOD0(ToggleAlwaysTranslate, void());
};

class TranslationInfoBarTest : public CocoaProfileTest {
 public:
  // Each test gets a single Mock translate delegate for the lifetime of
  // the test.
  virtual void SetUp() {
    CocoaProfileTest::SetUp();
    web_contents_.reset(
        WebContents::Create(WebContents::CreateParams(profile())));
    InfoBarService::CreateForWebContents(web_contents_.get());
    CreateInfoBar();
  }

  void CreateInfoBar() {
    CreateInfoBar(TranslateInfoBarDelegate::BEFORE_TRANSLATE);
  }

  void CreateInfoBar(TranslateInfoBarDelegate::Type type) {
    TranslateErrors::Type error = TranslateErrors::NONE;
    if (type == TranslateInfoBarDelegate::TRANSLATION_ERROR)
      error = TranslateErrors::NETWORK;
    InfoBarService* infobar_service =
        InfoBarService::FromWebContents(web_contents_.get());
    Profile* profile =
        Profile::FromBrowserContext(web_contents_->GetBrowserContext());
    infobar_delegate_.reset(new MockTranslateInfoBarDelegate(
        type,
        error,
        infobar_service,
        profile->GetPrefs()));
    [[infobar_controller_ view] removeFromSuperview];
    scoped_ptr<InfoBar> infobar(
        static_cast<InfoBarDelegate*>(infobar_delegate_.get())->
            CreateInfoBar(infobar_service));
    infobar_controller_.reset(
        reinterpret_cast<TranslateInfoBarControllerBase*>(
            infobar->controller()));
    // We need to set the window to be wide so that the options button
    // doesn't overlap the other buttons.
    [test_window() setContentSize:NSMakeSize(2000, 500)];
    [[infobar_controller_ view] setFrame:NSMakeRect(0, 0, 2000, 500)];
    [[test_window() contentView] addSubview:[infobar_controller_ view]];
  }

  scoped_ptr<WebContents> web_contents_;
  scoped_ptr<MockTranslateInfoBarDelegate> infobar_delegate_;
  scoped_nsobject<TranslateInfoBarControllerBase> infobar_controller_;
};

// Check that we can instantiate a Translate Infobar correctly.
TEST_F(TranslationInfoBarTest, Instantiate) {
  CreateInfoBar();
  ASSERT_TRUE(infobar_controller_.get());
}

// Check that clicking the Translate button calls Translate().
TEST_F(TranslationInfoBarTest, TranslateCalledOnButtonPress) {
  CreateInfoBar();

  EXPECT_CALL(*infobar_delegate_, Translate()).Times(1);
  [infobar_controller_ ok:nil];
}

// Check that clicking the "Retry" button calls Translate() when we're
// in the error mode - http://crbug.com/41315 .
TEST_F(TranslationInfoBarTest, TranslateCalledInErrorMode) {
  CreateInfoBar(TranslateInfoBarDelegate::TRANSLATION_ERROR);

  EXPECT_CALL(*infobar_delegate_, Translate()).Times(1);

  [infobar_controller_ ok:nil];
}

// Check that clicking the "Show Original button calls RevertTranslation().
TEST_F(TranslationInfoBarTest, RevertCalledOnButtonPress) {
  CreateInfoBar();

  EXPECT_CALL(*infobar_delegate_, RevertTranslation()).Times(1);
  [infobar_controller_ showOriginal:nil];
}

// Check that items in the options menu are hooked up correctly.
TEST_F(TranslationInfoBarTest, OptionsMenuItemsHookedUp) {
  EXPECT_CALL(*infobar_delegate_, Translate())
    .Times(0);

  [infobar_controller_ rebuildOptionsMenu:NO];
  NSMenu* optionsMenu = [infobar_controller_ optionsMenu];
  NSArray* optionsMenuItems = [optionsMenu itemArray];

  EXPECT_EQ(7U, [optionsMenuItems count]);

  // First item is the options menu button's title, so there's no need to test
  // that the target on that is setup correctly.
  for (NSUInteger i = 1; i < [optionsMenuItems count]; ++i) {
    NSMenuItem* item = [optionsMenuItems objectAtIndex:i];
    if (![item isSeparatorItem])
      EXPECT_EQ([item target], infobar_controller_.get());
  }
  NSMenuItem* alwaysTranslateLanguateItem = [optionsMenuItems objectAtIndex:1];
  NSMenuItem* neverTranslateLanguateItem = [optionsMenuItems objectAtIndex:2];
  NSMenuItem* neverTranslateSiteItem = [optionsMenuItems objectAtIndex:3];
  // Separator at 4.
  NSMenuItem* reportBadLanguageItem = [optionsMenuItems objectAtIndex:5];
  NSMenuItem* aboutTranslateItem = [optionsMenuItems objectAtIndex:6];

  {
    EXPECT_CALL(*infobar_delegate_, ToggleAlwaysTranslate())
    .Times(1);
    [infobar_controller_ optionsMenuChanged:alwaysTranslateLanguateItem];
  }

  {
    EXPECT_CALL(*infobar_delegate_, ToggleLanguageBlacklist())
    .Times(1);
    [infobar_controller_ optionsMenuChanged:neverTranslateLanguateItem];
  }

  {
    EXPECT_CALL(*infobar_delegate_, ToggleSiteBlacklist())
    .Times(1);
    [infobar_controller_ optionsMenuChanged:neverTranslateSiteItem];
  }

  {
    // Can't mock these effectively, so just check that the tag is set
    // correctly.
    EXPECT_EQ(IDC_TRANSLATE_REPORT_BAD_LANGUAGE_DETECTION,
              [reportBadLanguageItem tag]);
    EXPECT_EQ(IDC_TRANSLATE_OPTIONS_ABOUT, [aboutTranslateItem tag]);
  }
}

// Check that selecting a new item from the "Source Language" popup in "before
// translate" mode doesn't trigger a translation or change state.
// http://crbug.com/36666
TEST_F(TranslationInfoBarTest, Bug36666) {
  EXPECT_CALL(*infobar_delegate_, Translate())
    .Times(0);

  CreateInfoBar();
  int arbitrary_index = 2;
  [infobar_controller_ sourceLanguageModified:arbitrary_index];
  EXPECT_CALL(*infobar_delegate_, Translate())
    .Times(0);
}

// Check that the infobar lays itself out correctly when instantiated in
// each of the states.
// http://crbug.com/36895
TEST_F(TranslationInfoBarTest, Bug36895) {
  EXPECT_CALL(*infobar_delegate_, Translate())
    .Times(0);

  for (size_t i = 0; i < arraysize(kTranslateToolbarStates); ++i) {
    CreateInfoBar(kTranslateToolbarStates[i]);
    EXPECT_TRUE(
        [infobar_controller_ verifyLayout]) << "Layout wrong, for state #" << i;
  }
}

// Verify that the infobar shows the "Always translate this language" button
// after doing 3 translations.
TEST_F(TranslationInfoBarTest, TriggerShowAlwaysTranslateButton) {
  TranslatePrefs translate_prefs(profile()->GetPrefs());
  translate_prefs.ResetTranslationAcceptedCount("en");
  for (int i = 0; i < 4; ++i) {
    translate_prefs.IncrementTranslationAcceptedCount("en");
  }
  CreateInfoBar(TranslateInfoBarDelegate::BEFORE_TRANSLATE);
  BeforeTranslateInfobarController* controller =
      (BeforeTranslateInfobarController*)infobar_controller_.get();
  EXPECT_TRUE([[controller alwaysTranslateButton] superview] !=  nil);
  EXPECT_TRUE([[controller neverTranslateButton] superview] == nil);
}

// Verify that the infobar shows the "Never translate this language" button
// after denying 3 translations.
TEST_F(TranslationInfoBarTest, TriggerShowNeverTranslateButton) {
  TranslatePrefs translate_prefs(profile()->GetPrefs());
  translate_prefs.ResetTranslationDeniedCount("en");
  for (int i = 0; i < 4; ++i) {
    translate_prefs.IncrementTranslationDeniedCount("en");
  }
  CreateInfoBar(TranslateInfoBarDelegate::BEFORE_TRANSLATE);
  BeforeTranslateInfobarController* controller =
      (BeforeTranslateInfobarController*)infobar_controller_.get();
  EXPECT_TRUE([[controller alwaysTranslateButton] superview] == nil);
  EXPECT_TRUE([[controller neverTranslateButton] superview] != nil);
}

} // namespace
