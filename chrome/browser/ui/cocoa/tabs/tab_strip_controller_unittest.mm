// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsautorelease_pool.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/cocoa/cocoa_profile_test.h"
#import "chrome/browser/ui/cocoa/new_tab_button.h"
#import "chrome/browser/ui/cocoa/tabs/tab_controller.h"
#import "chrome/browser/ui/cocoa/tabs/tab_strip_controller.h"
#import "chrome/browser/ui/cocoa/tabs/tab_strip_view.h"
#import "chrome/browser/ui/cocoa/tabs/tab_view.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "ui/base/test/cocoa_test_event_utils.h"

using content::SiteInstance;
using content::WebContents;

@interface TestTabStripControllerDelegate
    : NSObject<TabStripControllerDelegate> {
}
@end

@implementation TestTabStripControllerDelegate
- (void)onActivateTabWithContents:(WebContents*)contents {
}
- (void)onTabChanged:(TabStripModelObserver::TabChangeType)change
        withContents:(WebContents*)contents {
}
- (void)onTabDetachedWithContents:(WebContents*)contents {
}
@end


// Helper class for invoking a base::Closure via
// -performSelector:withObject:afterDelay:.
@interface TestClosureRunner : NSObject {
 @private
  base::Closure closure_;
}
- (id)initWithClosure:(const base::Closure&)closure;
- (void)scheduleDelayedRun;
- (void)run;
@end

@implementation TestClosureRunner
- (id)initWithClosure:(const base::Closure&)closure {
  if (self) {
    closure_ = closure;
  }
  return self;
}
- (void)scheduleDelayedRun {
  [self performSelector:@selector(run) withObject:nil afterDelay:0];
}
- (void)run {
  closure_.Run();
}
@end

@interface TabStripController (Test)

- (void)mouseMoved:(NSEvent*)event;

@end

@implementation TabView (Test)

- (TabController*)controller {
  return controller_;
}

@end

namespace {

class TabStripControllerTest : public CocoaProfileTest {
 public:
  virtual void SetUp() OVERRIDE {
    CocoaProfileTest::SetUp();
    ASSERT_TRUE(browser());

    NSWindow* window = browser()->window()->GetNativeWindow();
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
    model_ = browser()->tab_strip_model();
    controller_delegate_.reset([TestTabStripControllerDelegate alloc]);
    controller_.reset([[TabStripController alloc]
                      initWithView:static_cast<TabStripView*>(tab_strip_.get())
                        switchView:switch_view.get()
                           browser:browser()
                          delegate:controller_delegate_.get()]);
  }

  virtual void TearDown() OVERRIDE {
    // The call to CocoaTest::TearDown() deletes the Browser and TabStripModel
    // objects, so we first have to delete the controller, which refers to them.
    controller_.reset();
    model_ = NULL;
    CocoaProfileTest::TearDown();
  }

  TabView* CreateTab() {
    SiteInstance* instance = SiteInstance::Create(profile());
    WebContents* web_contents = WebContents::Create(
        content::WebContents::CreateParams(profile(), instance));
    model_->AppendWebContents(web_contents, true);
    const NSUInteger tab_count = [controller_.get() viewsCount];
    return static_cast<TabView*>([controller_.get() viewAtIndex:tab_count - 1]);
  }

  // Closes all tabs and unrefs the tabstrip and then posts a NSLeftMouseUp
  // event which should end the nested drag event loop.
  void CloseTabsAndEndDrag() {
    // Simulate a close of the browser window.
    model_->CloseAllTabs();
    controller_.reset();
    tab_strip_.reset();
    // Schedule a NSLeftMouseUp to end the nested drag event loop.
    NSEvent* event =
        cocoa_test_event_utils::MouseEventWithType(NSLeftMouseUp, 0);
    [NSApp postEvent:event atStart:NO];
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
  CreateTab();
  EXPECT_EQ(model_->count(), 1);
}

TEST_F(TabStripControllerTest, SelectTab) {
  // TODO(pinkerton): Implement http://crbug.com/10899
}

TEST_F(TabStripControllerTest, RearrangeTabs) {
  // TODO(pinkerton): Implement http://crbug.com/10899
}

TEST_F(TabStripControllerTest, CorrectToolTipText) {
  // Set tab 1 tooltip.
  TabView* tab1 = CreateTab();
  [tab1 setToolTip:@"Tab1"];

  // Set tab 2 tooltip.
  TabView* tab2 = CreateTab();
  [tab2 setToolTip:@"Tab2"];

  EXPECT_FALSE([tab1 controller].selected);
  EXPECT_TRUE([tab2 controller].selected);

  // Check that there's no tooltip yet.
  EXPECT_FALSE([controller_ view:nil
                stringForToolTip:nil
                           point:NSMakePoint(0,0)
                        userData:nil]);

  // Set up mouse event on overlap of tab1 + tab2.
  const CGFloat min_y = NSMinY([tab_strip_.get() frame]) + 1;

  // Hover over overlap between tab 1 and 2.
  NSEvent* event =
      cocoa_test_event_utils::MouseEventAtPoint(NSMakePoint(280, min_y),
                                                NSMouseMoved, 0);
  [controller_.get() mouseMoved:event];
  EXPECT_STREQ("Tab2",
      [[controller_ view:nil
        stringForToolTip:nil
                   point:NSMakePoint(0,0)
                userData:nil] cStringUsingEncoding:NSASCIIStringEncoding]);


  // Hover over tab 1.
  event = cocoa_test_event_utils::MouseEventAtPoint(NSMakePoint(260, min_y),
                                                    NSMouseMoved, 0);
  [controller_.get() mouseMoved:event];
  EXPECT_STREQ("Tab1",
      [[controller_ view:nil
        stringForToolTip:nil
                   point:NSMakePoint(0,0)
                userData:nil] cStringUsingEncoding:NSASCIIStringEncoding]);

  // Hover over tab 2.
  event = cocoa_test_event_utils::MouseEventAtPoint(NSMakePoint(290, min_y),
                                                    NSMouseMoved, 0);
  [controller_.get() mouseMoved:event];
  EXPECT_STREQ("Tab2",
      [[controller_ view:nil
        stringForToolTip:nil
                   point:NSMakePoint(0,0)
                userData:nil] cStringUsingEncoding:NSASCIIStringEncoding]);
}

TEST_F(TabStripControllerTest, TabCloseDuringDrag) {
  TabController* tab;
  // The TabController gets autoreleased when created, but is owned by the
  // tab strip model. Use a ScopedNSAutoreleasePool to get a truly weak ref
  // to it to test that -maybeStartDrag:forTab: can handle that properly.
  {
    base::mac::ScopedNSAutoreleasePool pool;
    tab = [CreateTab() controller];
  }

  // Schedule a task to close all the tabs and stop the drag, before the call to
  // -maybeStartDrag:forTab:, which starts a nested event loop. This task will
  // run in that nested event loop, which shouldn't crash.
  scoped_nsobject<TestClosureRunner> runner(
      [[TestClosureRunner alloc] initWithClosure:
          base::Bind(&TabStripControllerTest::CloseTabsAndEndDrag,
                     base::Unretained(this))]);
  [runner scheduleDelayedRun];

  NSEvent* event =
      cocoa_test_event_utils::LeftMouseDownAtPoint(NSMakePoint(0, 0));
  [[controller_ dragController] maybeStartDrag:event forTab:tab];
}

TEST_F(TabStripControllerTest, ViewAccessibility_Contents) {
  NSArray* attrs = [tab_strip_ accessibilityAttributeNames];
  ASSERT_TRUE([attrs containsObject:NSAccessibilityContentsAttribute]);

  // Create two tabs and ensure they exist in the contents array.
  TabView* tab1 = CreateTab();
  TabView* tab2 = CreateTab();
  NSObject* contents =
      [tab_strip_ accessibilityAttributeValue:NSAccessibilityContentsAttribute];
  DCHECK([contents isKindOfClass:[NSArray class]]);
  NSArray* contentsArray = static_cast<NSArray*>(contents);
  ASSERT_TRUE([contentsArray containsObject:tab1]);
  ASSERT_TRUE([contentsArray containsObject:tab2]);
}

TEST_F(TabStripControllerTest, ViewAccessibility_Value) {
  NSArray* attrs = [tab_strip_ accessibilityAttributeNames];
  ASSERT_TRUE([attrs containsObject:NSAccessibilityValueAttribute]);

  // Create two tabs and ensure the active one gets returned.
  TabView* tab1 = CreateTab();
  TabView* tab2 = CreateTab();
  EXPECT_FALSE([tab1 controller].selected);
  EXPECT_TRUE([tab2 controller].selected);
  NSObject* value =
      [tab_strip_ accessibilityAttributeValue:NSAccessibilityValueAttribute];
  EXPECT_EQ(tab2, value);

  model_->ActivateTabAt(0, false);
  EXPECT_TRUE([tab1 controller].selected);
  EXPECT_FALSE([tab2 controller].selected);
  value =
      [tab_strip_ accessibilityAttributeValue:NSAccessibilityValueAttribute];
  EXPECT_EQ(tab1, value);
}

}  // namespace
