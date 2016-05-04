// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/common/window_positioner.h"

#include <utility>

#include "ash/test/ash_test_base.h"
#include "ash/test/test_shell_delegate.h"
#include "ash/wm/aura/wm_globals_aura.h"
#include "ash/wm/common/window_resizer.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/macros.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/test_browser_window_aura.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/render_view_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/env.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/display/screen.h"

namespace ash {
namespace test {

// A test class for preparing window positioner tests - it creates a testing
// base by adding a window and a popup which can be independently
// positioned to see where the positioner will place the window.
class WindowPositionerTest : public AshTestBase {
 public:
  WindowPositionerTest();

  void SetUp() override;
  void TearDown() override;

 protected:
  aura::Window* window() { return browser_->window()->GetNativeWindow(); }
  aura::Window* popup() { return browser_popup_->window()->GetNativeWindow(); }

  WindowPositioner* window_positioner() { return window_positioner_.get(); }

  // The positioner & desktop's used grid alignment size.
  const int grid_size_;

 private:
  std::unique_ptr<WindowPositioner> window_positioner_;

  TestingProfile profile_;

  std::unique_ptr<Browser> browser_;
  std::unique_ptr<Browser> browser_popup_;

  DISALLOW_COPY_AND_ASSIGN(WindowPositionerTest);
};

WindowPositionerTest::WindowPositionerTest()
    : grid_size_(WindowPositioner::kMinimumWindowOffset) {}

void WindowPositionerTest::SetUp() {
  AshTestBase::SetUp();
  // Create some default dummy windows.
  std::unique_ptr<aura::Window> dummy_window(CreateTestWindowInShellWithId(0));
  dummy_window->SetBounds(gfx::Rect(16, 32, 640, 320));
  std::unique_ptr<aura::Window> dummy_popup(CreateTestWindowInShellWithId(1));
  dummy_popup->SetBounds(gfx::Rect(16, 32, 128, 256));

  // Create a browser for the window.
  Browser::CreateParams window_params(&profile_);
  browser_ = chrome::CreateBrowserWithAuraTestWindowForParams(
      std::move(dummy_window), &window_params);

  // Creating a browser for the popup.
  Browser::CreateParams popup_params(Browser::TYPE_POPUP, &profile_);
  browser_popup_ = chrome::CreateBrowserWithAuraTestWindowForParams(
      std::move(dummy_popup), &popup_params);

  // We hide all windows upon start - each user is required to set it up
  // as he needs it.
  window()->Hide();
  popup()->Hide();
  window_positioner_.reset(new WindowPositioner(wm::WmGlobals::Get()));
}

void WindowPositionerTest::TearDown() {
  // Since the AuraTestBase is needed to create our assets, we have to
  // also delete them before we tear it down.
  browser_.reset();
  browser_popup_.reset();
  window_positioner_.reset();
  AshTestBase::TearDown();
}

int AlignToGridRoundDown(int location, int grid_size) {
  if (grid_size <= 1 || location % grid_size == 0)
    return location;
  return location / grid_size * grid_size;
}

TEST_F(WindowPositionerTest, cascading) {
  const gfx::Rect work_area =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();

  // First see that the window will cascade down when there is no space.
  window()->SetBounds(work_area);
  window()->Show();

  gfx::Rect popup_position(0, 0, 200, 200);
  // Check that it gets cascaded.
  gfx::Rect cascade_1 = window_positioner()->GetPopupPosition(popup_position);
  EXPECT_EQ(gfx::Rect(work_area.x() + grid_size_, work_area.y() + grid_size_,
                      popup_position.width(), popup_position.height()),
                      cascade_1);

  gfx::Rect cascade_2 = window_positioner()->GetPopupPosition(popup_position);
  EXPECT_EQ(gfx::Rect(work_area.x() + 2 * grid_size_,
                      work_area.y() + 2 * grid_size_,
                      popup_position.width(), popup_position.height()),
                      cascade_2);

  // Check that if there is even only a pixel missing it will cascade.
  window()->SetBounds(gfx::Rect(work_area.x() + popup_position.width() - 1,
                                work_area.y() + popup_position.height() - 1,
                                work_area.width() -
                                    2 * (popup_position.width() - 1),
                                work_area.height() -
                                    2 * (popup_position.height() - 1)));

  gfx::Rect cascade_3 = window_positioner()->GetPopupPosition(popup_position);
  EXPECT_EQ(gfx::Rect(work_area.x() + 3 * grid_size_,
                      work_area.y() + 3 * grid_size_,
                      popup_position.width(), popup_position.height()),
                      cascade_3);

  // Check that we overflow into the next line when we do not fit anymore in Y.
  gfx::Rect popup_position_4(0, 0, 200,
                             work_area.height() -
                                 (cascade_3.y() - work_area.y()));
  gfx::Rect cascade_4 =
      window_positioner()->GetPopupPosition(popup_position_4);
  EXPECT_EQ(gfx::Rect(work_area.x() + 2 * grid_size_,
                      work_area.y() + grid_size_,
                      popup_position_4.width(), popup_position_4.height()),
                      cascade_4);

  // Check that we overflow back to the first possible location if we overflow
  // to the end.
  gfx::Rect popup_position_5(0, 0,
                            work_area.width() + 1 -
                                (cascade_4.x() - work_area.x()),
                            work_area.height() -
                                (2 * grid_size_ - work_area.y()));
  gfx::Rect cascade_5 =
      window_positioner()->GetPopupPosition(popup_position_5);
  EXPECT_EQ(gfx::Rect(work_area.x() + grid_size_,
                      work_area.y() + grid_size_,
                      popup_position_5.width(), popup_position_5.height()),
                      cascade_5);
}

TEST_F(WindowPositionerTest, filling) {
  const gfx::Rect work_area =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();
  gfx::Rect popup_position(0, 0, 256, 128);
  // Leave space on the left and the right and see if we fill top to bottom.
  window()->SetBounds(gfx::Rect(work_area.x() + popup_position.width(),
                                work_area.y(),
                                work_area.width() - 2 * popup_position.width(),
                                work_area.height()));
  window()->Show();
  // Check that we are positioned in the top left corner.
  gfx::Rect top_left = window_positioner()->GetPopupPosition(popup_position);
  EXPECT_EQ(gfx::Rect(work_area.x(), work_area.y(),
                      popup_position.width(), popup_position.height()),
                      top_left);

  // Now block the found location.
  popup()->SetBounds(top_left);
  popup()->Show();
  gfx::Rect mid_left = window_positioner()->GetPopupPosition(popup_position);
  EXPECT_EQ(gfx::Rect(work_area.x(),
                      AlignToGridRoundDown(
                          work_area.y() + top_left.height(), grid_size_),
                          popup_position.width(), popup_position.height()),
                      mid_left);

  // Block now everything so that we can only put the popup on the bottom
  // of the left side.
  // Note: We need to keep one "grid spacing free" if the window does not
  // fit into the grid (which is true for 200 height).`
  popup()->SetBounds(gfx::Rect(work_area.x(), work_area.y(),
                               popup_position.width(),
                               work_area.height() - popup_position.height() -
                                   grid_size_ + 1));
  gfx::Rect bottom_left = window_positioner()->GetPopupPosition(
                              popup_position);
  EXPECT_EQ(gfx::Rect(work_area.x(),
                      work_area.bottom() - popup_position.height(),
                      popup_position.width(), popup_position.height()),
                      bottom_left);

  // Block now enough to force the right side.
  popup()->SetBounds(gfx::Rect(work_area.x(), work_area.y(),
                               popup_position.width(),
                               work_area.height() - popup_position.height() +
                               1));
  gfx::Rect top_right = window_positioner()->GetPopupPosition(
                            popup_position);
  EXPECT_EQ(gfx::Rect(AlignToGridRoundDown(work_area.right() -
                                           popup_position.width(), grid_size_),
                      work_area.y(),
                      popup_position.width(), popup_position.height()),
                      top_right);
}

TEST_F(WindowPositionerTest, biggerThenBorder) {
  const gfx::Rect work_area =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();

  gfx::Rect pop_position(0, 0, work_area.width(), work_area.height());

  // Check that the popup is placed full screen.
  gfx::Rect full = window_positioner()->GetPopupPosition(pop_position);
  EXPECT_EQ(gfx::Rect(work_area.x(), work_area.y(),
                      pop_position.width(), pop_position.height()),
                      full);
}

}  // namespace test
}  // namespace ash
