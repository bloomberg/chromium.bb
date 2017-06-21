// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/lock_contents_view.h"
#include "ash/login/ui/login_auth_user_view.h"
#include "ash/login/ui/login_display_style.h"
#include "ash/login/ui/login_test_base.h"
#include "ash/login/ui/login_user_view.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget.h"

using LockContentsViewUnitTest = ash::LoginTestBase;

namespace ash {

TEST_F(LockContentsViewUnitTest, DisplayMode) {
  // Build lock screen with 1 user.
  auto* contents = new LockContentsView(data_dispatcher());
  SetUserCount(1);
  ShowWidgetWithContent(contents);

  // Check that there are no extra user views shown.
  LockContentsView::TestApi test_api(contents);
  EXPECT_EQ(0u, test_api.user_views().size());

  // Verify user names and pod style is set correctly for 2-25 users. This also
  // sanity checks that LockContentsView can respond to a multiple user change
  // events fired from the data dispatcher, which is needed for the debug UI.
  for (size_t user_count = 2; user_count < 25; ++user_count) {
    SetUserCount(user_count);
    EXPECT_EQ(user_count - 1, test_api.user_views().size());

    // 1 extra user gets large style.
    LoginDisplayStyle expected_style = LoginDisplayStyle::kLarge;
    // 2-6 extra users get small style.
    if (user_count >= 3)
      expected_style = LoginDisplayStyle::kSmall;
    // 7+ users get get extra small style.
    if (user_count >= 7)
      expected_style = LoginDisplayStyle::kExtraSmall;

    for (size_t i = 0; i < test_api.user_views().size(); ++i) {
      LoginUserView::TestApi user_test_api(test_api.user_views()[i]);
      EXPECT_EQ(expected_style, user_test_api.display_style());

      const mojom::UserInfoPtr& user = users()[i + 1];
      EXPECT_EQ(base::UTF8ToUTF16(user->display_name),
                user_test_api.displayed_name());
    }
  }
}

// Verifies that the single user view is centered.
TEST_F(LockContentsViewUnitTest, SingleUserCentered) {
  auto* contents = new LockContentsView(data_dispatcher());
  SetUserCount(1);
  ShowWidgetWithContent(contents);

  LockContentsView::TestApi test_api(contents);
  LoginAuthUserView* auth_view = test_api.auth_user_view();
  gfx::Rect widget_bounds = widget()->GetWindowBoundsInScreen();
  int expected_margin =
      (widget_bounds.width() - auth_view->GetPreferredSize().width()) / 2;
  gfx::Rect auth_bounds = auth_view->GetBoundsInScreen();

  EXPECT_NE(0, expected_margin);
  EXPECT_EQ(expected_margin, auth_bounds.x());
  EXPECT_EQ(expected_margin,
            widget_bounds.width() - (auth_bounds.x() + auth_bounds.width()));
}

}  // namespace ash