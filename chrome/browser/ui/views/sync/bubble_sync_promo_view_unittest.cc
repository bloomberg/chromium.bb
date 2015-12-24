// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/sync/bubble_sync_promo_view.h"

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/sync/bubble_sync_promo_delegate.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/range/range.h"
#include "ui/views/controls/styled_label.h"

class BubbleSyncPromoViewTest : public BubbleSyncPromoDelegate,
                                public testing::Test {
 public:
  BubbleSyncPromoViewTest() : sign_in_clicked_count_(0) {}

 protected:
  // BubbleSyncPromoDelegate:
  void OnSignInLinkClicked() override { ++sign_in_clicked_count_; }

  // Number of times that OnSignInLinkClicked has been called.
  int sign_in_clicked_count_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BubbleSyncPromoViewTest);
};

TEST_F(BubbleSyncPromoViewTest, SignInLink) {
  scoped_ptr<BubbleSyncPromoView> sync_promo;
  sync_promo.reset(new BubbleSyncPromoView(this, IDS_BOOKMARK_SYNC_PROMO_LINK,
                                           IDS_BOOKMARK_SYNC_PROMO_MESSAGE));

  // Simulate clicking the "Sign in" link.
  views::StyledLabel styled_label(base::ASCIIToUTF16("test"), nullptr);
  views::StyledLabelListener* listener = sync_promo.get();
  listener->StyledLabelLinkClicked(&styled_label, gfx::Range(), ui::EF_NONE);

  EXPECT_EQ(1, sign_in_clicked_count_);
}
