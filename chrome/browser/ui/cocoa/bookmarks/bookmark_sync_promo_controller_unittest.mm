// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/bookmarks/bookmark_sync_promo_controller.h"

#import <Cocoa/Cocoa.h>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/mac/scoped_nsobject.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class BookmarkSyncPromoControllerTest : public BrowserWithTestWindowTest {
 public:
  virtual void SetUp() {
    BrowserWithTestWindowTest::SetUp();
    ASSERT_TRUE(profile());
    // Adds TestExtensionSystem, since signin uses the gaia auth extension.
    static_cast<extensions::TestExtensionSystem*>(
        extensions::ExtensionSystem::Get(profile()))->CreateExtensionService(
            CommandLine::ForCurrentProcess(), base::FilePath(), false);
  }
};

TEST_F(BookmarkSyncPromoControllerTest, SignInLink) {
  int starting_tab_count = browser()->tab_strip_model()->count();

  base::scoped_nsobject<BookmarkSyncPromoController> syncPromo(
      [[BookmarkSyncPromoController alloc] initWithBrowser:browser()]);

  // Simulate clicking the "Sign in" link.
  [syncPromo textView:nil clickedOnLink:nil atIndex:0u];

  // A new tab should have been opened.
  int tab_count = browser()->tab_strip_model()->count();
  EXPECT_EQ(starting_tab_count + 1, tab_count);
}

}  // namespace
