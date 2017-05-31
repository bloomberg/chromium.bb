// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/bookmark_bubble_view.h"

#include "base/command_line.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/signin/fake_signin_manager_builder.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/star_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/testing_profile.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "ui/views/window/dialog_client_view.h"

namespace {

const char kTestBookmarkURL[] = "http://www.google.com";
const char kTestGaiaID[] = "test";
const char kTestUserEmail[] = "testuser@gtest.com";

}  // namespace

class BookmarkBubbleViewBrowserTest : public DialogBrowserTest {
 public:
  BookmarkBubbleViewBrowserTest() {}

  void SetUpOnMainThread() override {
    TestingProfile::Builder builder;
    builder.AddTestingFactory(SigninManagerFactory::GetInstance(),
                              BuildFakeSigninManagerBase);
    profile_ = builder.Build();
    profile_->CreateBookmarkModel(true);
    bookmarks::BookmarkModel* bookmark_model =
        BookmarkModelFactory::GetForBrowserContext(profile_.get());
    bookmarks::test::WaitForBookmarkModelToLoad(bookmark_model);
    // Don't persist bookmark changes to disk. BookmarkStorage does its writes
    // on the UI thread, which violates base::ThreadRestrictions.
    bookmark_model->ClearStore();
    bookmarks::AddIfNotBookmarked(bookmark_model, GURL(kTestBookmarkURL),
                                  base::string16());
  }

  void TearDownOnMainThread() override { profile_.reset(); }

  // DialogBrowserTest:
  void ShowDialog(const std::string& name) override {
    if (name == "bookmark_details") {
#if !defined(OS_CHROMEOS)
      SigninManagerFactory::GetForProfile(profile_.get())
          ->SignOut(signin_metrics::SIGNOUT_TEST,
                    signin_metrics::SignoutDelete::IGNORE_METRIC);
#endif
    } else {
      SigninManagerFactory::GetForProfile(profile_.get())
          ->SetAuthenticatedAccountInfo(kTestGaiaID, kTestUserEmail);
    }

    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    BookmarkBubbleView::ShowBubble(
        browser_view->toolbar()->location_bar()->star_view(), gfx::Rect(),
        nullptr, nullptr, nullptr, profile_.get(), GURL(kTestBookmarkURL),
        true);
    if (name == "ios_promotion") {
      BookmarkBubbleView::bookmark_bubble()
          ->GetWidget()
          ->client_view()
          ->AsDialogClientView()
          ->AcceptWindow();
    }
  }

 private:
  std::unique_ptr<TestingProfile> profile_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkBubbleViewBrowserTest);
};

IN_PROC_BROWSER_TEST_F(BookmarkBubbleViewBrowserTest,
                       InvokeDialog_bookmark_details) {
  RunDialog();
}

IN_PROC_BROWSER_TEST_F(BookmarkBubbleViewBrowserTest,
                       InvokeDialog_bookmark_details_signed_in) {
  RunDialog();
}

#if defined(OS_WIN)
IN_PROC_BROWSER_TEST_F(BookmarkBubbleViewBrowserTest,
                       InvokeDialog_ios_promotion) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kForceDesktopIOSPromotion);
  RunDialog();
}
#endif
