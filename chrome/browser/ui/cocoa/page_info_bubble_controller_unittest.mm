// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_nsobject.h"
#include "base/string_util.h"
#include "base/string_number_conversions.h"
#include "base/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/page_info_model.h"
#import "chrome/browser/ui/cocoa/hyperlink_button_cell.h"
#import "chrome/browser/ui/cocoa/page_info_bubble_controller.h"
#include "chrome/browser/ui/cocoa/browser_test_helper.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

class FakeModel : public PageInfoModel {
 public:
  FakeModel() : PageInfoModel() {}

  void AddSection(SectionStateIcon icon_id,
                  const string16& headline,
                  const string16& description,
                  SectionInfoType type) {
    sections_.push_back(SectionInfo(
        icon_id, headline, description, type));
  }
};

class FakeBridge : public PageInfoModel::PageInfoModelObserver {
 public:
  void ModelChanged() {}
};

class PageInfoBubbleControllerTest : public CocoaTest {
 public:
  PageInfoBubbleControllerTest() {
    controller_ = nil;
    model_ = new FakeModel();
  }

  virtual void TearDown() {
    [controller_ close];
    CocoaTest::TearDown();
  }

  void CreateBubble() {
    // The controller cleans up after itself when the window closes.
    controller_ =
        [[PageInfoBubbleController alloc] initWithPageInfoModel:model_
                                                  modelObserver:NULL
                                                   parentWindow:test_window()];
    window_ = [controller_ window];
    [controller_ showWindow:nil];
  }

  // Checks the controller's window for the requisite subviews in the given
  // numbers.
  void CheckWindow(int text_count,
                   int image_count,
                   int spacer_count,
                   int button_count) {
    // All windows have the help center link and a spacer for it.
    int link_count = 1;
    ++spacer_count;

    // The window's only immediate child is an invisible view that has a flipped
    // coordinate origin. It is into this that all views get placed.
    NSArray* windowSubviews = [[window_ contentView] subviews];
    EXPECT_EQ(1U, [windowSubviews count]);
    NSArray* subviews = [[windowSubviews lastObject] subviews];

    for (NSView* view in subviews) {
      if ([view isKindOfClass:[NSTextField class]]) {
        --text_count;
      } else if ([view isKindOfClass:[NSImageView class]]) {
        --image_count;
      } else if ([view isKindOfClass:[NSBox class]]) {
        --spacer_count;
      } else if ([view isKindOfClass:[NSButton class]]) {
        NSButton* button = static_cast<NSButton*>(view);
        // Every window should have a single link button to the help page.
        if ([[button cell] isKindOfClass:[HyperlinkButtonCell class]]) {
          --link_count;
          CheckButton(button, @selector(showHelpPage:));
        } else {
          --button_count;
          CheckButton(button, @selector(showCertWindow:));
        }
      } else {
        ADD_FAILURE() << "Unknown subview: " << [[view description] UTF8String];
      }
    }
    EXPECT_EQ(0, text_count);
    EXPECT_EQ(0, image_count);
    EXPECT_EQ(0, spacer_count);
    EXPECT_EQ(0, button_count);
    EXPECT_EQ(0, link_count);
    EXPECT_EQ([window_ delegate], controller_);
  }

  // Checks that a button is hooked up correctly.
  void CheckButton(NSButton* button, SEL action) {
    EXPECT_EQ(action, [button action]);
    EXPECT_EQ(controller_, [button target]);
    EXPECT_TRUE([button stringValue]);
  }

  BrowserTestHelper helper_;

  PageInfoBubbleController* controller_;  // Weak, owns self.
  FakeModel* model_;  // Weak, owned by controller.
  NSWindow* window_;  // Weak, owned by controller.
};


TEST_F(PageInfoBubbleControllerTest, NoHistoryNoSecurity) {
  model_->AddSection(PageInfoModel::ICON_STATE_ERROR,
      string16(),
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_SECURITY_TAB_UNKNOWN_PARTY),
      PageInfoModel::SECTION_INFO_IDENTITY);
  model_->AddSection(PageInfoModel::ICON_STATE_ERROR,
      string16(),
      l10n_util::GetStringFUTF16(
          IDS_PAGE_INFO_SECURITY_TAB_NOT_ENCRYPTED_CONNECTION_TEXT,
          ASCIIToUTF16("google.com")),
      PageInfoModel::SECTION_INFO_CONNECTION);

  CreateBubble();
  CheckWindow(/*text=*/2, /*image=*/2, /*spacer=*/1, /*button=*/0);
}


TEST_F(PageInfoBubbleControllerTest, HistoryNoSecurity) {
  model_->AddSection(PageInfoModel::ICON_STATE_ERROR,
      string16(),
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_SECURITY_TAB_UNKNOWN_PARTY),
      PageInfoModel::SECTION_INFO_IDENTITY);
  model_->AddSection(PageInfoModel::ICON_STATE_ERROR,
      string16(),
      l10n_util::GetStringFUTF16(
          IDS_PAGE_INFO_SECURITY_TAB_NOT_ENCRYPTED_CONNECTION_TEXT,
          ASCIIToUTF16("google.com")),
      PageInfoModel::SECTION_INFO_CONNECTION);

  // In practice, the history information comes later because it's queried
  // asynchronously, so replicate the double-build here.
  CreateBubble();

  model_->AddSection(PageInfoModel::ICON_STATE_ERROR,
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_SITE_INFO_TITLE),
      l10n_util::GetStringUTF16(
          IDS_PAGE_INFO_SECURITY_TAB_FIRST_VISITED_TODAY),
      PageInfoModel::SECTION_INFO_FIRST_VISIT);

  [controller_ performLayout];

  CheckWindow(/*text=*/4, /*image=*/3, /*spacer=*/2, /*button=*/0);
}


TEST_F(PageInfoBubbleControllerTest, NoHistoryMixedSecurity) {
  model_->AddSection(PageInfoModel::ICON_STATE_OK,
      string16(),
      l10n_util::GetStringFUTF16(
          IDS_PAGE_INFO_SECURITY_TAB_SECURE_IDENTITY,
          ASCIIToUTF16("Goat Security Systems")),
      PageInfoModel::SECTION_INFO_IDENTITY);

  // This string is super long and the text should overflow the default clip
  // region (kImageSize).
  string16 description = l10n_util::GetStringFUTF16(
      IDS_PAGE_INFO_SECURITY_TAB_ENCRYPTED_SENTENCE_LINK,
      l10n_util::GetStringFUTF16(
          IDS_PAGE_INFO_SECURITY_TAB_ENCRYPTED_CONNECTION_TEXT,
          ASCIIToUTF16("chrome.google.com"),
          base::IntToString16(1024)),
      l10n_util::GetStringUTF16(
          IDS_PAGE_INFO_SECURITY_TAB_ENCRYPTED_INSECURE_CONTENT_WARNING));

  model_->AddSection(PageInfoModel::ICON_STATE_OK,
      string16(),
      description,
      PageInfoModel::SECTION_INFO_CONNECTION);

  CreateBubble();
  [controller_ setCertID:1];
  [controller_ performLayout];

  CheckWindow(/*text=*/2, /*image=*/2, /*spacer=*/1, /*button=*/1);

  // Look for the over-sized box.
  NSString* targetDesc = base::SysUTF16ToNSString(description);
  NSArray* subviews = [[window_ contentView] subviews];
  for (NSView* subview in subviews) {
    if ([subview isKindOfClass:[NSTextField class]]) {
      NSTextField* desc = static_cast<NSTextField*>(subview);
      if ([[desc stringValue] isEqualToString:targetDesc]) {
        // Typical box frame is ~55px, make sure this is extra large.
        EXPECT_LT(75, NSHeight([desc frame]));
      }
    }
  }
}

}  // namespace
