// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/app_list_button.h"

#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/command_line.h"
#include "base/test/scoped_command_line.h"
#include "chromeos/chromeos_switches.h"
#include "ui/app_list/presenter/app_list.h"
#include "ui/app_list/presenter/test/test_app_list_presenter.h"
#include "ui/events/event_constants.h"

namespace ash {

ui::GestureEvent CreateGestureEvent(ui::GestureEventDetails details) {
  return ui::GestureEvent(0, 0, ui::EF_NONE, base::TimeTicks(), details);
}

class AppListButtonTest : public AshTestBase {
 public:
  AppListButtonTest() {}
  ~AppListButtonTest() override {}

  void SetUp() override {
    command_line_ = base::MakeUnique<base::test::ScopedCommandLine>();
    SetupCommandLine(command_line_->GetProcessCommandLine());
    AshTestBase::SetUp();
    app_list_button_ =
        GetPrimaryShelf()->GetShelfViewForTesting()->GetAppListButton();
  }

  virtual void SetupCommandLine(base::CommandLine* command_line) {}

  void SendGestureEvent(ui::GestureEvent* event) {
    app_list_button_->OnGestureEvent(event);
  }

 private:
  AppListButton* app_list_button_;
  std::unique_ptr<base::test::ScopedCommandLine> command_line_;

  DISALLOW_COPY_AND_ASSIGN(AppListButtonTest);
};

TEST_F(AppListButtonTest, LongPressGestureWithoutVoiceInteractionFlag) {
  app_list::test::TestAppListPresenter test_app_list_presenter;
  Shell::Get()->app_list()->SetAppListPresenter(
      test_app_list_presenter.CreateInterfacePtrAndBind());
  ui::GestureEvent long_press =
      CreateGestureEvent(ui::GestureEventDetails(ui::ET_GESTURE_LONG_PRESS));
  SendGestureEvent(&long_press);
  RunAllPendingInMessageLoop();
  EXPECT_EQ(0u, test_app_list_presenter.voice_session_count());
}

class VoiceInteractionAppListButtonTest : public AppListButtonTest {
 public:
  VoiceInteractionAppListButtonTest() {}

  void SetupCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(chromeos::switches::kEnableVoiceInteraction);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(VoiceInteractionAppListButtonTest);
};

TEST_F(VoiceInteractionAppListButtonTest,
       LongPressGestureWithVoiceInteractionFlag) {
  app_list::test::TestAppListPresenter test_app_list_presenter;
  Shell::Get()->app_list()->SetAppListPresenter(
      test_app_list_presenter.CreateInterfacePtrAndBind());

  EXPECT_TRUE(base::CommandLine::ForCurrentProcess()->HasSwitch(
      chromeos::switches::kEnableVoiceInteraction));

  ui::GestureEvent long_press =
      CreateGestureEvent(ui::GestureEventDetails(ui::ET_GESTURE_LONG_PRESS));
  SendGestureEvent(&long_press);
  RunAllPendingInMessageLoop();
  EXPECT_EQ(1u, test_app_list_presenter.voice_session_count());
}

}  // namespace ash
