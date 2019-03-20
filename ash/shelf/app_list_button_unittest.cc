// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/app_list_button.h"

#include <memory>
#include <string>

#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/assistant/assistant_controller.h"
#include "ash/assistant/assistant_ui_controller.h"
#include "ash/assistant/model/assistant_ui_model.h"
#include "ash/assistant/test/test_assistant_service.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shelf/shelf_view_test_api.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/voice_interaction/voice_interaction_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/test/scoped_command_line.h"
#include "chromeos/constants/chromeos_switches.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
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
  }

  void SendGestureEvent(ui::GestureEvent* event) {
    app_list_button_->OnGestureEvent(event);
  }

  void SendGestureEventToSecondaryDisplay(ui::GestureEvent* event) {
    // Add secondary display.
    UpdateDisplay("1+1-1000x600,1002+0-600x400");
    // Send the gesture event to the secondary display.
    Shelf::ForWindow(Shell::GetAllRootWindows()[1])
        ->GetShelfViewForTesting()
        ->GetAppListButton()
        ->OnGestureEvent(event);
  }

  const AppListButton* app_list_button() const { return app_list_button_; }

 private:
  AppListButton* app_list_button_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(AppListButtonTest);
};

TEST_F(AppListButtonTest, SwipeUpToOpenFullscreenAppList) {
  Shelf* shelf = GetPrimaryShelf();
  EXPECT_EQ(SHELF_ALIGNMENT_BOTTOM, shelf->alignment());

  // Start the drags from the center of the app list button.
  gfx::Point start = app_list_button()->GetCenterPoint();
  views::View::ConvertPointToScreen(app_list_button(), &start);
  // Swiping up less than the threshold should trigger a peeking app list.
  gfx::Point end = start;
  end.set_y(shelf->GetIdealBounds().bottom() -
            app_list::AppListView::kDragSnapToPeekingThreshold + 10);
  GetEventGenerator()->GestureScrollSequence(
      start, end, base::TimeDelta::FromMilliseconds(100), 4 /* steps */);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(true);
  GetAppListTestHelper()->CheckState(app_list::AppListViewState::PEEKING);

  // Closing the app list.
  GetAppListTestHelper()->DismissAndRunLoop();
  GetAppListTestHelper()->CheckVisibility(false);
  GetAppListTestHelper()->CheckState(app_list::AppListViewState::CLOSED);

  // Swiping above the threshold should trigger a fullscreen app list.
  end.set_y(shelf->GetIdealBounds().bottom() -
            app_list::AppListView::kDragSnapToPeekingThreshold - 10);
  GetEventGenerator()->GestureScrollSequence(
      start, end, base::TimeDelta::FromMilliseconds(100), 4 /* steps */);
  base::RunLoop().RunUntilIdle();
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(true);
  GetAppListTestHelper()->CheckState(
      app_list::AppListViewState::FULLSCREEN_ALL_APPS);
}

TEST_F(AppListButtonTest, ClickToOpenAppList) {
  Shelf* shelf = GetPrimaryShelf();
  EXPECT_EQ(SHELF_ALIGNMENT_BOTTOM, shelf->alignment());

  gfx::Point center = app_list_button()->GetCenterPoint();
  views::View::ConvertPointToScreen(app_list_button(), &center);
  GetEventGenerator()->MoveMouseTo(center);

  // Click on the app list button should toggle the app list.
  GetEventGenerator()->ClickLeftButton();
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(true);
  GetAppListTestHelper()->CheckState(app_list::AppListViewState::PEEKING);
  GetEventGenerator()->ClickLeftButton();
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(false);
  GetAppListTestHelper()->CheckState(app_list::AppListViewState::CLOSED);

  // Shift-click should open the app list in fullscreen.
  GetEventGenerator()->set_flags(ui::EF_SHIFT_DOWN);
  GetEventGenerator()->ClickLeftButton();
  GetEventGenerator()->set_flags(0);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(true);
  GetAppListTestHelper()->CheckState(
      app_list::AppListViewState::FULLSCREEN_ALL_APPS);

  // Another shift-click should close the app list.
  GetEventGenerator()->set_flags(ui::EF_SHIFT_DOWN);
  GetEventGenerator()->ClickLeftButton();
  GetEventGenerator()->set_flags(0);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(false);
  GetAppListTestHelper()->CheckState(app_list::AppListViewState::CLOSED);
}

TEST_F(AppListButtonTest, ButtonPositionInTabletMode) {
  // Finish all setup tasks. In particular we want to finish the
  // GetSwitchStates post task in (Fake)PowerManagerClient which is triggered
  // by TabletModeController otherwise this will cause tablet mode to exit
  // while we wait for animations in the test.
  base::RunLoop().RunUntilIdle();

  ShelfViewTestAPI test_api(GetPrimaryShelf()->GetShelfViewForTesting());
  Shell::Get()->tablet_mode_controller()->EnableTabletModeWindowManager(true);
  test_api.RunMessageLoopUntilAnimationsDone();
  EXPECT_GT(app_list_button()->bounds().x(), 0);

  Shell::Get()->tablet_mode_controller()->EnableTabletModeWindowManager(false);
  test_api.RunMessageLoopUntilAnimationsDone();
  EXPECT_EQ(0, app_list_button()->bounds().x());
}

class VoiceInteractionAppListButtonTest : public AppListButtonTest {
 public:
  VoiceInteractionAppListButtonTest() {
    feature_list_.InitAndEnableFeature(chromeos::switches::kAssistantFeature);
  }

  // AppListButtonTest:
  void SetUp() override {
    AppListButtonTest::SetUp();

    Shell::Get()->assistant_controller()->SetAssistant(
        assistant_.CreateInterfacePtrAndBind());
  }

 private:
  std::unique_ptr<base::test::ScopedCommandLine> command_line_;
  base::test::ScopedFeatureList feature_list_;
  TestAssistantService assistant_;

  DISALLOW_COPY_AND_ASSIGN(VoiceInteractionAppListButtonTest);
};

TEST_F(VoiceInteractionAppListButtonTest,
       LongPressGestureWithVoiceInteractionFlag) {
  ui::ScopedAnimationDurationScaleMode animation_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  // Simulate two user with primary user as active.
  CreateUserSessions(2);

  // Enable voice interaction in system settings.
  Shell::Get()->voice_interaction_controller()->NotifySettingsEnabled(true);
  Shell::Get()->voice_interaction_controller()->NotifyFeatureAllowed(
      mojom::AssistantAllowedState::ALLOWED);
  Shell::Get()->voice_interaction_controller()->NotifyStatusChanged(
      mojom::VoiceInteractionState::STOPPED);

  ui::GestureEvent long_press =
      CreateGestureEvent(ui::GestureEventDetails(ui::ET_GESTURE_LONG_PRESS));
  SendGestureEvent(&long_press);
  GetAppListTestHelper()->WaitUntilIdle();
  EXPECT_EQ(AssistantVisibility::kVisible, Shell::Get()
                                               ->assistant_controller()
                                               ->ui_controller()
                                               ->model()
                                               ->visibility());

  Shell::Get()->assistant_controller()->ui_controller()->CloseUi(
      AssistantExitPoint::kUnspecified);
  // Test long press gesture on secondary display.
  SendGestureEventToSecondaryDisplay(&long_press);
  GetAppListTestHelper()->WaitUntilIdle();
  EXPECT_EQ(AssistantVisibility::kVisible, Shell::Get()
                                               ->assistant_controller()
                                               ->ui_controller()
                                               ->model()
                                               ->visibility());
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
  // Voice interaction is disabled for secondary user.
  EXPECT_NE(AssistantVisibility::kVisible, Shell::Get()
                                               ->assistant_controller()
                                               ->ui_controller()
                                               ->model()
                                               ->visibility());

  // Test long press gesture on secondary display.
  SendGestureEventToSecondaryDisplay(&long_press);
  EXPECT_NE(AssistantVisibility::kVisible, Shell::Get()
                                               ->assistant_controller()
                                               ->ui_controller()
                                               ->model()
                                               ->visibility());
}

TEST_F(VoiceInteractionAppListButtonTest,
       LongPressGestureWithSettingsDisabled) {
  // Simulate two user with primary user as active.
  CreateUserSessions(2);

  // Simulate a user who has already completed setup flow, but disabled voice
  // interaction in settings.
  Shell::Get()->voice_interaction_controller()->NotifySettingsEnabled(false);
  Shell::Get()->voice_interaction_controller()->NotifyFeatureAllowed(
      mojom::AssistantAllowedState::ALLOWED);

  ui::GestureEvent long_press =
      CreateGestureEvent(ui::GestureEventDetails(ui::ET_GESTURE_LONG_PRESS));
  SendGestureEvent(&long_press);
  EXPECT_NE(AssistantVisibility::kVisible, Shell::Get()
                                               ->assistant_controller()
                                               ->ui_controller()
                                               ->model()
                                               ->visibility());

  // Test long press gesture on secondary display.
  SendGestureEventToSecondaryDisplay(&long_press);
  EXPECT_NE(AssistantVisibility::kVisible, Shell::Get()
                                               ->assistant_controller()
                                               ->ui_controller()
                                               ->model()
                                               ->visibility());
}

}  // namespace ash
