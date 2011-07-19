// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/tabpose_window.h"

#include "chrome/browser/tabs/tab_strip_model.h"
#import "chrome/browser/ui/browser_window.h"
#import "chrome/browser/ui/cocoa/browser_test_helper.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "content/browser/site_instance.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "testing/gtest/include/gtest/gtest.h"

class TabposeWindowTest : public CocoaTest {
 public:
  TabposeWindowTest() {
    site_instance_ =
        SiteInstance::CreateSiteInstance(browser_helper_.profile());
  }

  void AppendTabToStrip() {
    TabContentsWrapper* tab_contents = Browser::TabContentsFactory(
        browser_helper_.profile(), site_instance_, MSG_ROUTING_NONE,
        NULL, NULL);
    browser_helper_.browser()->tabstrip_model()->AppendTabContents(
        tab_contents, /*foreground=*/true);
  }

  BrowserTestHelper browser_helper_;
  scoped_refptr<SiteInstance> site_instance_;
};

// Check that this doesn't leak.
TEST_F(TabposeWindowTest, TestShow) {
  BrowserWindow* browser_window = browser_helper_.CreateBrowserWindow();
  NSWindow* parent = browser_window->GetNativeHandle();

  [parent orderFront:nil];
  EXPECT_TRUE([parent isVisible]);

  // Add a few tabs to the tab strip model.
  for (int i = 0; i < 3; ++i)
    AppendTabToStrip();

  base::mac::ScopedNSAutoreleasePool pool;
  TabposeWindow* window =
      [TabposeWindow openTabposeFor:parent
                               rect:NSMakeRect(10, 20, 250, 160)
                              slomo:NO
                     tabStripModel:browser_helper_.browser()->tabstrip_model()];

  // Should release the window.
  [window mouseDown:nil];

  browser_helper_.CloseBrowserWindow();
}

TEST_F(TabposeWindowTest, TestModelObserver) {
  BrowserWindow* browser_window = browser_helper_.CreateBrowserWindow();
  NSWindow* parent = browser_window->GetNativeHandle();
  [parent orderFront:nil];

  // Add a few tabs to the tab strip model.
  for (int i = 0; i < 3; ++i)
    AppendTabToStrip();

  base::mac::ScopedNSAutoreleasePool pool;
  TabposeWindow* window =
      [TabposeWindow openTabposeFor:parent
                               rect:NSMakeRect(10, 20, 250, 160)
                              slomo:NO
                     tabStripModel:browser_helper_.browser()->tabstrip_model()];

  // Exercise all the model change events.
  TabStripModel* model = browser_helper_.browser()->tabstrip_model();
  DCHECK_EQ([window thumbnailLayerCount], 3u);
  DCHECK_EQ([window selectedIndex], 2);

  model->MoveTabContentsAt(0, 2, /*select_after_move=*/false);
  DCHECK_EQ([window thumbnailLayerCount], 3u);
  DCHECK_EQ([window selectedIndex], 1);

  model->MoveTabContentsAt(2, 0, /*select_after_move=*/false);
  DCHECK_EQ([window thumbnailLayerCount], 3u);
  DCHECK_EQ([window selectedIndex], 2);

  [window selectTileAtIndexWithoutAnimation:0];
  DCHECK_EQ([window selectedIndex], 0);

  model->MoveTabContentsAt(0, 2, /*select_after_move=*/false);
  DCHECK_EQ([window selectedIndex], 2);

  model->MoveTabContentsAt(2, 0, /*select_after_move=*/false);
  DCHECK_EQ([window selectedIndex], 0);

  delete model->DetachTabContentsAt(0);
  DCHECK_EQ([window thumbnailLayerCount], 2u);
  DCHECK_EQ([window selectedIndex], 0);

  AppendTabToStrip();
  DCHECK_EQ([window thumbnailLayerCount], 3u);
  DCHECK_EQ([window selectedIndex], 0);

  model->CloseTabContentsAt(0, TabStripModel::CLOSE_NONE);
  DCHECK_EQ([window thumbnailLayerCount], 2u);
  DCHECK_EQ([window selectedIndex], 0);

  [window selectTileAtIndexWithoutAnimation:1];
  model->CloseTabContentsAt(0, TabStripModel::CLOSE_NONE);
  DCHECK_EQ([window thumbnailLayerCount], 1u);
  DCHECK_EQ([window selectedIndex], 0);

  // Should release the window.
  [window mouseDown:nil];

  browser_helper_.CloseBrowserWindow();
}
