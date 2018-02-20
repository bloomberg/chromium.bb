// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <unordered_set>

#include "ash/login/ui/lock_contents_view.h"
#include "ash/login/ui/lock_screen.h"
#include "ash/login/ui/login_auth_user_view.h"
#include "ash/login/ui/login_bubble.h"
#include "ash/login/ui/login_display_style.h"
#include "ash/login/ui/login_keyboard_test_base.h"
#include "ash/login/ui/login_pin_view.h"
#include "ash/login/ui/login_test_base.h"
#include "ash/login/ui/login_user_view.h"
#include "ash/login/ui/scrollable_users_list_view.h"
#include "ash/public/interfaces/tray_action.mojom.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget.h"

namespace ash {

using LockContentsViewUnitTest = LoginTestBase;
using LockContentsViewKeyboardUnitTest = LoginKeyboardTestBase;

TEST_F(LockContentsViewUnitTest, DisplayMode) {
  // Build lock screen with 1 user.
  auto* contents = new LockContentsView(mojom::TrayActionState::kNotAvailable,
                                        data_dispatcher());
  SetUserCount(1);
  std::unique_ptr<views::Widget> widget = CreateWidgetWithContent(contents);

  // Verify user list and secondary auth are not shown for one user.
  LockContentsView::TestApi lock_contents(contents);
  EXPECT_EQ(nullptr, lock_contents.users_list());
  EXPECT_FALSE(lock_contents.opt_secondary_auth());

  // Verify user list is not shown for two users, but secondary auth is.
  SetUserCount(2);
  EXPECT_EQ(nullptr, lock_contents.users_list());
  EXPECT_TRUE(lock_contents.opt_secondary_auth());

  // Verify user names and pod style is set correctly for 3-25 users. This also
  // sanity checks that LockContentsView can respond to a multiple user change
  // events fired from the data dispatcher, which is needed for the debug UI.
  for (size_t user_count = 3; user_count < 25; ++user_count) {
    SetUserCount(user_count);
    ScrollableUsersListView::TestApi users_list(lock_contents.users_list());
    EXPECT_EQ(user_count - 1, users_list.user_views().size());

    // 1 extra user gets large style.
    LoginDisplayStyle expected_style = LoginDisplayStyle::kLarge;
    // 2-6 extra users get small style.
    if (user_count >= 3)
      expected_style = LoginDisplayStyle::kSmall;
    // 7+ users get get extra small style.
    if (user_count >= 7)
      expected_style = LoginDisplayStyle::kExtraSmall;

    for (size_t i = 0; i < users_list.user_views().size(); ++i) {
      LoginUserView::TestApi user_test_api(users_list.user_views()[i]);
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
  LockContentsView::TestApi lock_contents(contents);
  SetUserCount(3);
  std::unique_ptr<views::Widget> widget = CreateWidgetWithContent(contents);

  // Returns the distance between the auth user view and the user view.
  auto calculate_distance = [&]() {
    if (lock_contents.opt_secondary_auth()) {
      return lock_contents.opt_secondary_auth()->GetBoundsInScreen().x() -
             lock_contents.primary_auth()->GetBoundsInScreen().x();
    }
    ScrollableUsersListView::TestApi users_list(lock_contents.users_list());
    return users_list.user_views()[0]->GetBoundsInScreen().x() -
           lock_contents.primary_auth()->GetBoundsInScreen().x();
  };

  const display::Display& display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(
          widget->GetNativeWindow());
  for (int i = 2; i < 10; ++i) {
    SetUserCount(i);

    // Start at 0 degrees (landscape).
    display_manager()->SetDisplayRotation(
        display.id(), display::Display::ROTATE_0,
        display::Display::RotationSource::ACTIVE);
    int distance_0deg = calculate_distance();
    EXPECT_NE(distance_0deg, 0);

    // Rotate the display to 90 degrees (portrait).
    display_manager()->SetDisplayRotation(
        display.id(), display::Display::ROTATE_90,
        display::Display::RotationSource::ACTIVE);
    int distance_90deg = calculate_distance();
    EXPECT_GT(distance_0deg, distance_90deg);

    // Rotate the display back to 0 degrees (landscape).
    display_manager()->SetDisplayRotation(
        display.id(), display::Display::ROTATE_0,
        display::Display::RotationSource::ACTIVE);
    int distance_180deg = calculate_distance();
    EXPECT_EQ(distance_0deg, distance_180deg);
    EXPECT_NE(distance_0deg, distance_90deg);
  }
}

TEST_F(LockContentsViewUnitTest, AutoLayoutExtraSmallUsersListAfterRotation) {
  // Build lock screen with extra small layout (> 6 users).
  auto* contents = new LockContentsView(mojom::TrayActionState::kNotAvailable,
                                        data_dispatcher());
  SetUserCount(9);
  ScrollableUsersListView* users_list =
      LockContentsView::TestApi(contents).users_list();
  std::unique_ptr<views::Widget> widget = CreateWidgetWithContent(contents);

  // Users list in extra small layout should adjust its height to parent.
  EXPECT_EQ(contents->height(), users_list->height());

  const display::Display& display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(
          widget->GetNativeWindow());

  // Start at 0 degrees (landscape).
  display_manager()->SetDisplayRotation(
      display.id(), display::Display::ROTATE_0,
      display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(contents->height(), users_list->height());

  // Rotate the display to 90 degrees (portrait).
  display_manager()->SetDisplayRotation(
      display.id(), display::Display::ROTATE_90,
      display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(contents->height(), users_list->height());

  // Rotate the display back to 0 degrees (landscape).
  display_manager()->SetDisplayRotation(
      display.id(), display::Display::ROTATE_0,
      display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(contents->height(), users_list->height());
}

TEST_F(LockContentsViewUnitTest, AutoLayoutSmallUsersListAfterRotation) {
  // Build lock screen with small layout (3-6 users).
  auto* contents = new LockContentsView(mojom::TrayActionState::kNotAvailable,
                                        data_dispatcher());
  SetUserCount(4);
  ScrollableUsersListView* users_list =
      LockContentsView::TestApi(contents).users_list();
  std::unique_ptr<views::Widget> widget = CreateWidgetWithContent(contents);

  // Calculate top spacing between users list and lock screen contents.
  auto top_margin = [&]() {
    return users_list->GetBoundsInScreen().y() -
           contents->GetBoundsInScreen().y();
  };

  // Calculate bottom spacing between users list and lock screen contents.
  auto bottom_margin = [&]() {
    return contents->GetBoundsInScreen().bottom() -
           users_list->GetBoundsInScreen().bottom();
  };

  // Users list in small layout should adjust its height to content and be
  // vertical centered in parent.
  EXPECT_EQ(top_margin(), bottom_margin());
  EXPECT_EQ(users_list->height(), users_list->contents()->height());

  const display::Display& display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(
          widget->GetNativeWindow());

  // Start at 0 degrees (landscape).
  display_manager()->SetDisplayRotation(
      display.id(), display::Display::ROTATE_0,
      display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(top_margin(), bottom_margin());
  EXPECT_EQ(users_list->height(), users_list->contents()->height());

  // Rotate the display to 90 degrees (portrait).
  display_manager()->SetDisplayRotation(
      display.id(), display::Display::ROTATE_90,
      display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(top_margin(), bottom_margin());
  EXPECT_EQ(users_list->height(), users_list->contents()->height());

  // Rotate the display back to 0 degrees (landscape).
  display_manager()->SetDisplayRotation(
      display.id(), display::Display::ROTATE_0,
      display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(top_margin(), bottom_margin());
  EXPECT_EQ(users_list->height(), users_list->contents()->height());
}

TEST_F(LockContentsViewKeyboardUnitTest,
       AutoLayoutExtraSmallUsersListForKeyboard) {
  // Build lock screen with extra small layout (> 6 users).
  ASSERT_NO_FATAL_FAILURE(ShowLoginScreen());
  LockContentsView* contents =
      LockScreen::TestApi(LockScreen::Get()).contents_view();
  ASSERT_NE(nullptr, contents);
  LoadUsers(9);

  // Users list in extra small layout should adjust its height to parent.
  ScrollableUsersListView* users_list =
      LockContentsView::TestApi(contents).users_list();
  EXPECT_EQ(contents->height(), users_list->height());

  ASSERT_NO_FATAL_FAILURE(ShowKeyboard());
  gfx::Rect keyboard_bounds = GetKeyboardBoundsInScreen();
  EXPECT_FALSE(users_list->GetBoundsInScreen().Intersects(keyboard_bounds));
  EXPECT_EQ(contents->height(), users_list->height());

  ASSERT_NO_FATAL_FAILURE(HideKeyboard());
  EXPECT_EQ(contents->height(), users_list->height());
}

TEST_F(LockContentsViewKeyboardUnitTest, AutoLayoutSmallUsersListForKeyboard) {
  // Build lock screen with small layout (3-6 users).
  ASSERT_NO_FATAL_FAILURE(ShowLoginScreen());
  LockContentsView* contents =
      LockScreen::TestApi(LockScreen::Get()).contents_view();
  ASSERT_NE(nullptr, contents);
  LoadUsers(4);
  ScrollableUsersListView* users_list =
      LockContentsView::TestApi(contents).users_list();

  // Calculate top spacing between users list and lock screen contents.
  auto top_margin = [&]() {
    return users_list->GetBoundsInScreen().y() -
           contents->GetBoundsInScreen().y();
  };

  // Calculate bottom spacing between users list and lock screen contents.
  auto bottom_margin = [&]() {
    return contents->GetBoundsInScreen().bottom() -
           users_list->GetBoundsInScreen().bottom();
  };

  // Users list in small layout should adjust its height to content and be
  // vertical centered in parent.
  EXPECT_EQ(top_margin(), bottom_margin());
  EXPECT_EQ(users_list->height(), users_list->contents()->height());

  ASSERT_NO_FATAL_FAILURE(ShowKeyboard());
  gfx::Rect keyboard_bounds = GetKeyboardBoundsInScreen();
  EXPECT_FALSE(users_list->GetBoundsInScreen().Intersects(keyboard_bounds));
  EXPECT_EQ(top_margin(), bottom_margin());

  ASSERT_NO_FATAL_FAILURE(HideKeyboard());
  EXPECT_EQ(top_margin(), bottom_margin());
  EXPECT_EQ(users_list->height(), users_list->contents()->height());
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
  LockContentsView::TestApi lock_contents(contents);
  SetUserCount(5);
  ScrollableUsersListView::TestApi users_list(lock_contents.users_list());
  EXPECT_EQ(users().size() - 1, users_list.user_views().size());
  std::unique_ptr<views::Widget> widget = CreateWidgetWithContent(contents);

  LoginAuthUserView* auth_view = lock_contents.primary_auth();

  for (const LoginUserView* const list_user_view : users_list.user_views()) {
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
    for (const LoginUserView* const view : users_list.user_views()) {
      std::string email =
          view->current_user()->basic_user_info->account_id.GetUserEmail();
      EXPECT_TRUE(emails.insert(email).second);
    }
  }
}

// Test goes through different lock screen note state changes and tests that
// the note action visibility is updated accordingly.
TEST_F(LockContentsViewUnitTest, NoteActionButtonVisibilityChanges) {
  auto* contents = new LockContentsView(mojom::TrayActionState::kAvailable,
                                        data_dispatcher());
  SetUserCount(1);
  SetWidget(CreateWidgetWithContent(contents));

  LockContentsView::TestApi test_api(contents);
  views::View* note_action_button = test_api.note_action();

  // In kAvailable state, the note action button should be visible.
  EXPECT_TRUE(note_action_button->visible());

  // In kLaunching state, the note action button should not be visible.
  data_dispatcher()->SetLockScreenNoteState(mojom::TrayActionState::kLaunching);
  EXPECT_FALSE(note_action_button->visible());

  // In kActive state, the note action button should not be visible.
  data_dispatcher()->SetLockScreenNoteState(mojom::TrayActionState::kActive);
  EXPECT_FALSE(note_action_button->visible());

  // When moved back to kAvailable state, the note action button should become
  // visible again.
  data_dispatcher()->SetLockScreenNoteState(mojom::TrayActionState::kAvailable);
  EXPECT_TRUE(note_action_button->visible());

  // In kNotAvailable state, the note action button should not be visible.
  data_dispatcher()->SetLockScreenNoteState(
      mojom::TrayActionState::kNotAvailable);
  EXPECT_FALSE(note_action_button->visible());
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

  // Verify the note action button is visible and positioned in the top right
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

// Verifies the dev channel info view bounds.
TEST_F(LockContentsViewUnitTest, DevChannelInfoViewBounds) {
  auto* contents = new LockContentsView(mojom::TrayActionState::kAvailable,
                                        data_dispatcher());
  SetUserCount(1);

  std::unique_ptr<views::Widget> widget = CreateWidgetWithContent(contents);
  gfx::Rect widget_bounds = widget->GetWindowBoundsInScreen();
  LockContentsView::TestApi test_api(contents);
  // Verify that the dev channel info view is hidden by default.
  EXPECT_FALSE(test_api.dev_channel_info()->visible());

  // Verify that the dev channel info view becomes visible and it doesn't block
  // the note action button.
  data_dispatcher()->SetDevChannelInfo("Best version ever", "Asset ID: 6666",
                                       "Bluetooth adapter");
  EXPECT_TRUE(test_api.dev_channel_info()->visible());
  EXPECT_TRUE(test_api.note_action()->visible());
  gfx::Size note_action_size = test_api.note_action()->GetPreferredSize();
  EXPECT_GE(widget_bounds.right() -
                test_api.dev_channel_info()->GetBoundsInScreen().right(),
            note_action_size.width());

  // Verify that if the note action is disabled, the dev channel info view moves
  // to the right to fill the empty space.
  data_dispatcher()->SetLockScreenNoteState(
      mojom::TrayActionState::kNotAvailable);
  EXPECT_FALSE(test_api.note_action()->visible());
  EXPECT_LT(widget_bounds.right() -
                test_api.dev_channel_info()->GetBoundsInScreen().right(),
            note_action_size.width());
}

// Verifies the easy unlock tooltip is automatically displayed when requested.
TEST_F(LockContentsViewUnitTest, EasyUnlockForceTooltipCreatesTooltipWidget) {
  auto* lock = new LockContentsView(mojom::TrayActionState::kNotAvailable,
                                    data_dispatcher());

  SetUserCount(1);
  SetWidget(CreateWidgetWithContent(lock));

  LockContentsView::TestApi test_api(lock);
  // Creating lock screen does not show tooltip bubble.
  EXPECT_FALSE(test_api.tooltip_bubble()->IsVisible());

  // Show an icon with |autoshow_tooltip| is false. Tooltip bubble is not
  // activated.
  auto icon = mojom::EasyUnlockIconOptions::New();
  icon->icon = mojom::EasyUnlockIconId::LOCKED;
  icon->autoshow_tooltip = false;
  data_dispatcher()->ShowEasyUnlockIcon(users()[0]->basic_user_info->account_id,
                                        icon);
  EXPECT_FALSE(test_api.tooltip_bubble()->IsVisible());

  // Show icon with |autoshow_tooltip| set to true. Tooltip bubble is shown.
  icon->autoshow_tooltip = true;
  data_dispatcher()->ShowEasyUnlockIcon(users()[0]->basic_user_info->account_id,
                                        icon);
  EXPECT_TRUE(test_api.tooltip_bubble()->IsVisible());
}

// Verifies that easy unlock icon state persists when changing auth user.
TEST_F(LockContentsViewUnitTest, EasyUnlockIconUpdatedDuringUserSwap) {
  // Build lock screen with two users.
  auto* contents = new LockContentsView(mojom::TrayActionState::kNotAvailable,
                                        data_dispatcher());
  SetUserCount(2);
  SetWidget(CreateWidgetWithContent(contents));

  LockContentsView::TestApi test_api(contents);
  LoginAuthUserView* primary = test_api.primary_auth();
  LoginAuthUserView* secondary = test_api.opt_secondary_auth();

  // Returns true if the easy unlock icon is displayed for |view|.
  auto showing_easy_unlock_icon = [&](LoginAuthUserView* view) {
    views::View* icon = LoginPasswordView::TestApi(
                            LoginAuthUserView::TestApi(view).password_view())
                            .easy_unlock_icon();
    return icon->visible();
  };

  // Enables easy unlock icon for |view|.
  auto enable_icon = [&](LoginAuthUserView* view) {
    auto icon = mojom::EasyUnlockIconOptions::New();
    icon->icon = mojom::EasyUnlockIconId::LOCKED;
    data_dispatcher()->ShowEasyUnlockIcon(
        view->current_user()->basic_user_info->account_id, icon);
  };

  // Disables easy unlock icon for |view|.
  auto disable_icon = [&](LoginAuthUserView* view) {
    auto icon = mojom::EasyUnlockIconOptions::New();
    icon->icon = mojom::EasyUnlockIconId::NONE;
    data_dispatcher()->ShowEasyUnlockIcon(
        view->current_user()->basic_user_info->account_id, icon);
  };

  // Makes |view| the active auth view so it will can show auth methods.
  auto make_active_auth_view = [&](LoginAuthUserView* view) {
    // Send event to swap users.
    ui::test::EventGenerator& generator = GetEventGenerator();
    LoginUserView* user_view = LoginAuthUserView::TestApi(view).user_view();
    generator.MoveMouseTo(user_view->GetBoundsInScreen().CenterPoint());
    generator.ClickLeftButton();
  };

  // NOTE: we cannot assert on non-active auth views because the easy unlock
  // icon is lazily updated, ie, if we're not showing the view we will not
  // update icon state.

  // No easy unlock icons are shown.
  make_active_auth_view(primary);
  EXPECT_FALSE(showing_easy_unlock_icon(primary));

  // Activate icon for primary.
  enable_icon(primary);
  EXPECT_TRUE(showing_easy_unlock_icon(primary));

  // Secondary does not have easy unlock enabled; swapping auth hides the icon.
  make_active_auth_view(secondary);
  EXPECT_FALSE(showing_easy_unlock_icon(secondary));

  // Switching back enables the icon again.
  make_active_auth_view(primary);
  EXPECT_TRUE(showing_easy_unlock_icon(primary));

  // Activate icon for secondary. Primary visiblity does not change.
  enable_icon(secondary);
  EXPECT_TRUE(showing_easy_unlock_icon(primary));

  // Swap to secondary, icon still visible.
  make_active_auth_view(secondary);
  EXPECT_TRUE(showing_easy_unlock_icon(secondary));

  // Deactivate secondary, icon hides.
  disable_icon(secondary);
  EXPECT_FALSE(showing_easy_unlock_icon(secondary));

  // Deactivate primary, icon still hidden.
  disable_icon(primary);
  EXPECT_FALSE(showing_easy_unlock_icon(secondary));

  // Enable primary, icon still hidden.
  enable_icon(primary);
  EXPECT_FALSE(showing_easy_unlock_icon(secondary));
}

TEST_F(LockContentsViewKeyboardUnitTest, SwitchPinAndVirtualKeyboard) {
  ASSERT_NO_FATAL_FAILURE(ShowLockScreen());
  LockContentsView* contents =
      LockScreen::TestApi(LockScreen::Get()).contents_view();
  ASSERT_NE(nullptr, contents);

  // Add user with enabled pin method of authentication.
  const std::string email = "user@domain.com";
  LoadUser(email);
  contents->OnPinEnabledForUserChanged(AccountId::FromUserEmail(email), true);
  LoginAuthUserView* auth_view =
      LockContentsView::TestApi(contents).primary_auth();
  ASSERT_NE(nullptr, auth_view);

  // Pin keyboard should only be visible when there is no virtual keyboard
  // shown.
  LoginPinView* pin_view = LoginAuthUserView::TestApi(auth_view).pin_view();
  EXPECT_TRUE(pin_view->visible());

  ASSERT_NO_FATAL_FAILURE(ShowKeyboard());
  EXPECT_FALSE(pin_view->visible());

  ASSERT_NO_FATAL_FAILURE(HideKeyboard());
  EXPECT_TRUE(pin_view->visible());
}

}  // namespace ash
