// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <unordered_set>

#include "ash/login/ui/lock_contents_view.h"
#include "ash/login/ui/login_auth_user_view.h"
#include "ash/login/ui/login_display_style.h"
#include "ash/login/ui/login_test_base.h"
#include "ash/login/ui/login_user_view.h"
#include "ash/public/interfaces/tray_action.mojom.h"
#include "ash/shell.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget.h"

namespace ash {

using LockContentsViewUnitTest = LoginTestBase;

TEST_F(LockContentsViewUnitTest, DisplayMode) {
  // Build lock screen with 1 user.
  auto* contents = new LockContentsView(mojom::TrayActionState::kNotAvailable,
                                        data_dispatcher());
  SetUserCount(1);
  std::unique_ptr<views::Widget> widget = CreateWidgetWithContent(contents);

  // Verify user list and secondary auth are not shown for one user.
  LockContentsView::TestApi test_api(contents);
  EXPECT_EQ(0u, test_api.user_views().size());
  EXPECT_FALSE(test_api.opt_secondary_auth());

  // Verify user list is not shown for two users, but secondary auth is.
  SetUserCount(2);
  EXPECT_EQ(0u, test_api.user_views().size());
  EXPECT_TRUE(test_api.opt_secondary_auth());

  // Verify user names and pod style is set correctly for 3-25 users. This also
  // sanity checks that LockContentsView can respond to a multiple user change
  // events fired from the data dispatcher, which is needed for the debug UI.
  for (size_t user_count = 3; user_count < 25; ++user_count) {
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

      const mojom::LoginUserInfoPtr& user = users()[i + 1];
      EXPECT_EQ(base::UTF8ToUTF16(user->basic_user_info->display_name),
                user_test_api.displayed_name());
    }
  }
}

// Verifies that the single user view is centered.
TEST_F(LockContentsViewUnitTest, SingleUserCentered) {
  auto* contents = new LockContentsView(mojom::TrayActionState::kNotAvailable,
                                        data_dispatcher());
  SetUserCount(1);
  std::unique_ptr<views::Widget> widget = CreateWidgetWithContent(contents);

  LockContentsView::TestApi test_api(contents);
  LoginAuthUserView* auth_view = test_api.primary_auth();
  gfx::Rect widget_bounds = widget->GetWindowBoundsInScreen();
  int expected_margin =
      (widget_bounds.width() - auth_view->GetPreferredSize().width()) / 2;
  gfx::Rect auth_bounds = auth_view->GetBoundsInScreen();

  EXPECT_NE(0, expected_margin);
  EXPECT_EQ(expected_margin, auth_bounds.x());
  EXPECT_EQ(expected_margin,
            widget_bounds.width() - (auth_bounds.x() + auth_bounds.width()));
}

// Verifies that the single user view is centered when lock screen notes are
// enabled.
TEST_F(LockContentsViewUnitTest, SingleUserCenteredNoteActionEnabled) {
  auto* contents = new LockContentsView(mojom::TrayActionState::kAvailable,
                                        data_dispatcher());
  SetUserCount(1);
  std::unique_ptr<views::Widget> widget = CreateWidgetWithContent(contents);

  LockContentsView::TestApi test_api(contents);
  LoginAuthUserView* auth_view = test_api.primary_auth();
  gfx::Rect widget_bounds = widget->GetWindowBoundsInScreen();
  int expected_margin =
      (widget_bounds.width() - auth_view->GetPreferredSize().width()) / 2;
  gfx::Rect auth_bounds = auth_view->GetBoundsInScreen();

  EXPECT_NE(0, expected_margin);
  EXPECT_EQ(expected_margin, auth_bounds.x());
  EXPECT_EQ(expected_margin,
            widget_bounds.width() - (auth_bounds.x() + auth_bounds.width()));
}

// Verifies that layout dynamically updates after a rotation by checking the
// distance between the auth user and the user list in landscape and portrait
// mode.
TEST_F(LockContentsViewUnitTest, AutoLayoutAfterRotation) {
  // Build lock screen with three users.
  auto* contents = new LockContentsView(mojom::TrayActionState::kNotAvailable,
                                        data_dispatcher());
  LockContentsView::TestApi test_api(contents);
  SetUserCount(3);
  std::unique_ptr<views::Widget> widget = CreateWidgetWithContent(contents);

  // Returns the distance between the auth user view and the user view.
  auto calculate_distance = [&]() {
    if (test_api.opt_secondary_auth()) {
      return test_api.opt_secondary_auth()->GetBoundsInScreen().x() -
             test_api.primary_auth()->GetBoundsInScreen().x();
    }
    return test_api.user_views()[0]->GetBoundsInScreen().x() -
           test_api.primary_auth()->GetBoundsInScreen().x();
  };

  const display::Display& display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(
          widget->GetNativeWindow());
  for (int i = 2; i < 10; ++i) {
    SetUserCount(i);

    // Start at 0 degrees (landscape).
    display_manager()->SetDisplayRotation(
        display.id(), display::Display::ROTATE_0,
        display::Display::ROTATION_SOURCE_ACTIVE);
    int distance_0deg = calculate_distance();
    EXPECT_NE(distance_0deg, 0);

    // Rotate the display to 90 degrees (portrait).
    display_manager()->SetDisplayRotation(
        display.id(), display::Display::ROTATE_90,
        display::Display::ROTATION_SOURCE_ACTIVE);
    int distance_90deg = calculate_distance();
    EXPECT_GT(distance_0deg, distance_90deg);

    // Rotate the display back to 0 degrees (landscape).
    display_manager()->SetDisplayRotation(
        display.id(), display::Display::ROTATE_0,
        display::Display::ROTATION_SOURCE_ACTIVE);
    int distance_180deg = calculate_distance();
    EXPECT_EQ(distance_0deg, distance_180deg);
    EXPECT_NE(distance_0deg, distance_90deg);
  }
}

// Ensures that when swapping between two users, only auth method display swaps.
TEST_F(LockContentsViewUnitTest, SwapAuthUsersInTwoUserLayout) {
  // Build lock screen with two users.
  auto* contents = new LockContentsView(mojom::TrayActionState::kNotAvailable,
                                        data_dispatcher());
  LockContentsView::TestApi test_api(contents);
  SetUserCount(2);
  std::unique_ptr<views::Widget> widget = CreateWidgetWithContent(contents);

  // Capture user info to validate it did not change during the swap.
  AccountId primary_user =
      test_api.primary_auth()->current_user()->basic_user_info->account_id;
  AccountId secondary_user = test_api.opt_secondary_auth()
                                 ->current_user()
                                 ->basic_user_info->account_id;
  EXPECT_NE(primary_user, secondary_user);

  auto has_auth = [](LoginAuthUserView* view) -> bool {
    return view->auth_methods() != LoginAuthUserView::AUTH_NONE;
  };

  // Primary user starts with auth. Secondary user does not have any auth.
  EXPECT_TRUE(has_auth(test_api.primary_auth()));
  EXPECT_FALSE(has_auth(test_api.opt_secondary_auth()));

  // Send event to swap users.
  ui::test::EventGenerator& generator = GetEventGenerator();
  LoginAuthUserView::TestApi secondary_test_api(test_api.opt_secondary_auth());
  generator.MoveMouseTo(
      secondary_test_api.user_view()->GetBoundsInScreen().CenterPoint());
  generator.ClickLeftButton();

  // User info is not swapped.
  EXPECT_EQ(
      primary_user,
      test_api.primary_auth()->current_user()->basic_user_info->account_id);
  EXPECT_EQ(secondary_user, test_api.opt_secondary_auth()
                                ->current_user()
                                ->basic_user_info->account_id);

  // Active auth user (ie, which user is showing password) is swapped.
  EXPECT_FALSE(has_auth(test_api.primary_auth()));
  EXPECT_TRUE(has_auth(test_api.opt_secondary_auth()));
}

// Ensures that when swapping from a user list, the entire user info is swapped.
TEST_F(LockContentsViewUnitTest, SwapUserListToPrimaryAuthUser) {
  // Build lock screen with five users.
  auto* contents = new LockContentsView(mojom::TrayActionState::kNotAvailable,
                                        data_dispatcher());
  LockContentsView::TestApi test_api(contents);
  SetUserCount(5);
  EXPECT_EQ(users().size() - 1, test_api.user_views().size());
  std::unique_ptr<views::Widget> widget = CreateWidgetWithContent(contents);

  LoginAuthUserView* auth_view = test_api.primary_auth();

  for (const LoginUserView* const list_user_view : test_api.user_views()) {
    // Capture user info to validate it did not change during the swap.
    AccountId auth_id = auth_view->current_user()->basic_user_info->account_id;
    AccountId list_user_id =
        list_user_view->current_user()->basic_user_info->account_id;
    EXPECT_NE(auth_id, list_user_id);

    // Send event to swap users.
    ui::test::EventGenerator& generator = GetEventGenerator();
    generator.MoveMouseTo(list_user_view->GetBoundsInScreen().CenterPoint());
    generator.ClickLeftButton();

    // User info is swapped.
    EXPECT_EQ(list_user_id,
              auth_view->current_user()->basic_user_info->account_id);
    EXPECT_EQ(auth_id,
              list_user_view->current_user()->basic_user_info->account_id);

    // Validate that every user is still unique.
    std::unordered_set<std::string> emails;
    for (const LoginUserView* const view : test_api.user_views()) {
      std::string email =
          view->current_user()->basic_user_info->account_id.GetUserEmail();
      EXPECT_TRUE(emails.insert(email).second);
    }
  }
}

// Verifies note action view bounds.
TEST_F(LockContentsViewUnitTest, NoteActionButtonBounds) {
  auto* contents = new LockContentsView(mojom::TrayActionState::kNotAvailable,
                                        data_dispatcher());
  SetUserCount(1);
  std::unique_ptr<views::Widget> widget = CreateWidgetWithContent(contents);

  LockContentsView::TestApi test_api(contents);

  // The note action button should not be visible if the note action is not
  // available.
  EXPECT_FALSE(test_api.note_action()->visible());

  // When the note action becomes available, the note action button should be
  // shown.
  data_dispatcher()->SetLockScreenNoteState(mojom::TrayActionState::kAvailable);
  EXPECT_TRUE(test_api.note_action()->visible());

  // Verify the bounds of the note action button are as expected.
  gfx::Rect widget_bounds = widget->GetWindowBoundsInScreen();
  gfx::Size note_action_size = test_api.note_action()->GetPreferredSize();
  EXPECT_EQ(gfx::Rect(widget_bounds.top_right() -
                          gfx::Vector2d(note_action_size.width(), 0),
                      note_action_size),
            test_api.note_action()->GetBoundsInScreen());

  // If the note action is disabled again, the note action button should be
  // hidden.
  data_dispatcher()->SetLockScreenNoteState(
      mojom::TrayActionState::kNotAvailable);
  EXPECT_FALSE(test_api.note_action()->visible());
}

// Verifies the note action view bounds when note action is available at lock
// contents view creation.
TEST_F(LockContentsViewUnitTest, NoteActionButtonBoundsInitiallyAvailable) {
  auto* contents = new LockContentsView(mojom::TrayActionState::kAvailable,
                                        data_dispatcher());
  SetUserCount(1);
  std::unique_ptr<views::Widget> widget = CreateWidgetWithContent(contents);

  LockContentsView::TestApi test_api(contents);

  // Verify the note action button is visible and positioned in the top rigth
  // corner of the screen.
  EXPECT_TRUE(test_api.note_action()->visible());
  gfx::Rect widget_bounds = widget->GetWindowBoundsInScreen();
  gfx::Size note_action_size = test_api.note_action()->GetPreferredSize();
  EXPECT_EQ(gfx::Rect(widget_bounds.top_right() -
                          gfx::Vector2d(note_action_size.width(), 0),
                      note_action_size),
            test_api.note_action()->GetBoundsInScreen());

  // If the note action is disabled, the note action button should be hidden.
  data_dispatcher()->SetLockScreenNoteState(
      mojom::TrayActionState::kNotAvailable);
  EXPECT_FALSE(test_api.note_action()->visible());
}

}  // namespace ash
