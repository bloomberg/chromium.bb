// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/window_positioner.h"

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/test_shell_delegate.h"
#include "ash/wm/window_resizer.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/render_view_test.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebKit.h"
#include "ui/aura/env.h"
#include "ui/aura/root_window.h"
#include "ui/aura/test/test_windows.h"
#include "ui/gfx/screen.h"

namespace ash {
namespace test {

namespace {

// A browser window proxy which is able to associate an aura native window with
// it.
class TestBrowserWindowAura : public TestBrowserWindow {
 public:
  explicit TestBrowserWindowAura(aura::Window *native_window);
  virtual ~TestBrowserWindowAura();

  virtual gfx::NativeWindow GetNativeWindow() OVERRIDE {
    return native_window_;
  }

 private:
  gfx::NativeWindow native_window_;

  DISALLOW_COPY_AND_ASSIGN(TestBrowserWindowAura);
};

TestBrowserWindowAura::TestBrowserWindowAura(aura::Window *native_window)
    : native_window_(native_window) {
}

TestBrowserWindowAura::~TestBrowserWindowAura() {}

} // namespace

// A test class for preparing window positioner tests - it creates a testing
// base by adding a window, a popup and a panel which can be independently
// positioned to see where the positioner will place the window.
class WindowPositionerTest : public AshTestBase {
 public:
  WindowPositionerTest();
  virtual ~WindowPositionerTest();

  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

 protected:
  aura::Window* window() { return window_.get(); }
  aura::Window* popup() { return popup_.get(); }
  aura::Window* panel() { return panel_.get(); }

  Browser* window_browser() { return window_owning_browser_.get(); }
  Browser* popup_browser() { return popup_owning_browser_.get(); }
  Browser* panel_browser() { return panel_owning_browser_.get(); }

  WindowPositioner* window_positioner() { return window_positioner_; }

  // The positioner & desktop's used grid alignment size.
  const int grid_size_;

 private:
  WindowPositioner* window_positioner_;

  // These two need to be deleted after everything else is gone.
  scoped_ptr<content::TestBrowserThread> ui_thread_;
  scoped_ptr<TestingProfile> profile_;

  // These get created for each session.
  scoped_ptr<aura::Window> window_;
  scoped_ptr<aura::Window> popup_;
  scoped_ptr<aura::Window> panel_;

  scoped_ptr<BrowserWindow> browser_window_;
  scoped_ptr<BrowserWindow> browser_popup_;
  scoped_ptr<BrowserWindow> browser_panel_;

  scoped_ptr<Browser> window_owning_browser_;
  scoped_ptr<Browser> popup_owning_browser_;
  scoped_ptr<Browser> panel_owning_browser_;

  DISALLOW_COPY_AND_ASSIGN(WindowPositionerTest);
};

WindowPositionerTest::WindowPositionerTest()
    : grid_size_(WindowPositioner::kMinimumWindowOffset),
      window_positioner_(NULL) {
  // Create a message loop.
  MessageLoopForUI* ui_loop = message_loop();
  ui_thread_.reset(
      new content::TestBrowserThread(content::BrowserThread::UI, ui_loop));

  // Create a browser profile.
  profile_.reset(new TestingProfile());
}

WindowPositionerTest::~WindowPositionerTest() {
  profile_.reset(NULL);
  ui_thread_.reset(NULL);
}

void WindowPositionerTest::SetUp() {
  AshTestBase::SetUp();
  // Create some default dummy windows.
  window_.reset(CreateTestWindowInShellWithId(0));
  window_->SetBounds(gfx::Rect(16, 32, 640, 320));
  popup_.reset(CreateTestWindowInShellWithId(1));
  popup_->SetBounds(gfx::Rect(16, 32, 128, 256));
  panel_.reset(CreateTestWindowInShellWithId(2));
  panel_->SetBounds(gfx::Rect(32, 48, 256, 512));

  // Create a browser for the window.
  browser_window_.reset(new TestBrowserWindowAura(window_.get()));
  Browser::CreateParams window_params(profile_.get(),
                                      chrome::HOST_DESKTOP_TYPE_ASH);
  window_params.window = browser_window_.get();
  window_owning_browser_.reset(new Browser(window_params));

  // Creating a browser for the popup.
  browser_popup_.reset(new TestBrowserWindowAura(popup_.get()));
  Browser::CreateParams popup_params(Browser::TYPE_POPUP, profile_.get(),
                                     chrome::HOST_DESKTOP_TYPE_ASH);
  popup_params.window = browser_popup_.get();
  popup_owning_browser_.reset(new Browser(popup_params));

  // Creating a browser for the panel.
  browser_panel_.reset(new TestBrowserWindowAura(panel_.get()));
  Browser::CreateParams panel_params(Browser::TYPE_PANEL, profile_.get(),
                                     chrome::HOST_DESKTOP_TYPE_ASH);
  panel_params.window = browser_panel_.get();
  panel_owning_browser_.reset(new Browser(panel_params));
  // We hide all windows upon start - each user is required to set it up
  // as he needs it.
  window()->Hide();
  popup()->Hide();
  panel()->Hide();
  window_positioner_ = new WindowPositioner();
}

void WindowPositionerTest::TearDown() {
  // Since the AuraTestBase is needed to create our assets, we have to
  // also delete them before we tear it down.
  window_owning_browser_.reset(NULL);
  popup_owning_browser_.reset(NULL);
  panel_owning_browser_.reset(NULL);

  browser_window_.reset(NULL);
  browser_popup_.reset(NULL);
  browser_panel_.reset(NULL);

  window_.reset(NULL);
  popup_.reset(NULL);
  panel_.reset(NULL);

  AshTestBase::TearDown();
  delete window_positioner_;
  window_positioner_ = NULL;
}

int AlignToGridRoundDown(int location, int grid_size) {
  if (grid_size <= 1 || location % grid_size == 0)
    return location;
  return location / grid_size * grid_size;
}

TEST_F(WindowPositionerTest, cascading) {
  const gfx::Rect work_area =
      Shell::GetScreen()->GetPrimaryDisplay().work_area();

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
      Shell::GetScreen()->GetPrimaryDisplay().work_area();
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

TEST_F(WindowPositionerTest, blockedByPanel) {
  const gfx::Rect work_area =
      Shell::GetScreen()->GetPrimaryDisplay().work_area();

  gfx::Rect pop_position(0, 0, 200, 200);
  // Let the panel cover everything.
  panel()->SetBounds(work_area);
  panel()->Show();

  // Check that the popup does cascade due to the panel's existence.
  gfx::Rect top_right = window_positioner()->GetPopupPosition(pop_position);
  EXPECT_EQ(gfx::Rect(work_area.x() + grid_size_, work_area.y() + grid_size_,
                      pop_position.width(), pop_position.height()),
                      top_right);
}

TEST_F(WindowPositionerTest, biggerThenBorder) {
  const gfx::Rect work_area =
      Shell::GetScreen()->GetPrimaryDisplay().work_area();

  gfx::Rect pop_position(0, 0, work_area.width(), work_area.height());

  // Check that the popup is placed full screen.
  gfx::Rect full = window_positioner()->GetPopupPosition(pop_position);
  EXPECT_EQ(gfx::Rect(work_area.x(), work_area.y(),
                      pop_position.width(), pop_position.height()),
                      full);
}

}  // namespace test
}  // namespace ash
