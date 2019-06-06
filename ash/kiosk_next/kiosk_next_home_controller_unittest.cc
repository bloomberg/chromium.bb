// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/kiosk_next/kiosk_next_home_controller.h"

#include "ash/home_screen/home_screen_controller.h"
#include "ash/kiosk_next/kiosk_next_shell_test_util.h"
#include "ash/kiosk_next/mock_kiosk_next_shell_client.h"
#include "ash/public/cpp/app_types.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/window_state.h"
#include "base/test/scoped_feature_list.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/window_types.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/base/hit_test.h"
#include "ui/events/test/event_generator.h"

namespace ash {
namespace {

class KioskNextHomeControllerTest : public AshTestBase {
 public:
  KioskNextHomeControllerTest() = default;
  ~KioskNextHomeControllerTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kKioskNextShell);
    set_start_session(false);
    AshTestBase::SetUp();
    client_ = BindMockKioskNextShellClient();
    LogInKioskNextUser(GetSessionControllerClient());
    SetUpHomeWindow();
  }

  void TearDown() override {
    home_screen_window_.reset();
    AshTestBase::TearDown();
    client_.reset();
  }

  void SetUpHomeWindow() {
    auto* delegate =
        aura::test::TestWindowDelegate::CreateSelfDestroyingDelegate();

    home_screen_window_.reset(
        CreateTestWindowInShellWithDelegate(delegate, 0, gfx::Rect()));
    home_screen_window_->SetProperty(aura::client::kAppType,
                                     static_cast<int>(AppType::CHROME_APP));
    home_screen_window_->set_owned_by_parent(false);
    Shell::GetContainer(Shell::GetPrimaryRootWindow(),
                        kShellWindowId_HomeScreenContainer)
        ->AddChild(home_screen_window_.get());

    auto* window_state = wm::GetWindowState(home_screen_window_.get());
    window_state->Maximize();
    home_screen_window_->Show();
  }

  // TestWindowDelegate always returns its |window_component_| when
  // TestWindowDelegate::GetNonClientComponent(const gfx::Point& point) is
  // called, regardless of the location. Therefore individual tests have to set
  // the |window_component_|. KioskNextHomeController's event handler starts
  // overview only if the window component which the gesture event touches is
  // HTCLIENT.
  void SetWindowComponent(int component) {
    auto* delegate = static_cast<aura::test::TestWindowDelegate*>(
        home_screen_window_->delegate());
    delegate->set_window_component(component);
  }

 protected:
  std::unique_ptr<aura::Window> home_screen_window_;
  std::unique_ptr<MockKioskNextShellClient> client_;
  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  DISALLOW_COPY_AND_ASSIGN(KioskNextHomeControllerTest);
};

TEST_F(KioskNextHomeControllerTest, CheckWindows) {
  auto* kiosk_next_home_controller =
      Shell::Get()->home_screen_controller()->delegate();

  EXPECT_EQ(kiosk_next_home_controller->GetHomeScreenWindow(),
            home_screen_window_.get());
}

TEST_F(KioskNextHomeControllerTest, TestGestureToOverview) {
  SetWindowComponent(HTCLIENT);

  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());

  generator.GestureScrollSequence(gfx::Point(50, 0), gfx::Point(50, 20),
                                  base::TimeDelta::FromMilliseconds(10), 10);

  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());
}

TEST_F(KioskNextHomeControllerTest, TestGestureToNoOverview) {
  SetWindowComponent(HTNOWHERE);

  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());

  generator.GestureScrollSequence(gfx::Point(50, 0), gfx::Point(50, 20),
                                  base::TimeDelta::FromMilliseconds(10), 10);

  EXPECT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());
}

}  // namespace
}  // namespace ash
