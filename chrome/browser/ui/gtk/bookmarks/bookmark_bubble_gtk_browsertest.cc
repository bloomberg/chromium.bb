// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/bookmarks/bookmark_bubble_gtk.h"

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const char kTestBookmarkURL[] = "http://www.google.com";
} // namespace

class BookmarkBubbleGtkBrowserTest : public InProcessBrowserTest {
 public:
  BookmarkBubbleGtkBrowserTest() {}

  // content::BrowserTestBase:
  virtual void SetUpOnMainThread() OVERRIDE {
    bookmark_utils::AddIfNotBookmarked(
        BookmarkModelFactory::GetForProfile(browser()->profile()),
        GURL(kTestBookmarkURL),
        string16());
  }

 protected:
  // Creates a bookmark bubble.
  void CreateBookmarkBubble() {
    BookmarkBubbleGtk::Show(GTK_WIDGET(browser()->window()->GetNativeWindow()),
                            browser()->profile(),
                            GURL(kTestBookmarkURL),
                            true);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BookmarkBubbleGtkBrowserTest);
};

// Verifies that the sync promo is not displayed for a signed in user.
IN_PROC_BROWSER_TEST_F(BookmarkBubbleGtkBrowserTest, SyncPromoSignedIn) {
  SigninManager* signin = SigninManagerFactory::GetForProfile(
      browser()->profile());
  signin->SetAuthenticatedUsername("fake_username");

  CreateBookmarkBubble();
  EXPECT_FALSE(BookmarkBubbleGtk::bookmark_bubble_->promo_);
}

// Verifies that the sync promo is displayed for a user that is not signed in.
IN_PROC_BROWSER_TEST_F(BookmarkBubbleGtkBrowserTest, SyncPromoNotSignedIn) {
  CreateBookmarkBubble();
  EXPECT_TRUE(BookmarkBubbleGtk::bookmark_bubble_->promo_);
}

// Verifies that a new tab is opened when the "Sign in" link is clicked.
IN_PROC_BROWSER_TEST_F(BookmarkBubbleGtkBrowserTest, SyncPromoLink) {
  GURL initial_url =
      browser()->tab_strip_model()->GetActiveWebContents()->GetVisibleURL();
  CreateBookmarkBubble();

  // Simulate clicking the "Sign in" link.
  BookmarkBubbleGtk::bookmark_bubble_->OnSignInClicked(NULL, NULL);

  EXPECT_NE(
      initial_url,
      browser()->tab_strip_model()->GetActiveWebContents()->GetVisibleURL());
}

