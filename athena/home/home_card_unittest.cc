// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/home/public/home_card.h"

#include "athena/activity/public/activity_factory.h"
#include "athena/home/home_card_constants.h"
#include "athena/home/home_card_impl.h"
#include "athena/test/athena_test_base.h"
#include "athena/wm/public/window_manager.h"
#include "ui/aura/window.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/display.h"
#include "ui/gfx/screen.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/shadow_types.h"
#include "ui/wm/core/window_util.h"

namespace athena {

aura::Window* GetHomeCardWindow() {
  return static_cast<HomeCardImpl*>(HomeCard::Get())->
      GetHomeCardWindowForTest();
}

// Returns true if the keyboard focus is on the search box.
bool IsSearchBoxFocused(aura::Window* home_card) {
  return views::Widget::GetWidgetForNativeWindow(home_card)->
      GetContentsView()->GetViewByID(kHomeCardSearchBoxId)->HasFocus();
}

typedef test::AthenaTestBase HomeCardTest;

TEST_F(HomeCardTest, BasicTransition) {
  ASSERT_EQ(HomeCard::VISIBLE_MINIMIZED, HomeCard::Get()->GetState());
  aura::Window* home_card = GetHomeCardWindow();
  const int screen_height = root_window()->bounds().height();
  const int work_area_height = gfx::Screen::GetScreenFor(root_window())->
      GetDisplayNearestWindow(root_window()).work_area().height();
  ASSERT_TRUE(home_card);

  // In the minimized state, home card should be outside (below) the work area.
  EXPECT_EQ(screen_height - kHomeCardMinimizedHeight,
            home_card->GetTargetBounds().y());
  EXPECT_EQ(work_area_height, home_card->GetTargetBounds().y());
  EXPECT_EQ(wm::ShadowType::SHADOW_TYPE_NONE, wm::GetShadowType(home_card));

  WindowManager::Get()->ToggleOverview();
  EXPECT_EQ(HomeCard::VISIBLE_BOTTOM, HomeCard::Get()->GetState());
  EXPECT_EQ(screen_height - kHomeCardHeight, home_card->GetTargetBounds().y());
  EXPECT_EQ(wm::ShadowType::SHADOW_TYPE_RECTANGULAR,
            wm::GetShadowType(home_card));

  WindowManager::Get()->ToggleOverview();
  EXPECT_EQ(HomeCard::VISIBLE_MINIMIZED, HomeCard::Get()->GetState());
  EXPECT_EQ(work_area_height, home_card->GetTargetBounds().y());
  EXPECT_EQ(wm::ShadowType::SHADOW_TYPE_NONE, wm::GetShadowType(home_card));
}

TEST_F(HomeCardTest, VirtualKeyboardTransition) {
  // Minimized -> Hidden for virtual keyboard.
  EXPECT_EQ(HomeCard::VISIBLE_MINIMIZED, HomeCard::Get()->GetState());
  const gfx::Rect vk_bounds(0, 0, 100, 100);
  HomeCard::Get()->UpdateVirtualKeyboardBounds(vk_bounds);
  EXPECT_EQ(HomeCard::HIDDEN, HomeCard::Get()->GetState());
  HomeCard::Get()->UpdateVirtualKeyboardBounds(gfx::Rect());
  EXPECT_EQ(HomeCard::VISIBLE_MINIMIZED, HomeCard::Get()->GetState());

  // bottom -> centered for virtual keyboard.
  WindowManager::Get()->ToggleOverview();
  EXPECT_EQ(HomeCard::VISIBLE_BOTTOM, HomeCard::Get()->GetState());
  HomeCard::Get()->UpdateVirtualKeyboardBounds(vk_bounds);
  EXPECT_EQ(HomeCard::VISIBLE_CENTERED, HomeCard::Get()->GetState());

  aura::Window* home_card = GetHomeCardWindow();
  EXPECT_EQ(0, home_card->GetTargetBounds().y());
  EXPECT_EQ(wm::ShadowType::SHADOW_TYPE_RECTANGULAR,
            wm::GetShadowType(home_card));

  HomeCard::Get()->UpdateVirtualKeyboardBounds(gfx::Rect());
  EXPECT_EQ(HomeCard::VISIBLE_BOTTOM, HomeCard::Get()->GetState());
}

TEST_F(HomeCardTest, ToggleOverviewWithVirtualKeyboard) {
  // Minimized -> Hidden for virtual keyboard.
  EXPECT_EQ(HomeCard::VISIBLE_MINIMIZED, HomeCard::Get()->GetState());
  const gfx::Rect vk_bounds(0, 0, 100, 100);
  HomeCard::Get()->UpdateVirtualKeyboardBounds(vk_bounds);
  EXPECT_EQ(HomeCard::HIDDEN, HomeCard::Get()->GetState());

  // Toogle overview revives the bottom home card. Home card also gets
  /// activated which will close the virtual keyboard.
  WindowManager::Get()->ToggleOverview();
  EXPECT_EQ(HomeCard::VISIBLE_BOTTOM, HomeCard::Get()->GetState());
  aura::Window* home_card = GetHomeCardWindow();
  EXPECT_TRUE(wm::IsActiveWindow(home_card));
}

// Verify if the home card is correctly minimized after app launch.
TEST_F(HomeCardTest, AppSelection) {
  EXPECT_EQ(HomeCard::VISIBLE_MINIMIZED, HomeCard::Get()->GetState());

  WindowManager::Get()->ToggleOverview();
  EXPECT_EQ(HomeCard::VISIBLE_BOTTOM, HomeCard::Get()->GetState());

  athena::ActivityFactory::Get()->CreateWebActivity(
      NULL, base::string16(), GURL("http://www.google.com/"));
  EXPECT_EQ(HomeCard::VISIBLE_MINIMIZED, HomeCard::Get()->GetState());
}

TEST_F(HomeCardTest, Accelerators) {
  EXPECT_EQ(HomeCard::VISIBLE_MINIMIZED, HomeCard::Get()->GetState());

  ui::test::EventGenerator generator(root_window());
  generator.PressKey(ui::VKEY_L, ui::EF_CONTROL_DOWN);
  EXPECT_EQ(HomeCard::VISIBLE_CENTERED, HomeCard::Get()->GetState());

  generator.PressKey(ui::VKEY_L, ui::EF_CONTROL_DOWN);
  EXPECT_EQ(HomeCard::VISIBLE_MINIMIZED, HomeCard::Get()->GetState());

  // Do nothing for BOTTOM.
  WindowManager::Get()->ToggleOverview();
  EXPECT_EQ(HomeCard::VISIBLE_BOTTOM, HomeCard::Get()->GetState());
  generator.PressKey(ui::VKEY_L, ui::EF_CONTROL_DOWN);
  EXPECT_EQ(HomeCard::VISIBLE_BOTTOM, HomeCard::Get()->GetState());

  // Do nothing if the centered state is a temporary state.
  HomeCard::Get()->UpdateVirtualKeyboardBounds(gfx::Rect(0, 0, 100, 100));
  EXPECT_EQ(HomeCard::VISIBLE_CENTERED, HomeCard::Get()->GetState());
  generator.PressKey(ui::VKEY_L, ui::EF_CONTROL_DOWN);
  EXPECT_EQ(HomeCard::VISIBLE_CENTERED, HomeCard::Get()->GetState());
}

TEST_F(HomeCardTest, MouseClick) {
  ASSERT_EQ(HomeCard::VISIBLE_MINIMIZED, HomeCard::Get()->GetState());

  // Mouse click at the bottom of the screen should invokes overview mode and
  // changes the state to BOTTOM.
  gfx::Rect screen_rect(root_window()->bounds());
  ui::test::EventGenerator generator(
      root_window(), gfx::Point(
          screen_rect.x() + screen_rect.width() / 2, screen_rect.bottom() - 1));
  generator.ClickLeftButton();

  EXPECT_EQ(HomeCard::VISIBLE_BOTTOM, HomeCard::Get()->GetState());
  EXPECT_TRUE(WindowManager::Get()->IsOverviewModeActive());

  // Further clicks are simply ignored.
  generator.ClickLeftButton();
  EXPECT_EQ(HomeCard::VISIBLE_BOTTOM, HomeCard::Get()->GetState());
  EXPECT_TRUE(WindowManager::Get()->IsOverviewModeActive());
}

TEST_F(HomeCardTest, Gestures) {
  ASSERT_EQ(HomeCard::VISIBLE_MINIMIZED, HomeCard::Get()->GetState());
  ui::test::EventGenerator generator(root_window());
  gfx::Rect screen_rect(root_window()->bounds());

  const int bottom = screen_rect.bottom();
  const int x = screen_rect.x() + 1;

  generator.GestureScrollSequence(gfx::Point(x, bottom - 1),
                                  gfx::Point(x, bottom - 70),
                                  base::TimeDelta::FromSeconds(1),
                                  10);
  EXPECT_EQ(HomeCard::VISIBLE_BOTTOM, HomeCard::Get()->GetState());
  EXPECT_TRUE(WindowManager::Get()->IsOverviewModeActive());

  // Too short moves. Nothing has changed.
  generator.GestureScrollSequence(gfx::Point(x, bottom - 40),
                                  gfx::Point(x, bottom - 80),
                                  base::TimeDelta::FromSeconds(1),
                                  10);
  EXPECT_EQ(HomeCard::VISIBLE_BOTTOM, HomeCard::Get()->GetState());
  EXPECT_TRUE(WindowManager::Get()->IsOverviewModeActive());

  generator.GestureScrollSequence(gfx::Point(x, bottom - 40),
                                  gfx::Point(x, bottom - 20),
                                  base::TimeDelta::FromSeconds(1),
                                  10);
  EXPECT_EQ(HomeCard::VISIBLE_BOTTOM, HomeCard::Get()->GetState());
  EXPECT_TRUE(WindowManager::Get()->IsOverviewModeActive());

  // Swipe up to the centered state.
  generator.GestureScrollSequence(gfx::Point(x, bottom - 40),
                                  gfx::Point(x, bottom - 300),
                                  base::TimeDelta::FromSeconds(1),
                                  10);
  EXPECT_EQ(HomeCard::VISIBLE_CENTERED, HomeCard::Get()->GetState());
  EXPECT_TRUE(WindowManager::Get()->IsOverviewModeActive());

  // Swipe up from centered; nothing has to be changed.
  generator.GestureScrollSequence(gfx::Point(x, bottom - 300),
                                  gfx::Point(x, bottom - 350),
                                  base::TimeDelta::FromSeconds(1),
                                  10);
  EXPECT_EQ(HomeCard::VISIBLE_CENTERED, HomeCard::Get()->GetState());
  EXPECT_TRUE(WindowManager::Get()->IsOverviewModeActive());

  // Swipe down slightly; nothing has to be changed.
  generator.GestureScrollSequence(gfx::Point(x, bottom - 300),
                                  gfx::Point(x, bottom - 250),
                                  base::TimeDelta::FromSeconds(1),
                                  10);
  EXPECT_EQ(HomeCard::VISIBLE_CENTERED, HomeCard::Get()->GetState());
  EXPECT_TRUE(WindowManager::Get()->IsOverviewModeActive());

  // Swipe down to the bottom state.
  generator.GestureScrollSequence(gfx::Point(x, 10),
                                  gfx::Point(x, bottom - 90),
                                  base::TimeDelta::FromSeconds(1),
                                  10);
  EXPECT_EQ(HomeCard::VISIBLE_BOTTOM, HomeCard::Get()->GetState());
  EXPECT_TRUE(WindowManager::Get()->IsOverviewModeActive());

  generator.GestureScrollSequence(gfx::Point(x, bottom - 40),
                                  gfx::Point(x, bottom - 300),
                                  base::TimeDelta::FromSeconds(1),
                                  10);
  EXPECT_EQ(HomeCard::VISIBLE_CENTERED, HomeCard::Get()->GetState());
  EXPECT_TRUE(WindowManager::Get()->IsOverviewModeActive());

  // Swipe down to the minimized state.
  generator.GestureScrollSequence(gfx::Point(x, 10),
                                  gfx::Point(x, bottom - 1),
                                  base::TimeDelta::FromSeconds(1),
                                  10);
  EXPECT_EQ(HomeCard::VISIBLE_MINIMIZED, HomeCard::Get()->GetState());
  EXPECT_FALSE(WindowManager::Get()->IsOverviewModeActive());
}

TEST_F(HomeCardTest, GesturesToFullDirectly) {
  ASSERT_EQ(HomeCard::VISIBLE_MINIMIZED, HomeCard::Get()->GetState());
  ui::test::EventGenerator generator(root_window());
  gfx::Rect screen_rect(root_window()->bounds());

  const int bottom = screen_rect.bottom();
  const int x = screen_rect.x() + 1;

  generator.GestureScrollSequence(gfx::Point(x, bottom - 1),
                                  gfx::Point(x, 20),
                                  base::TimeDelta::FromSeconds(1),
                                  10);
  EXPECT_EQ(HomeCard::VISIBLE_CENTERED, HomeCard::Get()->GetState());
  EXPECT_TRUE(WindowManager::Get()->IsOverviewModeActive());
}

TEST_F(HomeCardTest, KeyboardFocus) {
  ASSERT_EQ(HomeCard::VISIBLE_MINIMIZED, HomeCard::Get()->GetState());
  aura::Window* home_card = GetHomeCardWindow();
  ASSERT_FALSE(IsSearchBoxFocused(home_card));

  WindowManager::Get()->ToggleOverview();
  ASSERT_FALSE(IsSearchBoxFocused(home_card));

  ui::test::EventGenerator generator(root_window());
  gfx::Rect screen_rect(root_window()->bounds());

  const int bottom = screen_rect.bottom();
  const int x = screen_rect.x() + 1;

  generator.GestureScrollSequence(gfx::Point(x, bottom - 40),
                                  gfx::Point(x, 10),
                                  base::TimeDelta::FromSeconds(1),
                                  10);
  EXPECT_EQ(HomeCard::VISIBLE_CENTERED, HomeCard::Get()->GetState());
  EXPECT_TRUE(IsSearchBoxFocused(home_card));

  generator.GestureScrollSequence(gfx::Point(x, 10),
                                  gfx::Point(x, bottom - 100),
                                  base::TimeDelta::FromSeconds(1),
                                  10);
  EXPECT_EQ(HomeCard::VISIBLE_BOTTOM, HomeCard::Get()->GetState());
  EXPECT_FALSE(IsSearchBoxFocused(home_card));
}

}  // namespace athena
