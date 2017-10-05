// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/note_action_launch_button.h"

#include <memory>
#include <vector>

#include "ash/login/ui/layout_util.h"
#include "ash/login/ui/login_test_base.h"
#include "ash/public/interfaces/tray_action.mojom.h"
#include "ash/shell.h"
#include "ash/tray_action/test_tray_action_client.h"
#include "base/macros.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// The note action button bubble sizes:
constexpr int kLargeButtonRadiusDp = 56;
constexpr int kSmallButtonRadiusDp = 48;

constexpr float kSqrt2 = 1.4142;

}  // namespace

class NoteActionLaunchButtonTest : public LoginTestBase {
 public:
  NoteActionLaunchButtonTest() = default;
  ~NoteActionLaunchButtonTest() override = default;

  void SetUp() override {
    LoginTestBase::SetUp();

    Shell::Get()->tray_action()->SetClient(
        tray_action_client_.CreateInterfacePtrAndBind(),
        mojom::TrayActionState::kAvailable);
  }

  TestTrayActionClient* tray_action_client() { return &tray_action_client_; }


  void PerformClick(const gfx::Point& point) {
    ui::test::EventGenerator& generator = GetEventGenerator();
    generator.MoveMouseTo(point.x(), point.y());
    generator.ClickLeftButton();

    Shell::Get()->tray_action()->FlushMojoForTesting();
  }

 private:
  std::unique_ptr<LoginDataDispatcher> data_dispatcher_;
  TestTrayActionClient tray_action_client_;

  DISALLOW_COPY_AND_ASSIGN(NoteActionLaunchButtonTest);
};

// Verifies that note action button is not visible if lock screen note taking
// is not enabled.
TEST_F(NoteActionLaunchButtonTest, VisibilityActionNotAvailable) {
  auto note_action_button = std::make_unique<NoteActionLaunchButton>(
      mojom::TrayActionState::kNotAvailable, data_dispatcher());
  EXPECT_FALSE(note_action_button->visible());
}

// Verifies that note action button is shown and enabled if lock screen note
// taking is available.
TEST_F(NoteActionLaunchButtonTest, VisibilityActionAvailable) {
  auto note_action_button = std::make_unique<NoteActionLaunchButton>(
      mojom::TrayActionState::kAvailable, data_dispatcher());
  NoteActionLaunchButton::TestApi test_api(note_action_button.get());

  EXPECT_TRUE(note_action_button->visible());
  EXPECT_TRUE(note_action_button->enabled());

  EXPECT_TRUE(test_api.ActionButtonView()->visible());
  EXPECT_TRUE(test_api.ActionButtonView()->enabled());
  EXPECT_TRUE(test_api.BackgroundView()->visible());
}

// Test goes through different lock screen note state changes and tests that
// the note action visibility is updated accordingly.
TEST_F(NoteActionLaunchButtonTest, StateChanges) {
  auto* note_action_button = new NoteActionLaunchButton(
      mojom::TrayActionState::kAvailable, data_dispatcher());
  std::unique_ptr<views::Widget> widget = CreateWidgetWithContent(
      login_layout_util::WrapViewForPreferredSize(note_action_button));
  NoteActionLaunchButton::TestApi test_api(note_action_button);

  // In kAvailable state, the action button should be visible.
  EXPECT_TRUE(note_action_button->visible());
  EXPECT_TRUE(test_api.ActionButtonView()->visible());
  EXPECT_TRUE(test_api.BackgroundView()->visible());
  EXPECT_EQ(gfx::Rect(gfx::Point(),
                      gfx::Size(kLargeButtonRadiusDp, kLargeButtonRadiusDp)),
            note_action_button->GetBoundsInScreen());

  // In kLaunching state, the action button should not be visible.
  data_dispatcher()->SetLockScreenNoteState(mojom::TrayActionState::kLaunching);
  EXPECT_FALSE(note_action_button->visible());

  // In kActive state, the action button should not be visible.
  data_dispatcher()->SetLockScreenNoteState(mojom::TrayActionState::kActive);
  EXPECT_FALSE(note_action_button->visible());

  // When moved back to kAvailable state, the action button should become
  // visible again.
  data_dispatcher()->SetLockScreenNoteState(mojom::TrayActionState::kAvailable);
  EXPECT_TRUE(note_action_button->visible());
  EXPECT_EQ(gfx::Rect(gfx::Point(),
                      gfx::Size(kLargeButtonRadiusDp, kLargeButtonRadiusDp)),
            note_action_button->GetBoundsInScreen());

  // In kNotAvailable state, the action button should not be visible.
  data_dispatcher()->SetLockScreenNoteState(
      mojom::TrayActionState::kNotAvailable);
  EXPECT_FALSE(note_action_button->visible());
}

// Tests that clicking Enter while lock screen action button is focused requests
// a new note action.
TEST_F(NoteActionLaunchButtonTest, KeyboardTest) {
  auto* note_action_button = new NoteActionLaunchButton(
      mojom::TrayActionState::kAvailable, data_dispatcher());
  std::unique_ptr<views::Widget> widget = CreateWidgetWithContent(
      login_layout_util::WrapViewForPreferredSize(note_action_button));
  NoteActionLaunchButton::TestApi test_api(note_action_button);

  note_action_button->RequestFocus();
  // Focusing the whole note action launch button view should give the image
  // button sub-view the focus.
  EXPECT_TRUE(test_api.ActionButtonView()->HasFocus());

  ui::test::EventGenerator& generator = GetEventGenerator();
  generator.PressKey(ui::KeyboardCode::VKEY_RETURN, ui::EF_NONE);
  Shell::Get()->tray_action()->FlushMojoForTesting();

  EXPECT_EQ(std::vector<mojom::LockScreenNoteOrigin>(
                {mojom::LockScreenNoteOrigin::kLockScreenButtonKeyboard}),
            tray_action_client()->note_origins());
}

// The button hit area is expected to be a circle centered in the top right
// corner of the view with kSmallButtonRadiusDp (and clipped but the view
// bounds). The test verifies clicking the button within the button's hit area
// requests a new note action.
TEST_F(NoteActionLaunchButtonTest, ClickTest) {
  auto* note_action_button = new NoteActionLaunchButton(
      mojom::TrayActionState::kAvailable, data_dispatcher());
  std::unique_ptr<views::Widget> widget = CreateWidgetWithContent(
      login_layout_util::WrapViewForPreferredSize(note_action_button));

  const gfx::Size action_size = note_action_button->GetPreferredSize();
  EXPECT_EQ(gfx::Size(kLargeButtonRadiusDp, kLargeButtonRadiusDp), action_size);
  ASSERT_EQ(gfx::Rect(gfx::Point(), action_size),
            note_action_button->GetBoundsInScreen());

  const gfx::Rect view_bounds = note_action_button->GetBoundsInScreen();
  const std::vector<mojom::LockScreenNoteOrigin> expected_actions = {
      mojom::LockScreenNoteOrigin::kLockScreenButtonTap};

  // Point near the center of the view, inside the actionable area:
  PerformClick(view_bounds.top_right() +
               gfx::Vector2d(-kSmallButtonRadiusDp / kSqrt2 + 2,
                             kSmallButtonRadiusDp / kSqrt2 - 2));
  EXPECT_EQ(expected_actions, tray_action_client()->note_origins());
  tray_action_client()->ClearRecordedRequests();

  // Point near the center of the view, outside the actionable area:
  PerformClick(view_bounds.top_right() +
               gfx::Vector2d(-kSmallButtonRadiusDp / kSqrt2 - 2,
                             kSmallButtonRadiusDp / kSqrt2 + 2));
  EXPECT_TRUE(tray_action_client()->note_origins().empty());
  tray_action_client()->ClearRecordedRequests();

  // Point near the top right corner:
  PerformClick(view_bounds.top_right() + gfx::Vector2d(-2, 2));
  EXPECT_EQ(expected_actions, tray_action_client()->note_origins());
  tray_action_client()->ClearRecordedRequests();

  // Point near the bottom left corner:
  PerformClick(view_bounds.bottom_left() + gfx::Vector2d(2, -2));
  EXPECT_TRUE(tray_action_client()->note_origins().empty());
  tray_action_client()->ClearRecordedRequests();

  // Point near the origin:
  PerformClick(view_bounds.origin() + gfx::Vector2d(2, 2));
  EXPECT_TRUE(tray_action_client()->note_origins().empty());
  tray_action_client()->ClearRecordedRequests();

  // Point near the origin of the actionable area bounds (inside the bounds):
  PerformClick(view_bounds.top_right() +
               gfx::Vector2d(-kSmallButtonRadiusDp + 2, 2));
  EXPECT_EQ(expected_actions, tray_action_client()->note_origins());
  tray_action_client()->ClearRecordedRequests();

  // Point near the origin of the actionable area bounds (outside the bounds):
  PerformClick(view_bounds.top_right() +
               gfx::Vector2d(-kSmallButtonRadiusDp - 2, 2));
  EXPECT_TRUE(tray_action_client()->note_origins().empty());
  tray_action_client()->ClearRecordedRequests();

  // Point near the bottom right corner:
  PerformClick(view_bounds.bottom_right() + gfx::Vector2d(0, -2));
  EXPECT_TRUE(tray_action_client()->note_origins().empty());
  tray_action_client()->ClearRecordedRequests();

  // Point near the bottom right corner of the actionable area bounds (inside
  // the bounds):
  PerformClick(view_bounds.top_right() +
               gfx::Vector2d(-2, kSmallButtonRadiusDp - 2));
  EXPECT_EQ(expected_actions, tray_action_client()->note_origins());
  tray_action_client()->ClearRecordedRequests();

  // Point near the bottom right corner of the actionable area bounds (outside
  // the bounds):
  PerformClick(view_bounds.top_right() +
               gfx::Vector2d(-2, kSmallButtonRadiusDp + 2));
  EXPECT_TRUE(tray_action_client()->note_origins().empty());
  tray_action_client()->ClearRecordedRequests();

  // Point near the bottom edge:
  PerformClick(view_bounds.bottom_left() +
               gfx::Vector2d(kSmallButtonRadiusDp / 2, -1));
  EXPECT_TRUE(tray_action_client()->note_origins().empty());
  tray_action_client()->ClearRecordedRequests();

  // Point near the top edge:
  PerformClick(view_bounds.origin() +
               gfx::Vector2d(kSmallButtonRadiusDp / 2, 1));
  EXPECT_EQ(expected_actions, tray_action_client()->note_origins());
  tray_action_client()->ClearRecordedRequests();

  // Point near the left edge:
  PerformClick(view_bounds.origin() +
               gfx::Vector2d(1, kSmallButtonRadiusDp / 2));
  EXPECT_TRUE(tray_action_client()->note_origins().empty());
  tray_action_client()->ClearRecordedRequests();

  // Point near the right edge:
  PerformClick(view_bounds.top_right() +
               gfx::Vector2d(-1, kSmallButtonRadiusDp / 2));
  EXPECT_EQ(expected_actions, tray_action_client()->note_origins());
  tray_action_client()->ClearRecordedRequests();

  // Point in the center of the actionable area:
  PerformClick(
      view_bounds.top_right() +
      gfx::Vector2d(-kSmallButtonRadiusDp / 2, kSmallButtonRadiusDp / 2));
  EXPECT_EQ(expected_actions, tray_action_client()->note_origins());
  tray_action_client()->ClearRecordedRequests();

  // Point outside the view bounds:
  PerformClick(view_bounds.top_right() + gfx::Vector2d(2, 2));
  EXPECT_TRUE(tray_action_client()->note_origins().empty());
  tray_action_client()->ClearRecordedRequests();
}

}  // namespace ash
