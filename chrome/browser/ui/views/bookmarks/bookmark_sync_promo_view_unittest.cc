// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/bookmark_sync_promo_view.h"

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/bookmarks/bookmark_bubble_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/range/range.h"

class BookmarkSyncPromoViewTest : public BookmarkBubbleDelegate,
                                  public testing::Test {
 public:
  BookmarkSyncPromoViewTest() : sign_in_clicked_count_(0) {}

 protected:
  // BookmarkBubbleDelegate:
  virtual void OnSignInLinkClicked() OVERRIDE {
    ++sign_in_clicked_count_;
  }

  // Number of times that OnSignInLinkClicked has been called.
  int sign_in_clicked_count_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BookmarkSyncPromoViewTest);
};

TEST_F(BookmarkSyncPromoViewTest, SignInLink) {
  scoped_ptr<BookmarkSyncPromoView> sync_promo;
  sync_promo.reset(new BookmarkSyncPromoView(this));

  // Simulate clicking the "Sign in" link.
  views::StyledLabelListener* listener = sync_promo.get();
  listener->StyledLabelLinkClicked(gfx::Range(), ui::EF_NONE);

  EXPECT_EQ(1, sign_in_clicked_count_);
}
