// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/debugger.h"
#import "base/mac/foundation_util.h"
#include "base/mac/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#import "chrome/browser/ui/cocoa/tab_contents/sad_tab_controller.h"
#import "chrome/browser/ui/cocoa/tab_contents/sad_tab_view_cocoa.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#import "ui/base/cocoa/controls/hyperlink_text_view.h"

@interface SadTabView (ExposedForTesting)
// Implementation is below.
- (HyperlinkTextView*)helpTextView;
@end

@implementation SadTabView (ExposedForTesting)
- (HyperlinkTextView*)helpTextView {
  NSView* containerView = [[self subviews] lastObject];
  for (NSView* view in [containerView subviews]) {
    if (auto textView = base::mac::ObjCCast<HyperlinkTextView>(view))
      return textView;
  }
  return nil;
}
@end

namespace {

class ContentsDelegate : public content::WebContentsDelegate {
 public:
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params) override {
    opened_url_ = params.url;
    return nullptr;
  }

  const GURL& OpenedURL() { return opened_url_; }

 private:
  GURL opened_url_;
};

class SadTabControllerTest : public ChromeRenderViewHostTestHarness {
 public:
  SadTabControllerTest() : test_window_(nil) {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    web_contents()->SetDelegate(&contents_delegate_);
    // Inherting from ChromeRenderViewHostTestHarness means we can't inherit
    // from from CocoaTest, so do a bootstrap and create test window.
    CocoaTest::BootstrapCocoa();
    test_window_ = [[CocoaTestHelperWindow alloc] init];
    if (base::debug::BeingDebugged()) {
      [test_window_ orderFront:nil];
    } else {
      [test_window_ orderBack:nil];
    }
  }

  void TearDown() override {
    [test_window_ close];
    test_window_ = nil;
    ChromeRenderViewHostTestHarness::TearDown();
  }

  // Creates the controller and adds its view to contents, caller has ownership.
  SadTabController* CreateController() {
    SadTabController* controller =
        [[SadTabController alloc] initWithWebContents:web_contents()];
    EXPECT_TRUE(controller);
    NSView* view = [controller view];
    EXPECT_TRUE(view);
    NSView* contentView = [test_window_ contentView];
    [contentView addSubview:view];

    return controller;
  }

  HyperlinkTextView* GetHelpTextView(SadTabController* controller) {
    SadTabView* view = static_cast<SadTabView*>([controller view]);
    return ([view helpTextView]);
  }

  ContentsDelegate contents_delegate_;
  CocoaTestHelperWindow* test_window_;
};

TEST_F(SadTabControllerTest, ClickOnLink) {
  base::scoped_nsobject<SadTabController> controller(CreateController());
  HyperlinkTextView* help = GetHelpTextView(controller);
  EXPECT_TRUE(help);
  EXPECT_TRUE(contents_delegate_.OpenedURL().is_empty());
  [help clickedOnLink:@(chrome::kCrashReasonURL) atIndex:0];
  EXPECT_EQ(contents_delegate_.OpenedURL(), GURL(chrome::kCrashReasonURL));
}

}  // namespace
