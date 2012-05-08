// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include <vector>

#include "chrome/browser/tabs/test_tab_strip_model_delegate.h"
#import "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/cocoa/cocoa_profile_test.h"
#import "chrome/browser/ui/cocoa/new_tab_button.h"
#import "chrome/browser/ui/cocoa/tabs/tab_strip_controller.h"
#import "chrome/browser/ui/cocoa/tabs/tab_strip_view.h"
#import "chrome/browser/ui/cocoa/tabs/tab_view.h"
#import "chrome/browser/ui/cocoa/tabs/tab_controller.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

using content::SiteInstance;
using content::WebContents;

@interface TestTabStripControllerDelegate
    : NSObject<TabStripControllerDelegate> {
}
@end

@implementation TestTabStripControllerDelegate
- (void)onActivateTabWithContents:(WebContents*)contents {
}
- (void)onReplaceTabWithContents:(WebContents*)contents {
}
- (void)onTabChanged:(TabStripModelObserver::TabChangeType)change
        withContents:(WebContents*)contents {
}
- (void)onTabDetachedWithContents:(WebContents*)contents {
}
@end

@interface TabStripController (Test)

- (void)mouseMoved:(NSEvent*)event;

@end

@implementation TabView (Test)

- (TabController*)getController {
  return controller_;
}

@end

namespace {

class TabStripControllerTest : public CocoaProfileTest {
 public:
  virtual void SetUp() {
    CocoaProfileTest::SetUp();
    ASSERT_TRUE(browser());

    BrowserWindow* browser_window = CreateBrowserWindow();
    NSWindow* window = browser_window->GetNativeHandle();
    NSView* parent = [window contentView];
    NSRect content_frame = [parent frame];

    // Create the "switch view" (view that gets changed out when a tab
    // switches).
    NSRect switch_frame = NSMakeRect(0, 0, content_frame.size.width, 500);
    scoped_nsobject<NSView> switch_view(
        [[NSView alloc] initWithFrame:switch_frame]);
    [parent addSubview:switch_view.get()];

    // Create the tab strip view. It's expected to have a child button in it
    // already as the "new tab" button so create that too.
    NSRect strip_frame = NSMakeRect(0, NSMaxY(switch_frame),
                                    content_frame.size.width, 30);
    tab_strip_.reset(
        [[TabStripView alloc] initWithFrame:strip_frame]);
    [parent addSubview:tab_strip_.get()];
    NSRect button_frame = NSMakeRect(0, 0, 15, 15);
    scoped_nsobject<NewTabButton> new_tab_button(
        [[NewTabButton alloc] initWithFrame:button_frame]);
    [tab_strip_ addSubview:new_tab_button.get()];
    [tab_strip_ setNewTabButton:new_tab_button.get()];

    delegate_.reset(new TestTabStripModelDelegate());
    model_ = browser()->tabstrip_model();
    controller_delegate_.reset([TestTabStripControllerDelegate alloc]);
    controller_.reset([[TabStripController alloc]
                      initWithView:static_cast<TabStripView*>(tab_strip_.get())
                        switchView:switch_view.get()
                           browser:browser()
                          delegate:controller_delegate_.get()]);
  }

  virtual void TearDown() {
    // The call to CocoaTest::TearDown() deletes the Browser and TabStripModel
    // objects, so we first have to delete the controller, which refers to them.
    controller_.reset();
    model_ = NULL;
    CocoaProfileTest::TearDown();
  }

  scoped_ptr<TestTabStripModelDelegate> delegate_;
  TabStripModel* model_;
  scoped_nsobject<TestTabStripControllerDelegate> controller_delegate_;
  scoped_nsobject<TabStripController> controller_;
  scoped_nsobject<TabStripView> tab_strip_;
};

// Test adding and removing tabs and making sure that views get added to
// the tab strip.
TEST_F(TabStripControllerTest, AddRemoveTabs) {
  EXPECT_TRUE(model_->empty());
  SiteInstance* instance = SiteInstance::Create(profile());
  TabContentsWrapper* tab_contents =
      Browser::TabContentsFactory(profile(), instance,
          MSG_ROUTING_NONE, NULL, NULL);
  model_->AppendTabContents(tab_contents, true);
  EXPECT_EQ(model_->count(), 1);
}

TEST_F(TabStripControllerTest, SelectTab) {
  // TODO(pinkerton): Implement http://crbug.com/10899
}

TEST_F(TabStripControllerTest, RearrangeTabs) {
  // TODO(pinkerton): Implement http://crbug.com/10899
}

// Test that changing the number of tabs broadcasts a
// kTabStripNumberOfTabsChanged notifiction.
TEST_F(TabStripControllerTest, Notifications) {
  // TODO(pinkerton): Implement http://crbug.com/10899
}

TEST_F(TabStripControllerTest, CorrectToolTipText) {
  // Create tab 1.
  SiteInstance* instance = SiteInstance::Create(profile());
  TabContentsWrapper* tab_contents =
  Browser::TabContentsFactory(profile(), instance,
                              MSG_ROUTING_NONE, NULL, NULL);
  model_->AppendTabContents(tab_contents, true);

  // Create tab 2.
  SiteInstance* instance2 = SiteInstance::Create(profile());
  TabContentsWrapper* tab_contents2 =
  Browser::TabContentsFactory(profile(), instance2,
                              MSG_ROUTING_NONE, NULL, NULL);
  model_->AppendTabContents(tab_contents2, true);

  // Set tab 1 tooltip.
  TabView* tab1 = (TabView*)[controller_.get() viewAtIndex:0];
  [tab1 setToolTip:@"Tab1"];

  // Set tab 2 tooltip.
  TabView* tab2 = (TabView*)[controller_.get() viewAtIndex:1];
  [tab2 setToolTip:@"Tab2"];

  EXPECT_FALSE([tab1 getController].selected);
  EXPECT_TRUE([tab2 getController].selected);

  // Check that there's no tooltip yet.
  EXPECT_FALSE([controller_ view:nil
                stringForToolTip:nil
                           point:NSMakePoint(0,0)
                        userData:nil]);

  // Set up mouse event on overlap of tab1 + tab2.
  NSWindow* window = test_window();
  NSEvent* current = [NSApp currentEvent];

  NSRect tab_strip_frame = [tab_strip_.get() frame];

  // Hover over overlap between tab 1 and 2.
  NSEvent* event = [NSEvent mouseEventWithType:NSMouseMoved
                                    location:NSMakePoint(
                                        275, NSMinY(tab_strip_frame) + 1)
                               modifierFlags:0
                                   timestamp:[current timestamp]
                                windowNumber:[window windowNumber]
                                     context:nil
                                 eventNumber:0
                                  clickCount:1
                                    pressure:1.0];

  [controller_.get() mouseMoved:event];
  EXPECT_STREQ("Tab2",
      [[controller_ view:nil
        stringForToolTip:nil
                   point:NSMakePoint(0,0)
                userData:nil] cStringUsingEncoding:NSASCIIStringEncoding]);


  // Hover over tab 1.
  event = [NSEvent mouseEventWithType:NSMouseMoved
                                    location:NSMakePoint(260,
                                        NSMinY(tab_strip_frame) + 1)
                               modifierFlags:0
                                   timestamp:[current timestamp]
                                windowNumber:[window windowNumber]
                                     context:nil
                                 eventNumber:0
                                  clickCount:1
                                    pressure:1.0];

  [controller_.get() mouseMoved:event];
  EXPECT_STREQ("Tab1",
      [[controller_ view:nil
        stringForToolTip:nil
                   point:NSMakePoint(0,0)
                userData:nil] cStringUsingEncoding:NSASCIIStringEncoding]);

  // Hover over tab 2.
  event = [NSEvent mouseEventWithType:NSMouseMoved
                                    location:NSMakePoint(290,
                                        NSMinY(tab_strip_frame) + 1)
                               modifierFlags:0
                                   timestamp:[current timestamp]
                                windowNumber:[window windowNumber]
                                     context:nil
                                 eventNumber:0
                                  clickCount:1
                                    pressure:1.0];

  [controller_.get() mouseMoved:event];
  EXPECT_STREQ("Tab2",
      [[controller_ view:nil
        stringForToolTip:nil
                   point:NSMakePoint(0,0)
                userData:nil] cStringUsingEncoding:NSASCIIStringEncoding]);
}

}  // namespace
