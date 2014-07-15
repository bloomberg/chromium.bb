// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/bookmark_bubble_view.h"

#include <string>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/signin/fake_signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/bookmarks/bookmark_bubble_delegate.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/signin/core/browser/signin_manager.h"

namespace {
const char kTestBookmarkURL[] = "http://www.google.com";
} // namespace

class BookmarkBubbleViewTest : public BrowserWithTestWindowTest {
 public:
  BookmarkBubbleViewTest() {}

  // testing::Test:
  virtual void SetUp() OVERRIDE {
    BrowserWithTestWindowTest::SetUp();

    profile()->CreateBookmarkModel(true);
    BookmarkModel* bookmark_model =
        BookmarkModelFactory::GetForProfile(profile());
    test::WaitForBookmarkModelToLoad(bookmark_model);

    bookmarks::AddIfNotBookmarked(
        bookmark_model, GURL(kTestBookmarkURL), base::string16());
  }

  virtual void TearDown() OVERRIDE {
    // Make sure the bubble is destroyed before the profile to avoid a crash.
    bubble_.reset();

    BrowserWithTestWindowTest::TearDown();
  }

  // BrowserWithTestWindowTest:
  virtual TestingProfile* CreateProfile() OVERRIDE {
    TestingProfile::Builder builder;
    builder.AddTestingFactory(SigninManagerFactory::GetInstance(),
                              FakeSigninManagerBase::Build);
    return builder.Build().release();
  }

 protected:
  // Creates a bookmark bubble view.
  void CreateBubbleView() {
    scoped_ptr<BookmarkBubbleDelegate> delegate;
    bubble_.reset(new BookmarkBubbleView(NULL,
                                         NULL,
                                         delegate.Pass(),
                                         profile(),
                                         GURL(kTestBookmarkURL),
                                         true));
  }

  void SetUpSigninManager(const std::string& username) {
    if (username.empty())
      return;
    SigninManagerBase* signin_manager = static_cast<SigninManagerBase*>(
        SigninManagerFactory::GetForProfile(profile()));
    ASSERT_TRUE(signin_manager);
    signin_manager->SetAuthenticatedUsername(username);
  }

  scoped_ptr<BookmarkBubbleView> bubble_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BookmarkBubbleViewTest);
};

// Verifies that the sync promo is not displayed for a signed in user.
TEST_F(BookmarkBubbleViewTest, SyncPromoSignedIn) {
  SetUpSigninManager("fake_username");
  CreateBubbleView();
  bubble_->Init();
  EXPECT_FALSE(bubble_->sync_promo_view_);
}

// Verifies that the sync promo is displayed for a user that is not signed in.
TEST_F(BookmarkBubbleViewTest, SyncPromoNotSignedIn) {
  CreateBubbleView();
  bubble_->Init();
#if defined(OS_CHROMEOS)
  EXPECT_FALSE(bubble_->sync_promo_view_);
#else  // !defined(OS_CHROMEOS)
  EXPECT_TRUE(bubble_->sync_promo_view_);
#endif
}
