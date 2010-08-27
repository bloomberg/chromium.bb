// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/l10n_util.h"
#include "base/scoped_nsobject.h"
#include "base/string_util.h"
#include "base/string_number_conversions.h"
#include "base/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#import "chrome/browser/cocoa/page_info_window_mac.h"
#import "chrome/browser/cocoa/page_info_window_controller.h"
#include "chrome/browser/cocoa/browser_test_helper.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#include "chrome/browser/page_info_model.h"
#include "grit/generated_resources.h"

namespace {

class FakeModel : public PageInfoModel {
 public:
  void AddSection(SectionInfoState state,
                  const string16& title,
                  const string16& description,
                  SectionInfoType type) {
    sections_.push_back(SectionInfo(
        state,
        title,
        string16(),
        description,
        type));
  }
};

class PageInfoWindowMacTest : public CocoaTest {
 public:
  virtual void SetUp() {
    CocoaTest::SetUp();

    // The controller cleans up after itself when the window closes.
    controller_ = [[PageInfoWindowController alloc] init];
    window_ = [controller_ window];

    // The bridge will own the model.
    model_ = new FakeModel();

    // The controller will take ownership of the bridge.
    bridge_ = new PageInfoWindowMac(controller_, model_);
    [controller_ setPageInfo:bridge_];
    EXPECT_TRUE([controller_ window]);
  }

  virtual void TearDown() {
    [controller_ close];
    CocoaTest::TearDown();
  }

  // Checks the controller's window for the requisite subviews in the given
  // numbers.
  void CheckWindow(int button_count, int box_count) {
    for (NSView* view in [[window_ contentView] subviews]) {
      if ([view isKindOfClass:[NSButton class]]) {
        --button_count;
        CheckButton(static_cast<NSButton*>(view));
      } else if ([view isKindOfClass:[NSBox class]]) {
        --box_count;
        CheckBox(static_cast<NSBox*>(view));
      } else {
        EXPECT_TRUE(false) << "Unknown subview";
      }
    }
    EXPECT_EQ(0, button_count);
    EXPECT_EQ(0, box_count);
    EXPECT_EQ([window_ delegate], controller_);
  }

  // Checks that a button is hooked up correctly.
  void CheckButton(NSButton* button) {
    EXPECT_EQ(@selector(showCertWindow:), [button action]);
    EXPECT_EQ(controller_, [button target]);
    EXPECT_TRUE([button stringValue]);
  }

  // Makes sure the box has a valid image and a string.
  void CheckBox(NSBox* box) {
    EXPECT_TRUE([box title]);
    NSArray* subviews = [[box contentView] subviews];
    EXPECT_EQ(2U, [subviews count]);
    for (NSView* view in subviews) {
      if ([view isKindOfClass:[NSImageView class]]) {
        NSImageView* image_view = static_cast<NSImageView*>(view);
        EXPECT_TRUE([image_view image] == bridge_->good_image_.get() ||
                    [image_view image] == bridge_->bad_image_.get());
      } else if ([view isKindOfClass:[NSTextField class]]) {
        NSTextField* text_field = static_cast<NSTextField*>(view);
        EXPECT_LT(0U, [[text_field stringValue] length]);
      } else {
        EXPECT_TRUE(false) << "Unknown box subview";
      }
    }
  }

  BrowserTestHelper helper_;

  PageInfoWindowController* controller_;  // Weak, owns self.
  PageInfoWindowMac* bridge_;  // Weak, owned by controller.
  FakeModel* model_;  // Weak, owned by bridge.

  NSWindow* window_;  // Weak, owned by controller.
};


TEST_F(PageInfoWindowMacTest, NoHistoryNoSecurity) {
  model_->AddSection(PageInfoModel::SECTION_STATE_ERROR,
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_SECURITY_TAB_IDENTITY_TITLE),
      l10n_util::GetStringFUTF16(
          IDS_PAGE_INFO_SECURITY_TAB_UNKNOWN_PARTY,
          ASCIIToUTF16("google.com")),
      PageInfoModel::SECTION_INFO_IDENTITY);
  model_->AddSection(PageInfoModel::SECTION_STATE_ERROR,
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_SECURITY_TAB_CONNECTION_TITLE),
      l10n_util::GetStringFUTF16(
          IDS_PAGE_INFO_SECURITY_TAB_NOT_ENCRYPTED_CONNECTION_TEXT,
          ASCIIToUTF16("google.com")),
      PageInfoModel::SECTION_INFO_CONNECTION);

  bridge_->ModelChanged();

  CheckWindow(1, 2);
}


TEST_F(PageInfoWindowMacTest, HistoryNoSecurity) {
  model_->AddSection(PageInfoModel::SECTION_STATE_ERROR,
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_SECURITY_TAB_IDENTITY_TITLE),
      l10n_util::GetStringFUTF16(
          IDS_PAGE_INFO_SECURITY_TAB_UNKNOWN_PARTY,
          ASCIIToUTF16("google.com")),
      PageInfoModel::SECTION_INFO_IDENTITY);
  model_->AddSection(PageInfoModel::SECTION_STATE_ERROR,
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_SECURITY_TAB_CONNECTION_TITLE),
      l10n_util::GetStringFUTF16(
          IDS_PAGE_INFO_SECURITY_TAB_NOT_ENCRYPTED_CONNECTION_TEXT,
          ASCIIToUTF16("google.com")),
      PageInfoModel::SECTION_INFO_CONNECTION);

  // In practice, the history information comes later because it's queried
  // asynchronously, so replicate the double-build here.
  bridge_->ModelChanged();

  model_->AddSection(PageInfoModel::SECTION_STATE_ERROR,
      l10n_util::GetStringUTF16(
          IDS_PAGE_INFO_SECURITY_TAB_PERSONAL_HISTORY_TITLE),
      l10n_util::GetStringUTF16(
          IDS_PAGE_INFO_SECURITY_TAB_FIRST_VISITED_TODAY),
      PageInfoModel::SECTION_INFO_FIRST_VISIT);

  bridge_->ModelChanged();

  CheckWindow(1, 3);
}


TEST_F(PageInfoWindowMacTest, NoHistoryMixedSecurity) {
  model_->AddSection(PageInfoModel::SECTION_STATE_OK,
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_SECURITY_TAB_IDENTITY_TITLE),
      l10n_util::GetStringFUTF16(
          IDS_PAGE_INFO_SECURITY_TAB_SECURE_IDENTITY,
          ASCIIToUTF16("Goat Security Systems")),
      PageInfoModel::SECTION_INFO_IDENTITY);

  // This string is super long and the text should overflow the default clip
  // region (kImageSize).
  string16 title =
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_SECURITY_TAB_CONNECTION_TITLE);
  model_->AddSection(PageInfoModel::SECTION_STATE_OK,
      title,
      l10n_util::GetStringFUTF16(
          IDS_PAGE_INFO_SECURITY_TAB_ENCRYPTED_SENTENCE_LINK,
          l10n_util::GetStringFUTF16(
              IDS_PAGE_INFO_SECURITY_TAB_ENCRYPTED_CONNECTION_TEXT,
              ASCIIToUTF16("chrome.google.com"),
              base::IntToString16(1024)),
          l10n_util::GetStringUTF16(
              IDS_PAGE_INFO_SECURITY_TAB_ENCRYPTED_INSECURE_CONTENT_WARNING)),
      PageInfoModel::SECTION_INFO_CONNECTION);

  bridge_->ModelChanged();

  NSArray* subviews = [[window_ contentView] subviews];
  CheckWindow(1, 2);

  // Look for the over-sized box.
  NSString* targetTitle = base::SysUTF16ToNSString(title);
  for (NSView* subview in subviews) {
    if ([subview isKindOfClass:[NSBox class]]) {
      NSBox* box = static_cast<NSBox*>(subview);
      if ([[box title] isEqualToString:targetTitle]) {
        // Typical box frame is ~55px, make sure this is extra large.
        EXPECT_LT(75, NSHeight([box frame]));
      }
    }
  }
}

}  // namespace
