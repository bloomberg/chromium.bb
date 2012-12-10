// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/tabpose_window.h"

#include "base/mac/mac_util.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/cocoa/cocoa_profile_test.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::SiteInstance;

class TabposeWindowTest : public CocoaProfileTest {
 public:
  virtual void SetUp() {
    CocoaProfileTest::SetUp();
    ASSERT_TRUE(profile());

    site_instance_ = SiteInstance::Create(profile());
  }

  void AppendTabToStrip() {
    content::WebContents* web_contents = content::WebContents::Create(
        content::WebContents::CreateParams(profile(), site_instance_));
    browser()->tab_strip_model()->AppendWebContents(
        web_contents, /*foreground=*/true);
  }

  scoped_refptr<SiteInstance> site_instance_;
};

TEST_F(TabposeWindowTest, TestShow) {
  // Skip this test on 10.7
  // http://code.google.com/p/chromium/issues/detail?id=127845
  if (base::mac::IsOSLionOrLater()) {
    return;
  }

  NSWindow* parent = browser()->window()->GetNativeWindow();

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
                      tabStripModel:browser()->tab_strip_model()];

  // Should release the window.
  [window mouseDown:nil];
}

TEST_F(TabposeWindowTest, TestModelObserver) {
  // Skip this test on 10.7
  // http://code.google.com/p/chromium/issues/detail?id=127845
  if (base::mac::IsOSLionOrLater()) {
    return;
  }

  NSWindow* parent = browser()->window()->GetNativeWindow();
  [parent orderFront:nil];

  // Add a few tabs to the tab strip model.
  for (int i = 0; i < 3; ++i)
    AppendTabToStrip();

  base::mac::ScopedNSAutoreleasePool pool;
  TabposeWindow* window =
      [TabposeWindow openTabposeFor:parent
                               rect:NSMakeRect(10, 20, 250, 160)
                              slomo:NO
                      tabStripModel:browser()->tab_strip_model()];

  // Exercise all the model change events.
  TabStripModel* model = browser()->tab_strip_model();
  DCHECK_EQ([window thumbnailLayerCount], 3u);
  DCHECK_EQ([window selectedIndex], 2);

  model->MoveWebContentsAt(0, 2, /*select_after_move=*/false);
  DCHECK_EQ([window thumbnailLayerCount], 3u);
  DCHECK_EQ([window selectedIndex], 1);

  model->MoveWebContentsAt(2, 0, /*select_after_move=*/false);
  DCHECK_EQ([window thumbnailLayerCount], 3u);
  DCHECK_EQ([window selectedIndex], 2);

  [window selectTileAtIndexWithoutAnimation:0];
  DCHECK_EQ([window selectedIndex], 0);

  model->MoveWebContentsAt(0, 2, /*select_after_move=*/false);
  DCHECK_EQ([window selectedIndex], 2);

  model->MoveWebContentsAt(2, 0, /*select_after_move=*/false);
  DCHECK_EQ([window selectedIndex], 0);

  delete model->DetachWebContentsAt(0);
  DCHECK_EQ([window thumbnailLayerCount], 2u);
  DCHECK_EQ([window selectedIndex], 0);

  AppendTabToStrip();
  DCHECK_EQ([window thumbnailLayerCount], 3u);
  DCHECK_EQ([window selectedIndex], 0);

  model->CloseWebContentsAt(0, TabStripModel::CLOSE_NONE);
  DCHECK_EQ([window thumbnailLayerCount], 2u);
  DCHECK_EQ([window selectedIndex], 0);

  [window selectTileAtIndexWithoutAnimation:1];
  model->CloseWebContentsAt(0, TabStripModel::CLOSE_NONE);
  DCHECK_EQ([window thumbnailLayerCount], 1u);
  DCHECK_EQ([window selectedIndex], 0);

  // Should release the window.
  [window mouseDown:nil];
}
