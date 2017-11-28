// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/app_list_button.h"

#include <memory>
#include <string>

#include "ash/public/cpp/config.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shelf/shelf_view_test_api.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/voice_interaction/voice_interaction_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/command_line.h"
#include "base/i18n/rtl.h"
#include "base/test/scoped_command_line.h"
#include "chromeos/chromeos_switches.h"
#include "ui/app_list/presenter/app_list.h"
#include "ui/app_list/presenter/test/test_app_list_presenter.h"
#include "ui/events/event_constants.h"
#include "ui/events/test/event_generator.h"

namespace ash {

ui::GestureEvent CreateGestureEvent(ui::GestureEventDetails details) {
  return ui::GestureEvent(0, 0, ui::EF_NONE, base::TimeTicks(), details);
}

class AppListButtonTest : public AshTestBase {
 public:
  AppListButtonTest() = default;
  ~AppListButtonTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    app_list_button_ =
        GetPrimaryShelf()->GetShelfViewForTesting()->GetAppListButton();
    Shell::Get()->app_list()->SetAppListPresenter(
        test_app_list_presenter.CreateInterfacePtrAndBind());
  }

  void SendGestureEvent(ui::GestureEvent* event) {
    app_list_button_->OnGestureEvent(event);
    Shell::Get()->app_list()->FlushForTesting();
    RunAllPendingInMessageLoop();
  }

  void SendGestureEventToSecondaryDisplay(ui::GestureEvent* event) {
    // Add secondary display.
    UpdateDisplay("1+1-1000x600,1002+0-600x400");
    // Send the gesture event to the secondary display.
    Shelf::ForWindow(Shell::GetAllRootWindows()[1])
        ->GetShelfViewForTesting()
        ->GetAppListButton()
        ->OnGestureEvent(event);
    Shell::Get()->app_list()->FlushForTesting();
    RunAllPendingInMessageLoop();
  }

  const AppListButton* app_list_button() const { return app_list_button_; }

 protected:
  app_list::test::TestAppListPresenter test_app_list_presenter;

 private:
  AppListButton* app_list_button_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(AppListButtonTest);
};

TEST_F(AppListButtonTest, LongPressGestureWithoutVoiceInteractionFlag) {
  // Simulate two user with primary user as active.
  CreateUserSessions(2);

  // Enable voice interaction in system settings.
  Shell::Get()->voice_interaction_controller()->NotifySettingsEnabled(true);

  ui::GestureEvent long_press =
      CreateGestureEvent(ui::GestureEventDetails(ui::ET_GESTURE_LONG_PRESS));
  SendGestureEvent(&long_press);
  EXPECT_EQ(0u, test_app_list_presenter.voice_session_count());

  // Test long press gesture on secondary display.
  SendGestureEventToSecondaryDisplay(&long_press);
  EXPECT_EQ(0u, test_app_list_presenter.voice_session_count());
}

TEST_F(AppListButtonTest, SwipeUpToOpenFullscreenAppList) {
  Shelf* shelf = GetPrimaryShelf();
  EXPECT_EQ(SHELF_ALIGNMENT_BOTTOM, shelf->alignment());

  // Start the drags from the center of the app list button.
  gfx::Point start = app_list_button()->GetAppListButtonCenterPoint();
  views::View::ConvertPointToScreen(app_list_button(), &start);
  // Swiping up less than the threshold should trigger a peeking app list.
  gfx::Point end = start;
  end.set_y(shelf->GetIdealBounds().bottom() -
            ShelfLayoutManager::kAppListDragSnapToPeekingThreshold + 10);
  GetEventGenerator().GestureScrollSequence(
      start, end, base::TimeDelta::FromMilliseconds(100), 4 /* steps */);
  RunAllPendingInMessageLoop();
  Shell::Get()->app_list()->FlushForTesting();
  EXPECT_EQ(1u, test_app_list_presenter.show_count());
  EXPECT_GE(test_app_list_presenter.set_y_position_count(), 1u);
  EXPECT_EQ(app_list::mojom::AppListState::PEEKING,
            test_app_list_presenter.app_list_state());

  // Swiping above the threshold should trigger a fullscreen app list.
  end.set_y(shelf->GetIdealBounds().bottom() -
            ShelfLayoutManager::kAppListDragSnapToPeekingThreshold - 10);
  GetEventGenerator().GestureScrollSequence(
      start, end, base::TimeDelta::FromMilliseconds(100), 4 /* steps */);
  RunAllPendingInMessageLoop();
  Shell::Get()->app_list()->FlushForTesting();
  EXPECT_EQ(2u, test_app_list_presenter.show_count());
  EXPECT_GE(test_app_list_presenter.set_y_position_count(), 1u);
  EXPECT_EQ(app_list::mojom::AppListState::FULLSCREEN_ALL_APPS,
            test_app_list_presenter.app_list_state());
}

class VoiceInteractionAppListButtonTest : public AppListButtonTest {
 public:
  VoiceInteractionAppListButtonTest() = default;

  // AppListButtonTest:
  void SetUp() override {
    command_line_ = std::make_unique<base::test::ScopedCommandLine>();
    command_line_->GetProcessCommandLine()->AppendSwitch(
        chromeos::switches::kEnableVoiceInteraction);
    EXPECT_TRUE(chromeos::switches::IsVoiceInteractionFlagsEnabled());
    AppListButtonTest::SetUp();
  }

 private:
  std::unique_ptr<base::test::ScopedCommandLine> command_line_;
  DISALLOW_COPY_AND_ASSIGN(VoiceInteractionAppListButtonTest);
};

TEST_F(VoiceInteractionAppListButtonTest,
       LongPressGestureWithVoiceInteractionFlag) {
  // Simulate two user with primary user as active.
  CreateUserSessions(2);

  // Enable voice interaction in system settings.
  Shell::Get()->voice_interaction_controller()->NotifySettingsEnabled(true);

  ui::GestureEvent long_press =
      CreateGestureEvent(ui::GestureEventDetails(ui::ET_GESTURE_LONG_PRESS));
  SendGestureEvent(&long_press);
  EXPECT_EQ(1u, test_app_list_presenter.voice_session_count());

  // Test long press gesture on secondary display.
  SendGestureEventToSecondaryDisplay(&long_press);
  EXPECT_EQ(2u, test_app_list_presenter.voice_session_count());
}

TEST_F(VoiceInteractionAppListButtonTest, LongPressGestureWithSecondaryUser) {
  // Disallowed by secondary user.
  Shell::Get()->voice_interaction_controller()->NotifyFeatureAllowed(
      mojom::AssistantAllowedState::DISALLOWED_BY_NONPRIMARY_USER);

  // Enable voice interaction in system settings.
  Shell::Get()->voice_interaction_controller()->NotifySettingsEnabled(true);

  ui::GestureEvent long_press =
      CreateGestureEvent(ui::GestureEventDetails(ui::ET_GESTURE_LONG_PRESS));
  SendGestureEvent(&long_press);
  // Voice interaction is disabled for secondary user, so the count here should
  // be 0.
  EXPECT_EQ(0u, test_app_list_presenter.voice_session_count());

  // Test long press gesture on secondary display.
  SendGestureEventToSecondaryDisplay(&long_press);
  EXPECT_EQ(0u, test_app_list_presenter.voice_session_count());
}

TEST_F(VoiceInteractionAppListButtonTest,
       LongPressGestureWithSettingsDisabled) {
  // Simulate two user with primary user as active.
  CreateUserSessions(2);

  // Simulate a user who has already completed setup flow, but disabled voice
  // interaction in settings.
  Shell::Get()->voice_interaction_controller()->NotifySettingsEnabled(false);
  Shell::Get()->voice_interaction_controller()->NotifySetupCompleted(true);

  ui::GestureEvent long_press =
      CreateGestureEvent(ui::GestureEventDetails(ui::ET_GESTURE_LONG_PRESS));
  SendGestureEvent(&long_press);
  // After value prop has been accepted, if voice interaction is disalbed in
  // settings we should not handle long press action in app list button.
  EXPECT_EQ(0u, test_app_list_presenter.voice_session_count());

  // Test long press gesture on secondary display.
  SendGestureEventToSecondaryDisplay(&long_press);
  EXPECT_EQ(0u, test_app_list_presenter.voice_session_count());
}

TEST_F(VoiceInteractionAppListButtonTest,
       LongPressGestureBeforeSetupCompleted) {
  // Simulate two user with primary user as active.
  CreateUserSessions(2);

  // Disable voice interaction in system settings.
  Shell::Get()->voice_interaction_controller()->NotifySettingsEnabled(false);

  ui::GestureEvent long_press =
      CreateGestureEvent(ui::GestureEventDetails(ui::ET_GESTURE_LONG_PRESS));
  SendGestureEvent(&long_press);
  // Before setup flow completed we should show the animation even if the
  // settings are disabled.
  EXPECT_EQ(1u, test_app_list_presenter.voice_session_count());

  // Test long press gesture on secondary display.
  SendGestureEventToSecondaryDisplay(&long_press);
  EXPECT_EQ(2u, test_app_list_presenter.voice_session_count());
}

namespace {

class BackButtonAppListButtonTest : public AppListButtonTest,
                                    public testing::WithParamInterface<bool> {
 public:
  BackButtonAppListButtonTest() : is_rtl_(GetParam()) {}
  ~BackButtonAppListButtonTest() override = default;

  void SetUp() override {
    if (is_rtl_) {
      original_locale_ = base::i18n::GetConfiguredLocale();
      base::i18n::SetICUDefaultLocale("he");
    }
    AppListButtonTest::SetUp();
    ASSERT_EQ(is_rtl_, base::i18n::IsRTL());
  }

  void TearDown() override {
    if (is_rtl_)
      base::i18n::SetICUDefaultLocale(original_locale_);
    AppListButtonTest::TearDown();
  }

 private:
  bool is_rtl_ = false;
  std::string original_locale_;

  DISALLOW_COPY_AND_ASSIGN(BackButtonAppListButtonTest);
};

INSTANTIATE_TEST_CASE_P(
    /* prefix intentionally left blank due to only one parameterization */,
    BackButtonAppListButtonTest,
    testing::Bool());

}  // namespace

// Verify the locations of the back button and app list button.
TEST_P(BackButtonAppListButtonTest, BackButtonAppListButtonLocation) {
  ShelfViewTestAPI test_api(GetPrimaryShelf()->GetShelfViewForTesting());

  // Finish all setup tasks. In particular we want to finish the GetSwitchStates
  // post task in (Fake)PowerManagerClient which is triggered by
  // TabletModeController otherwise this will cause tablet mode to exit while we
  // wait for animations in the test.
  RunAllPendingInMessageLoop();

  Shell::Get()->tablet_mode_controller()->EnableTabletModeWindowManager(true);
  test_api.RunMessageLoopUntilAnimationsDone();

  gfx::Point back_button_center = app_list_button()->GetBackButtonCenterPoint();
  gfx::Point app_list_button_center =
      app_list_button()->GetAppListButtonCenterPoint();

  // Verify that in rtl, the app list button is left of the back button and vice
  // versa.
  if (base::i18n::IsRTL())
    EXPECT_LT(app_list_button_center.x(), back_button_center.x());
  else
    EXPECT_GT(app_list_button_center.x(), back_button_center.x());

  Shell::Get()->tablet_mode_controller()->EnableTabletModeWindowManager(false);
}

}  // namespace ash
