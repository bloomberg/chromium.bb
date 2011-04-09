// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#import "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/cocoa/browser_test_helper.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#import "chrome/browser/ui/cocoa/new_tab_button.h"
#import "chrome/browser/ui/cocoa/tabs/tab_strip_controller.h"
#import "chrome/browser/ui/cocoa/tabs/tab_strip_view.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "content/browser/site_instance.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

@interface TestTabStripControllerDelegate :
  NSObject<TabStripControllerDelegate> {
}
@end

@implementation TestTabStripControllerDelegate
- (void)onSelectTabWithContents:(TabContents*)contents {
}
- (void)onReplaceTabWithContents:(TabContents*)contents {
}
- (void)onSelectedTabChange:(TabStripModelObserver::TabChangeType)change {
}
- (void)onTabDetachedWithContents:(TabContents*)contents {
}
@end

namespace {

// Stub model delegate
class TestTabStripDelegate : public TabStripModelDelegate {
 public:
  virtual TabContentsWrapper* AddBlankTab(bool foreground) {
    return NULL;
  }
  virtual TabContentsWrapper* AddBlankTabAt(int index, bool foreground) {
    return NULL;
  }
  virtual Browser* CreateNewStripWithContents(TabContentsWrapper* contents,
                                              const gfx::Rect& window_bounds,
                                              const DockInfo& dock_info,
                                              bool maximize) {
    return NULL;
  }
  virtual void ContinueDraggingDetachedTab(TabContentsWrapper* contents,
                                           const gfx::Rect& window_bounds,
                                           const gfx::Rect& tab_bounds) {
  }
  virtual int GetDragActions() const {
    return 0;
  }
  virtual TabContentsWrapper* CreateTabContentsForURL(
      const GURL& url,
      const GURL& referrer,
      Profile* profile,
      PageTransition::Type transition,
      bool defer_load,
      SiteInstance* instance) const {
    return NULL;
  }
  virtual bool CanDuplicateContentsAt(int index) { return true; }
  virtual void DuplicateContentsAt(int index) { }
  virtual void CloseFrameAfterDragSession() { }
  virtual void CreateHistoricalTab(TabContentsWrapper* contents) { }
  virtual bool RunUnloadListenerBeforeClosing(TabContentsWrapper* contents) {
    return true;
  }
  virtual bool CanRestoreTab() {
    return true;
  }
  virtual void RestoreTab() {}

  virtual bool CanCloseContentsAt(int index) { return true; }

  virtual bool CanBookmarkAllTabs() const { return false; }

  virtual bool CanCloseTab() const { return true; }

  virtual void BookmarkAllTabs() {}

  virtual bool UseVerticalTabs() const { return false; }

  virtual void ToggleUseVerticalTabs() {}

  virtual bool LargeIconsPermitted() const { return true; }
};

class TabStripControllerTest : public CocoaTest {
 public:
  TabStripControllerTest() {
    Browser* browser = browser_helper_.browser();
    BrowserWindow* browser_window = browser_helper_.CreateBrowserWindow();
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
    scoped_nsobject<TabStripView> tab_strip(
        [[TabStripView alloc] initWithFrame:strip_frame]);
    [parent addSubview:tab_strip.get()];
    NSRect button_frame = NSMakeRect(0, 0, 15, 15);
    scoped_nsobject<NewTabButton> new_tab_button(
        [[NewTabButton alloc] initWithFrame:button_frame]);
    [tab_strip addSubview:new_tab_button.get()];
    [tab_strip setNewTabButton:new_tab_button.get()];

    delegate_.reset(new TestTabStripDelegate());
    model_ = browser->tabstrip_model();
    controller_delegate_.reset([TestTabStripControllerDelegate alloc]);
    controller_.reset([[TabStripController alloc]
                        initWithView:static_cast<TabStripView*>(tab_strip.get())
                          switchView:switch_view.get()
                             browser:browser
                            delegate:controller_delegate_.get()]);
  }

  virtual void TearDown() {
    browser_helper_.CloseBrowserWindow();
    // The call to CocoaTest::TearDown() deletes the Browser and TabStripModel
    // objects, so we first have to delete the controller, which refers to them.
    controller_.reset(nil);
    model_ = NULL;
    CocoaTest::TearDown();
  }

  BrowserTestHelper browser_helper_;
  scoped_ptr<TestTabStripDelegate> delegate_;
  TabStripModel* model_;
  scoped_nsobject<TestTabStripControllerDelegate> controller_delegate_;
  scoped_nsobject<TabStripController> controller_;
};

// Test adding and removing tabs and making sure that views get added to
// the tab strip.
TEST_F(TabStripControllerTest, AddRemoveTabs) {
  EXPECT_TRUE(model_->empty());
  SiteInstance* instance =
      SiteInstance::CreateSiteInstance(browser_helper_.profile());
  TabContentsWrapper* tab_contents =
      Browser::TabContentsFactory(browser_helper_.profile(), instance,
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

}  // namespace
