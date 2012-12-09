// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/cocoa/constrained_window/constrained_window_sheet_controller.h"
#import "chrome/browser/ui/cocoa/intents/web_intent_picker_cocoa.h"
#import "chrome/browser/ui/cocoa/intents/web_intent_inline_service_view_controller.h"
#include "chrome/browser/ui/intents/web_intent_picker_delegate_mock.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/web_contents.h"
#import "testing/gtest_mac.h"

namespace {

NSString* const kTitle = @"A quick brown fox jumps over the lazy dog.";

}  // namespace

class WebIntentInlineServiceViewControllerTest : public InProcessBrowserTest {
 public:
  WebIntentInlineServiceViewControllerTest() : picker_(NULL) {
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    content::WebContents* tab =
        browser()->tab_strip_model()->GetWebContentsAt(0);
    picker_ = new WebIntentPickerCocoa(tab, &delegate_, &model_);
    view_controller_.reset(
        [[WebIntentInlineServiceViewController alloc] initWithPicker:picker_]);
    view_.reset([[view_controller_ view] retain]);

    NSWindow* sheet = picker_->constrained_window()->GetNativeWindow();
    [[sheet contentView] addSubview:view_];
  }

  virtual void CleanUpOnMainThread() OVERRIDE {
    EXPECT_CALL(delegate_, OnClosing());
    picker_->Close();
    view_controller_.reset();
  }

 protected:
  scoped_nsobject<NSView> view_;
  scoped_nsobject<WebIntentInlineServiceViewController> view_controller_;
  WebIntentPickerDelegateMock delegate_;
  WebIntentPickerModel model_;
  WebIntentPickerCocoa* picker_;
};

IN_PROC_BROWSER_TEST_F(WebIntentInlineServiceViewControllerTest, Layout) {
  const CGFloat margin = 10;
  NSRect inner_frame = NSMakeRect(margin, margin, 200, 50);
  EXPECT_TRUE([view_controller_ chooseServiceButton]);

  // Layout empty view.
  NSSize empty_size =
      [view_controller_ minimumSizeForInnerWidth:NSWidth(inner_frame)];
  EXPECT_LE(empty_size.width, NSWidth(inner_frame));
  [view_ setFrame:NSInsetRect(inner_frame, -margin, -margin)];
  [view_controller_ layoutSubviewsWithinFrame:inner_frame];

  // Layout with a long string that wraps.
  [view_controller_ setServiceName:kTitle];
  [view_controller_ setServiceIcon:[NSImage imageNamed:NSImageNameFolder]];
  NSSize new_size =
      [view_controller_ minimumSizeForInnerWidth:NSWidth(inner_frame)];
  EXPECT_GE(new_size.width, empty_size.width);
  EXPECT_GT(new_size.height, empty_size.height);
  EXPECT_EQ(NSWidth(inner_frame), new_size.width);
  inner_frame.size.height = new_size.height;
  [view_ setFrame:NSInsetRect(inner_frame, -margin, -margin)];
  [view_controller_ layoutSubviewsWithinFrame:inner_frame];

  // Verify that all controls are inside the inner frame.
  for (NSView* child in [view_ subviews]) {
    EXPECT_TRUE([child isMemberOfClass:[NSBox class]] ||
                NSContainsRect(inner_frame, [child frame]) ||
                NSIsEmptyRect([child frame]));
  }
}

IN_PROC_BROWSER_TEST_F(WebIntentInlineServiceViewControllerTest, WebView) {
  const CGFloat margin = 10;
  NSRect inner_frame = NSMakeRect(margin, margin, 200, 100);

  [view_controller_ setServiceName:kTitle];

  GURL url("about:blank");
  content::WebContents::CreateParams create_params(
      browser()->profile(),
      tab_util::GetSiteInstanceForNewTab(browser()->profile(), url));
  content::WebContents* web_contents = content::WebContents::Create(
      create_params);

  // Create a web view
  EXPECT_CALL(delegate_,
              CreateWebContentsForInlineDisposition(testing::_, testing::_))
      .WillOnce(testing::Return(web_contents));
  [view_controller_ setServiceURL:url];
  ASSERT_TRUE([view_controller_ webContents]);
  EXPECT_NSEQ(view_, [[[view_controller_ webContents]->GetNativeView()
      superview] superview]);
  NSSize minWebViewSize =
      [view_controller_ minimumInlineWebViewSizeForFrame:inner_frame];
  EXPECT_GT(minWebViewSize.width, 0);
  EXPECT_GT(minWebViewSize.height, 0);

  // Layout
  [view_controller_ minimumSizeForInnerWidth:NSWidth(inner_frame)];
  [view_ setFrame:NSInsetRect(inner_frame, -margin, -margin)];
  [view_controller_ layoutSubviewsWithinFrame:inner_frame];

  // Verify that all controls are inside the inner frame.
  for (NSView* child in [view_ subviews]) {
    EXPECT_TRUE([child isMemberOfClass:[NSBox class]] ||
                NSContainsRect(inner_frame, [child frame]));
  }
}

IN_PROC_BROWSER_TEST_F(WebIntentInlineServiceViewControllerTest, UnsetWebView) {
  ASSERT_FALSE([view_controller_ webContents]);

  GURL url("about:blank");
  content::WebContents::CreateParams create_params(
      browser()->profile(),
      tab_util::GetSiteInstanceForNewTab(browser()->profile(), url));
  content::WebContents* web_contents = content::WebContents::Create(
      create_params);
  EXPECT_CALL(delegate_,
              CreateWebContentsForInlineDisposition(testing::_, testing::_))
      .WillOnce(testing::Return(web_contents));

  [view_controller_ setServiceURL:url];
  ASSERT_TRUE([view_controller_ webContents]);
  [view_controller_ setServiceURL:GURL::EmptyGURL()];
  ASSERT_FALSE([view_controller_ webContents]);
}
