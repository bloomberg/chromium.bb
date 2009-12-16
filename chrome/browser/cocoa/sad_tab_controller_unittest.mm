// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_nsobject.h"
#include "chrome/browser/cocoa/browser_test_helper.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#import "chrome/browser/cocoa/sad_tab_controller.h"
#import "chrome/browser/cocoa/sad_tab_view.h"
#include "chrome/browser/renderer_host/site_instance.h"
#include "chrome/browser/tab_contents/tab_contents.h"

@interface SadTabView (ExposedForTesting)
// Implementation is below.
- (NSButton*)linkButton;
@end

@implementation SadTabView (ExposedForTesting)
- (NSButton*)linkButton {
  return linkButton_;
}
@end

namespace {

class SadTabControllerTest : public CocoaTest {
 public:
  SadTabControllerTest() {
    link_clicked_ = false;
  }

  TabContents* CreateTabContents() {
    SiteInstance* instance =
        SiteInstance::CreateSiteInstance(browser_helper_.profile());
    TabContents* tab_contents = new TabContents(browser_helper_.profile(),
        instance, MSG_ROUTING_NONE, NULL);
    return tab_contents;
  }

  // Creates the controller and adds its view to contents, caller has ownership.
  SadTabController* CreateController(TabContents* tab_contents) {
    NSView* contentView = [test_window() contentView];
    SadTabController* controller =
        [[SadTabController alloc] initWithTabContents:tab_contents
                                            superview:contentView];
    EXPECT_TRUE(controller);
    NSView* view = [controller view];
    EXPECT_TRUE(view);

    return controller;
  }

  NSButton* GetLinkButton(SadTabController* controller) {
    SadTabView* view = static_cast<SadTabView*>([controller view]);
    return ([view linkButton]);
  }

  BrowserTestHelper browser_helper_;
  static bool link_clicked_;
};

/* TODO(kuan): (BUG:30522) enable this test when leak is fixed
TEST_F(SadTabControllerTest, TestWithTabContents) {
  scoped_ptr<TabContents> tab_contents(CreateTabContents());
  scoped_nsobject<SadTabController>
      controller(CreateController(tab_contents.get()));
  EXPECT_TRUE(controller);
  NSButton* link = GetLinkButton(controller);
  EXPECT_TRUE(link);
}
*/

TEST_F(SadTabControllerTest, TestWithoutTabContents) {
  scoped_nsobject<SadTabController> controller(CreateController(NULL));
  EXPECT_TRUE(controller);
  NSButton* link = GetLinkButton(controller);
  EXPECT_FALSE(link);
}

/* TODO(kuan): (BUG:30522) enable this when leak is fixed
TEST_F(SadTabControllerTest, TestClickOnLink) {
  scoped_ptr<TabContents> tab_contents(CreateTabContents());
  scoped_nsobject<SadTabController>
      controller(CreateController(tab_contents.get()));
  NSButton* link = GetLinkButton(controller);
  EXPECT_TRUE(link);
  EXPECT_FALSE(link_clicked_);
  [link performClick:link];
  EXPECT_TRUE(link_clicked_);
}
*/

}  // namespace

@implementation NSApplication (SadTabControllerUnitTest)
// Add handler for the openLearnMoreAboutCrashLink: action to NSApp for testing
// purposes. Normally this would be sent up the responder tree correctly, but
// since tests run in the background, key window and main window are never set
// on NSApplication. Adding it to NSApplication directly removes the need for
// worrying about what the current window with focus is.
- (void)openLearnMoreAboutCrashLink:(id)sender {
  SadTabControllerTest::link_clicked_ = true;
}

@end
