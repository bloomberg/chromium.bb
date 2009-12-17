// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#import "chrome/browser/browser_window.h"
#include "chrome/browser/cocoa/browser_test_helper.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#import "chrome/browser/cocoa/tab_strip_controller.h"
#import "chrome/browser/cocoa/tab_strip_view.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/renderer_host/site_instance.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

// Stub model delegate
class TestTabStripDelegate : public TabStripModelDelegate {
 public:
  virtual TabContents* AddBlankTab(bool foreground) {
    return NULL;
  }
  virtual TabContents* AddBlankTabAt(int index, bool foreground) {
    return NULL;
  }
  virtual Browser* CreateNewStripWithContents(TabContents* contents,
                                              const gfx::Rect& window_bounds,
                                              const DockInfo& dock_info) {
    return NULL;
  }
  virtual void ContinueDraggingDetachedTab(TabContents* contents,
                                         const gfx::Rect& window_bounds,
                                         const gfx::Rect& tab_bounds) {
  }
  virtual int GetDragActions() const {
    return 0;
  }
  virtual TabContents* CreateTabContentsForURL(
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
  virtual void CreateHistoricalTab(TabContents* contents) { }
  virtual bool RunUnloadListenerBeforeClosing(TabContents* contents) {
    return true;
  }
  virtual bool CanRestoreTab() {
    return true;
  }
  virtual void RestoreTab() {}

  virtual bool CanCloseContentsAt(int index) { return true; }

  virtual bool CanBookmarkAllTabs() const { return false; }

  virtual void BookmarkAllTabs() {}
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
    scoped_nsobject<NSButton> new_tab_button(
        [[NSButton alloc] initWithFrame:button_frame]);
    [tab_strip addSubview:new_tab_button.get()];
    [tab_strip setNewTabButton:new_tab_button.get()];

    delegate_.reset(new TestTabStripDelegate());
    model_ = browser->tabstrip_model();
    controller_.reset([[TabStripController alloc]
                        initWithView:static_cast<TabStripView*>(tab_strip.get())
                          switchView:switch_view.get()
                             browser:browser]);
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
  scoped_nsobject<TabStripController> controller_;
};

// Test adding and removing tabs and making sure that views get added to
// the tab strip.
TEST_F(TabStripControllerTest, AddRemoveTabs) {
  EXPECT_TRUE(model_->empty());
  SiteInstance* instance =
      SiteInstance::CreateSiteInstance(browser_helper_.profile());
  TabContents* tab_contents =
      new TabContents(browser_helper_.profile(), instance, MSG_ROUTING_NONE,
                      NULL);
  model_->AppendTabContents(tab_contents, true);
  EXPECT_EQ(model_->count(), 1);
}

TEST_F(TabStripControllerTest, SelectTab) {
  // TODO(pinkerton): Implement
}

TEST_F(TabStripControllerTest, RearrangeTabs) {
  // TODO(pinkerton): Implement
}

// Test that changing the number of tabs broadcasts a
// kTabStripNumberOfTabsChanged notifiction.
TEST_F(TabStripControllerTest, Notifications) {
  // TODO(pinkerton): Implement
}

}  // namespace
