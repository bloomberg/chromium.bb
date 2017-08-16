// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/app_list_button.h"

#include "ash/public/interfaces/session_controller.mojom.h"
#include "ash/session/session_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shelf/shelf_view_test_api.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/command_line.h"
#include "base/i18n/rtl.h"
#include "base/test/scoped_command_line.h"
#include "chromeos/chromeos_switches.h"
#include "components/user_manager/fake_user_manager.h"
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

    controller_ = base::MakeUnique<SessionController>(nullptr);
    controller_->AddObserver(app_list_button_);
    user_manager_ = base::MakeUnique<user_manager::FakeUserManager>();
    user_manager_->Initialize();
  }

  void TearDown() override {
    AshTestBase::TearDown();
    controller_->RemoveObserver(app_list_button_);
    user_manager_->Shutdown();
    user_manager_->Destroy();
  }

  void UpdateSession(uint32_t session_id, const std::string& email) {
    mojom::UserSessionPtr session = mojom::UserSession::New();
    session->session_id = session_id;
    session->user_info = mojom::UserInfo::New();
    session->user_info->type = user_manager::USER_TYPE_REGULAR;
    session->user_info->account_id = AccountId::FromUserEmail(email);
    session->user_info->display_name = email;
    session->user_info->display_email = email;

    controller_->UpdateUserSession(std::move(session));
    user_manager_->AddUser(AccountId::FromUserEmail(email));
    user_manager_->UserLoggedIn(AccountId::FromUserEmail(email), "", false);
  }

  virtual void SetupCommandLine(base::CommandLine* command_line) {}

  void SendGestureEvent(ui::GestureEvent* event) {
    app_list_button_->OnGestureEvent(event);
  }

  const AppListButton* app_list_button() const { return app_list_button_; }

  void ChangeActiveUserSession() {
    UpdateSession(1u, "user1@test.com");
    UpdateSession(2u, "user2@test.com");

    std::vector<uint32_t> order = {1u, 2u};
    controller_->SetUserSessionOrder(order);
  }

 private:
  AppListButton* app_list_button_;

  std::unique_ptr<base::test::ScopedCommandLine> command_line_;
  std::unique_ptr<SessionController> controller_;
  std::unique_ptr<user_manager::FakeUserManager> user_manager_;

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
  ChangeActiveUserSession();

  EXPECT_TRUE(base::CommandLine::ForCurrentProcess()->HasSwitch(
      chromeos::switches::kEnableVoiceInteraction));

  ui::GestureEvent long_press =
      CreateGestureEvent(ui::GestureEventDetails(ui::ET_GESTURE_LONG_PRESS));
  SendGestureEvent(&long_press);
  RunAllPendingInMessageLoop();
  EXPECT_EQ(1u, test_app_list_presenter.voice_session_count());
}

namespace {

class BackButtonAppListButtonTest : public AppListButtonTest,
                                    public testing::WithParamInterface<bool> {
 public:
  BackButtonAppListButtonTest() : is_rtl_(GetParam()) {}
  ~BackButtonAppListButtonTest() override {}

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
