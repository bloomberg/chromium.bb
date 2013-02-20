// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/window_sizer/window_sizer_common_unittest.h"

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/test_shell_delegate.h"
#include "ash/wm/window_resizer.h"
#include "base/compiler_specific.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/render_view_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebKit.h"
#include "ui/aura/client/activation_client.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/env.h"
#include "ui/aura/root_window.h"
#include "ui/aura/test/test_windows.h"

typedef ash::test::AshTestBase WindowSizerTest;

namespace {

// A browser window proxy which is able to associate an aura native window with
// it.
class TestBrowserWindowAura : public TestBrowserWindow {
 public:
  // |native_window| will still be owned by the caller after the constructor
  // was called.
  explicit TestBrowserWindowAura(aura::Window* native_window);
  virtual ~TestBrowserWindowAura();

  // TestBrowserWindow overrides:
  virtual void Show() OVERRIDE;
  virtual void Activate() OVERRIDE;
  virtual gfx::NativeWindow GetNativeWindow() OVERRIDE;
  virtual gfx::Rect GetBounds() const OVERRIDE;

 private:
  // The |native_window_| is still owned by the creator of this class.
  aura::Window* native_window_;

  DISALLOW_COPY_AND_ASSIGN(TestBrowserWindowAura);
};

TestBrowserWindowAura::TestBrowserWindowAura(aura::Window *native_window)
    : native_window_(native_window) {
}

TestBrowserWindowAura::~TestBrowserWindowAura() {}

void TestBrowserWindowAura::Show() {
  native_window_->Show();
  Activate();
}

void TestBrowserWindowAura::Activate() {
  GetActivationClient(
      native_window_->GetRootWindow())->ActivateWindow(native_window_);
}

gfx::NativeWindow TestBrowserWindowAura::GetNativeWindow() {
    return native_window_;
}

gfx::Rect TestBrowserWindowAura::GetBounds() const {
  return native_window_->bounds();
}

int AlignToGridRoundDown(int location, int grid_size) {
  if (grid_size <= 1 || location % grid_size == 0)
    return location;
  return location / grid_size * grid_size;
}

// A special test class for use with browser creation - it will create a
// browser thread and deletes it after all other things have been destroyed.
class WindowSizerTestWithBrowser : public WindowSizerTest {
 public:
  WindowSizerTestWithBrowser();
  virtual ~WindowSizerTestWithBrowser();

 private:
  // Note: It is important to delete the thread after the browser instances got
  // deleted. For this we transfer the thread here.
  scoped_ptr<content::TestBrowserThread> ui_thread_;

  DISALLOW_COPY_AND_ASSIGN(WindowSizerTestWithBrowser);
};

// The class function definitions from window_sizer_common_unittest.h
WindowSizerTestWithBrowser::WindowSizerTestWithBrowser() {
  // Set up a UI message thread.
  MessageLoopForUI* ui_loop = message_loop();
  ui_thread_.reset(
      new content::TestBrowserThread(content::BrowserThread::UI, ui_loop));
}

WindowSizerTestWithBrowser::~WindowSizerTestWithBrowser() {
}

}
// Test that the window is sized appropriately for the first run experience
// where the default window bounds calculation is invoked.
TEST_F(WindowSizerTest, DefaultSizeCase) {
  int grid = WindowSizer::kDesktopBorderSize;
  { // 4:3 monitor case, 1024x768, no taskbar
    gfx::Rect window_bounds;
    GetWindowBounds(p1024x768, p1024x768, gfx::Rect(), gfx::Rect(),
                    gfx::Rect(), DEFAULT, NULL, gfx::Rect(), &window_bounds);
    EXPECT_EQ(gfx::Rect(WindowSizer::kDesktopBorderSize,
                        WindowSizer::kDesktopBorderSize,
                        1024 - WindowSizer::kDesktopBorderSize * 2,
                        768 - WindowSizer::kDesktopBorderSize),
              window_bounds);
  }

  { // 4:3 monitor case, 1024x768, taskbar on bottom
    gfx::Rect window_bounds;
    GetWindowBounds(p1024x768, taskbar_bottom_work_area, gfx::Rect(),
                    gfx::Rect(), gfx::Rect(), DEFAULT, NULL, gfx::Rect(),
                    &window_bounds);
    EXPECT_EQ(gfx::Rect(WindowSizer::kDesktopBorderSize,
                        WindowSizer::kDesktopBorderSize,
                        1024 - WindowSizer::kDesktopBorderSize * 2,
                        AlignToGridRoundDown(
                            taskbar_bottom_work_area.height() -
                            WindowSizer::kDesktopBorderSize, grid)),
                        window_bounds);
  }

  { // 4:3 monitor case, 1024x768, taskbar on right
    gfx::Rect window_bounds;
    GetWindowBounds(p1024x768, taskbar_right_work_area, gfx::Rect(),
                    gfx::Rect(), gfx::Rect(), DEFAULT, NULL, gfx::Rect(),
                    &window_bounds);
    EXPECT_EQ(gfx::Rect(WindowSizer::kDesktopBorderSize,
                        WindowSizer::kDesktopBorderSize,
                        AlignToGridRoundDown(
                            taskbar_right_work_area.width() -
                            WindowSizer::kDesktopBorderSize * 2, grid),
                        768 - WindowSizer::kDesktopBorderSize),
              window_bounds);
  }

  { // 4:3 monitor case, 1024x768, taskbar on left
    gfx::Rect window_bounds;
    GetWindowBounds(p1024x768, taskbar_left_work_area, gfx::Rect(),
                    gfx::Rect(), gfx::Rect(), DEFAULT, NULL, gfx::Rect(),
                    &window_bounds);
    EXPECT_EQ(gfx::Rect(taskbar_left_work_area.x() +
                          WindowSizer::kDesktopBorderSize,
                        WindowSizer::kDesktopBorderSize,
                        AlignToGridRoundDown(
                            taskbar_left_work_area.width() -
                        WindowSizer::kDesktopBorderSize * 2, grid),
                            AlignToGridRoundDown(
                            taskbar_left_work_area.height() -
                            WindowSizer::kDesktopBorderSize, grid)),
              window_bounds);
  }

  { // 4:3 monitor case, 1024x768, taskbar on top
    gfx::Rect window_bounds;
    GetWindowBounds(p1024x768, taskbar_top_work_area, gfx::Rect(),
                    gfx::Rect(), gfx::Rect(), DEFAULT, NULL, gfx::Rect(),
                    &window_bounds);
    EXPECT_EQ(gfx::Rect(WindowSizer::kDesktopBorderSize,
                        taskbar_top_work_area.y() +
                          WindowSizer::kDesktopBorderSize,
                        1024 - WindowSizer::kDesktopBorderSize * 2,
                        AlignToGridRoundDown(
                            taskbar_top_work_area.height() -
                            WindowSizer::kDesktopBorderSize, grid)),
              window_bounds);
  }

  { // 4:3 monitor case, 1280x1024
    gfx::Rect window_bounds;
    GetWindowBounds(p1280x1024, p1280x1024, gfx::Rect(), gfx::Rect(),
                    gfx::Rect(), DEFAULT, NULL, gfx::Rect(), &window_bounds);
    EXPECT_EQ(gfx::Rect((1280 - WindowSizer::kMaximumWindowWidth) / 2,
                        WindowSizer::kDesktopBorderSize,
                        WindowSizer::kMaximumWindowWidth,
                        1024 - WindowSizer::kDesktopBorderSize),
              window_bounds);
  }

  { // 4:3 monitor case, 1600x1200
    gfx::Rect window_bounds;
    GetWindowBounds(p1600x1200, p1600x1200, gfx::Rect(), gfx::Rect(),
                    gfx::Rect(), DEFAULT, NULL, gfx::Rect(), &window_bounds);
    EXPECT_EQ(gfx::Rect((1600 - WindowSizer::kMaximumWindowWidth) / 2,
                        WindowSizer::kDesktopBorderSize,
                        WindowSizer::kMaximumWindowWidth,
                        1200 - WindowSizer::kDesktopBorderSize),
              window_bounds);
  }

  { // 16:10 monitor case, 1680x1050
    gfx::Rect window_bounds;
    GetWindowBounds(p1680x1050, p1680x1050, gfx::Rect(), gfx::Rect(),
                    gfx::Rect(), DEFAULT, NULL, gfx::Rect(), &window_bounds);
    EXPECT_EQ(gfx::Rect((1680 - WindowSizer::kMaximumWindowWidth) / 2,
                        WindowSizer::kDesktopBorderSize,
                        WindowSizer::kMaximumWindowWidth,
                        AlignToGridRoundDown(
                            1050 - WindowSizer::kDesktopBorderSize,
                            grid)),
              window_bounds);
  }

  { // 16:10 monitor case, 1920x1200
    gfx::Rect window_bounds;
    GetWindowBounds(p1920x1200, p1920x1200, gfx::Rect(), gfx::Rect(),
                    gfx::Rect(), DEFAULT, NULL, gfx::Rect(), &window_bounds);
    EXPECT_EQ(gfx::Rect((1920 - WindowSizer::kMaximumWindowWidth) / 2,
                        WindowSizer::kDesktopBorderSize,
                        WindowSizer::kMaximumWindowWidth,
                        1200 - WindowSizer::kDesktopBorderSize),
              window_bounds);
  }
}

// Test that the next opened window is positioned appropriately given the
// bounds of an existing window of the same type.
TEST_F(WindowSizerTest, LastWindowBoundsCase) {
  { // normal, in the middle of the screen somewhere.
    gfx::Rect window_bounds;
    GetWindowBounds(p1024x768, p1024x768, gfx::Rect(),
                    gfx::Rect(WindowSizer::kDesktopBorderSize,
                              WindowSizer::kDesktopBorderSize, 500, 400),
                    gfx::Rect(), LAST_ACTIVE, NULL, gfx::Rect(),
                    &window_bounds);
    EXPECT_EQ(gfx::Rect(kWindowTilePixels + WindowSizer::kDesktopBorderSize,
                        kWindowTilePixels + WindowSizer::kDesktopBorderSize,
                        500, 400).ToString(),
              window_bounds.ToString());
  }

  { // taskbar on top.
    gfx::Rect window_bounds;
    GetWindowBounds(p1024x768, taskbar_top_work_area, gfx::Rect(),
                    gfx::Rect(WindowSizer::kDesktopBorderSize,
                              WindowSizer::kDesktopBorderSize, 500, 400),
                    gfx::Rect(), LAST_ACTIVE, NULL, gfx::Rect(),
                    &window_bounds);
    EXPECT_EQ(gfx::Rect(kWindowTilePixels + WindowSizer::kDesktopBorderSize,
                        std::max(kWindowTilePixels +
                                 WindowSizer::kDesktopBorderSize,
                                 34 /* toolbar height */),
                        500, 400).ToString(), window_bounds.ToString());
  }

  { // Too small to satisify the minimum visibility condition.
    gfx::Rect window_bounds;
    GetWindowBounds(p1024x768, p1024x768, gfx::Rect(),
                    gfx::Rect(WindowSizer::kDesktopBorderSize,
                              WindowSizer::kDesktopBorderSize, 29, 29),
                    gfx::Rect(), LAST_ACTIVE, NULL, gfx::Rect(),
                    &window_bounds);
    EXPECT_EQ(gfx::Rect(kWindowTilePixels + WindowSizer::kDesktopBorderSize,
                        kWindowTilePixels + WindowSizer::kDesktopBorderSize,
                        30 /* not 29 */,
                        30 /* not 29 */).ToString(),
              window_bounds.ToString());
  }


  { // Normal.
    gfx::Rect window_bounds;
    GetWindowBounds(p1024x768, p1024x768, gfx::Rect(),
                    gfx::Rect(WindowSizer::kDesktopBorderSize,
                              WindowSizer::kDesktopBorderSize, 500, 400),
                    gfx::Rect(), LAST_ACTIVE, NULL, gfx::Rect(),
                    &window_bounds);
    EXPECT_EQ(gfx::Rect(kWindowTilePixels + WindowSizer::kDesktopBorderSize,
                        kWindowTilePixels + WindowSizer::kDesktopBorderSize,
                        500, 400).ToString(),
              window_bounds.ToString());
  }
}

// Test that the window opened is sized appropriately given persisted sizes.
TEST_F(WindowSizerTest, PersistedBoundsCase) {
  { // normal, in the middle of the screen somewhere.
    gfx::Rect initial_bounds(WindowSizer::kDesktopBorderSize,
                             WindowSizer::kDesktopBorderSize, 500, 400);

    gfx::Rect window_bounds;
    GetWindowBounds(p1024x768, p1024x768, gfx::Rect(), initial_bounds,
                    gfx::Rect(), PERSISTED, NULL, gfx::Rect(), &window_bounds);
    EXPECT_EQ(initial_bounds.ToString(), window_bounds.ToString());
  }

  { // Normal.
    gfx::Rect initial_bounds(0, 0, 1024, 768);

    gfx::Rect window_bounds;
    GetWindowBounds(p1024x768, p1024x768, gfx::Rect(), initial_bounds,
                    gfx::Rect(), PERSISTED, NULL, gfx::Rect(), &window_bounds);
    EXPECT_EQ(initial_bounds.ToString(), window_bounds.ToString());
  }

  { // normal, on non-primary monitor in negative coords.
    gfx::Rect initial_bounds(-600, 10, 500, 400);

    gfx::Rect window_bounds;
    GetWindowBounds(p1024x768, p1024x768, left_s1024x768,
                    initial_bounds, gfx::Rect(), PERSISTED, NULL, gfx::Rect(),
                    &window_bounds);
    EXPECT_EQ(initial_bounds.ToString(), window_bounds.ToString());
  }

  { // normal, on non-primary monitor in negative coords.
    gfx::Rect initial_bounds(-1024, 0, 1024, 768);

    gfx::Rect window_bounds;
    GetWindowBounds(p1024x768, p1024x768, left_s1024x768,
                    initial_bounds, gfx::Rect(), PERSISTED, NULL, gfx::Rect(),
                    &window_bounds);
    EXPECT_EQ(initial_bounds.ToString(), window_bounds.ToString());
  }

  { // Non-primary monitor resoultion has changed, but the monitor still
    // completely contains the window.

    gfx::Rect initial_bounds(1074, 50, 600, 500);

    gfx::Rect window_bounds;
    GetWindowBounds(p1024x768, p1024x768, gfx::Rect(1024, 0, 800, 600),
                    initial_bounds, right_s1024x768, PERSISTED, NULL,
                    gfx::Rect(), &window_bounds);
    EXPECT_EQ(initial_bounds.ToString(), window_bounds.ToString());
  }

  { // Non-primary monitor resoultion has changed, and the window is partially
    // off-screen.

    gfx::Rect initial_bounds(1274, 50, 600, 500);

    gfx::Rect window_bounds;
    GetWindowBounds(p1024x768, p1024x768, gfx::Rect(1024, 0, 800, 600),
                    initial_bounds, right_s1024x768, PERSISTED,
                    NULL, gfx::Rect(), &window_bounds);
    EXPECT_EQ("1224,50 600x500", window_bounds.ToString());
  }

  { // Non-primary monitor resoultion has changed, and the window is now too
    // large for the monitor.

    gfx::Rect initial_bounds(1274, 50, 900, 700);

    gfx::Rect window_bounds;
    GetWindowBounds(p1024x768, p1024x768, gfx::Rect(1024, 0, 800, 600),
                    initial_bounds, right_s1024x768, PERSISTED,
                    NULL, gfx::Rect(), &window_bounds);
    EXPECT_EQ("1024,0 800x600", window_bounds.ToString());
  }

  { // width and height too small
    gfx::Rect window_bounds;
    GetWindowBounds(p1024x768, p1024x768, gfx::Rect(),
                    gfx::Rect(WindowSizer::kDesktopBorderSize,
                              WindowSizer::kDesktopBorderSize, 29, 29),
                    gfx::Rect(), PERSISTED, NULL, gfx::Rect(), &window_bounds);
    EXPECT_EQ(gfx::Rect(WindowSizer::kDesktopBorderSize,
                        WindowSizer::kDesktopBorderSize,
                        30 /* not 29 */, 30 /* not 29 */).ToString(),
              window_bounds.ToString());
  }
}

//////////////////////////////////////////////////////////////////////////////
// The following unittests have different results on Mac/non-Mac because we
// reposition windows aggressively on Mac.  The *WithAggressiveReposition tests
// are run on Mac, and the *WithNonAggressiveRepositioning tests are run on
// other platforms.

TEST_F(WindowSizerTest, LastWindowOffscreenWithNonAggressiveRepositioning) {
  { // taskbar on left.
    gfx::Rect window_bounds;
    GetWindowBounds(p1024x768, taskbar_left_work_area, gfx::Rect(),
                    gfx::Rect(WindowSizer::kDesktopBorderSize,
                              WindowSizer::kDesktopBorderSize, 500, 400),
                    gfx::Rect(), LAST_ACTIVE, NULL, gfx::Rect(),
                    &window_bounds);
    EXPECT_EQ(gfx::Rect(kWindowTilePixels + WindowSizer::kDesktopBorderSize,
                        kWindowTilePixels + WindowSizer::kDesktopBorderSize,
                        500, 400).ToString(),
              window_bounds.ToString());
  }

  { // offset would put the new window offscreen at the bottom but the minimum
    // visibility condition is barely satisfied without relocation.
    gfx::Rect window_bounds;
    GetWindowBounds(p1024x768, p1024x768, gfx::Rect(),
                    gfx::Rect(10, 728, 500, 400), gfx::Rect(), LAST_ACTIVE,
                    NULL, gfx::Rect(), &window_bounds);
    EXPECT_EQ(gfx::Rect(10 + kWindowTilePixels, 738, 500, 400).ToString(),
              window_bounds.ToString());
  }

  { // offset would put the new window offscreen at the bottom and the minimum
    // visibility condition is satisified by relocation.
    gfx::Rect window_bounds;
    GetWindowBounds(p1024x768, p1024x768, gfx::Rect(),
                    gfx::Rect(10, 729, 500, 400), gfx::Rect(), LAST_ACTIVE,
                    NULL, gfx::Rect(), &window_bounds);
    EXPECT_EQ(gfx::Rect(10 + kWindowTilePixels,
                        738 /* not 739 */,
                        500,
                        400).ToString(),
              window_bounds.ToString());
  }

  { // offset would put the new window offscreen at the right but the minimum
    // visibility condition is barely satisfied without relocation.
    gfx::Rect window_bounds;
    GetWindowBounds(p1024x768, p1024x768, gfx::Rect(),
                    gfx::Rect(984, 10, 500, 400), gfx::Rect(), LAST_ACTIVE,
                    NULL, gfx::Rect(), &window_bounds);
    EXPECT_EQ(gfx::Rect(994, 10 + kWindowTilePixels, 500, 400).ToString(),
              window_bounds.ToString());
  }

  { // offset would put the new window offscreen at the right and the minimum
    // visibility condition is satisified by relocation.
    gfx::Rect window_bounds;
    GetWindowBounds(p1024x768, p1024x768, gfx::Rect(),
                    gfx::Rect(985, 10, 500, 400), gfx::Rect(), LAST_ACTIVE,
                    NULL, gfx::Rect(), &window_bounds);
    EXPECT_EQ(gfx::Rect(994 /* not 995 */,
                        10 + kWindowTilePixels,
                        500,
                        400).ToString(),
              window_bounds.ToString());
  }

  { // offset would put the new window offscreen at the bottom right and the
    // minimum visibility condition is satisified by relocation.
    gfx::Rect window_bounds;
    GetWindowBounds(p1024x768, p1024x768, gfx::Rect(),
                    gfx::Rect(985, 729, 500, 400), gfx::Rect(), LAST_ACTIVE,
                    NULL, gfx::Rect(), &window_bounds);
    EXPECT_EQ(gfx::Rect(994 /* not 995 */,
                        738 /* not 739 */,
                        500,
                        400).ToString(),
              window_bounds.ToString());
  }
}

// Test the placement of newly created windows.
TEST_F(WindowSizerTestWithBrowser, PlaceNewWindows) {
  // Create a dummy window.
  scoped_ptr<aura::Window> window(CreateTestWindowInShellWithId(0));
  window->SetBounds(gfx::Rect(16, 32, 640, 320));

  scoped_ptr<aura::Window> popup(CreateTestWindowInShellWithId(1));
  popup->SetBounds(gfx::Rect(16, 32, 128, 256));

  scoped_ptr<aura::Window> panel(CreateTestWindowInShellWithId(2));
  panel->SetBounds(gfx::Rect(32, 48, 256, 512));

  // Create a browser which we can use to pass into the GetWindowBounds
  // function.
  scoped_ptr<TestingProfile> profile(new TestingProfile());
  // Creating a popup handler here to make sure it does not interfere with the
  // existing windows.
  scoped_ptr<Browser> browser(
      chrome::CreateBrowserWithTestWindowForProfile(profile.get()));

  // Creating a popup handler here to make sure it does not interfere with the
  // existing windows.
  scoped_ptr<BrowserWindow> browser_window(
      new TestBrowserWindowAura(window.get()));
  Browser::CreateParams window_params(profile.get());
  window_params.window = browser_window.get();
  scoped_ptr<Browser> window_owning_browser(new Browser(window_params));

  // Creating a popup to make sure it does not interfere with the positioning.
  scoped_ptr<BrowserWindow> browser_popup(
      new TestBrowserWindowAura(popup.get()));
  Browser::CreateParams popup_params(Browser::TYPE_POPUP, profile.get());
  popup_params.window = browser_popup.get();
  scoped_ptr<Browser> popup_owning_browser(new Browser(popup_params));

  // Creating a panel to make sure it does not interfere with the positioning.
  scoped_ptr<BrowserWindow> browser_panel(
      new TestBrowserWindowAura(panel.get()));
  Browser::CreateParams panel_params(Browser::TYPE_POPUP, profile.get());
  panel_params.window = browser_panel.get();
  scoped_ptr<Browser> panel_owning_browser(new Browser(panel_params));

  window->Show();
  { // With a shown window it's size should get returned.
    gfx::Rect window_bounds;
    GetWindowBounds(p1600x1200, p1600x1200, gfx::Rect(),
                    gfx::Rect(50, 100, 300, 150), bottom_s1600x1200,
                    PERSISTED, browser.get(), gfx::Rect(), &window_bounds);
    // The position should be right flush.
    EXPECT_EQ("960,32 640x320", window_bounds.ToString());
  }

  { // With the window shown - but more on the right side then on the left
    // side (and partially out of the screen), it should default to the other
    // side and inside the screen.
    window->SetBounds(gfx::Rect(1000, 600, 640, 320));
    gfx::Rect window_bounds;
    GetWindowBounds(p1600x1200, p1600x1200, gfx::Rect(),
                    gfx::Rect(50, 100, 300, 150), bottom_s1600x1200,
                    PERSISTED, browser.get(), gfx::Rect(), &window_bounds);
    // The position should be left & bottom flush.
    EXPECT_EQ("0,600 640x320", window_bounds.ToString());
  }

  { // If the second windows right side is already over the right side of the
    // screen, it will not move back into the screen.
    window->SetBounds(gfx::Rect(1000, 600, 640, 320));
    gfx::Rect window_bounds;
    GetWindowBounds(p1600x1200, p1600x1200, gfx::Rect(),
                    gfx::Rect(50, 100, 300, 150), bottom_s1600x1200,
                    PERSISTED, browser.get(), gfx::Rect(), &window_bounds);
    // The position should be left & bottom flush.
    EXPECT_EQ("0,600 640x320", window_bounds.ToString());
    // If the other window was already beyond the point to get right flush
    // it will remain where it is.
    EXPECT_EQ("1000,600 640x320", window->bounds().ToString());
  }

  { // Make sure that popups do not get changed.
    gfx::Rect window_bounds;
    GetWindowBounds(p1600x1200, p1600x1200, gfx::Rect(),
                    gfx::Rect(50, 100, 300, 150), bottom_s1600x1200,
                    PERSISTED, popup_owning_browser.get(),
                    gfx::Rect(), &window_bounds);
    EXPECT_EQ("50,100 300x150", window_bounds.ToString());
  }

  window->Hide();
  { // If a window is there but not shown the persisted default should be used.
    gfx::Rect window_bounds;
    GetWindowBounds(p1600x1200, p1600x1200, gfx::Rect(),
                    gfx::Rect(50, 100, 300, 150), bottom_s1600x1200,
                    PERSISTED, browser.get(), gfx::Rect(), &window_bounds);
    EXPECT_EQ("50,100 300x150", window_bounds.ToString());
  }

  { // If a window is there but not shown the default should be returned.
    gfx::Rect window_bounds;
    GetWindowBounds(p1600x1200, p1600x1200, gfx::Rect(),
                    gfx::Rect(), bottom_s1600x1200,
                    DEFAULT, browser.get(), gfx::Rect(), &window_bounds);
    // Note: We need to also take the defaults maximum width into account here
    // since that might get used if the resolution is too big.
    EXPECT_EQ(gfx::Rect(std::max(WindowSizer::kDesktopBorderSize,
                                 (1600 - WindowSizer::kMaximumWindowWidth) / 2),
                        WindowSizer::kDesktopBorderSize,
                        std::min(WindowSizer::kMaximumWindowWidth,
                                 1600 - 2 * WindowSizer::kDesktopBorderSize),
                        1200 - WindowSizer::kDesktopBorderSize).ToString(),
              window_bounds.ToString());
  }
}

#if defined(OS_CHROMEOS)
#define MAYBE_PlaceNewWindowsOnMultipleDisplays PlaceNewWindowsOnMultipleDisplays
#else
// No multiple displays on windows ash.
#define MAYBE_PlaceNewWindowsOnMultipleDisplays DISABLED_PlaceNewWindowsOnMultipleDisplays
#endif

// Test the placement of newly created windows on multiple dislays.
TEST_F(WindowSizerTestWithBrowser, MAYBE_PlaceNewWindowsOnMultipleDisplays) {
  UpdateDisplay("1600x1200,1600x1200");
  const gfx::Rect secondary(1600, 0, 1600, 1200);

  ash::Shell::GetInstance()->set_active_root_window(
      ash::Shell::GetPrimaryRootWindow());

  scoped_ptr<TestingProfile> profile(new TestingProfile());

  // Create browser windows that are used as reference.
  scoped_ptr<aura::Window> window(CreateTestWindowInShellWithId(0));
  window->SetBounds(gfx::Rect(10, 10, 200, 200));
  scoped_ptr<BrowserWindow> browser_window(
      new TestBrowserWindowAura(window.get()));
  Browser::CreateParams window_params(profile.get());
  window_params.window = browser_window.get();
  scoped_ptr<Browser> window_owning_browser(new Browser(window_params));
  browser_window->Show();
  EXPECT_EQ(window->GetRootWindow(), ash::Shell::GetActiveRootWindow());

  scoped_ptr<aura::Window> another_window(CreateTestWindowInShellWithId(1));
  another_window->SetBounds(gfx::Rect(1600 - 200, 10, 300, 300));
  scoped_ptr<BrowserWindow> another_browser_window(
      new TestBrowserWindowAura(another_window.get()));
  Browser::CreateParams another_window_params(profile.get());
  another_window_params.window = another_browser_window.get();
  scoped_ptr<Browser> another_window_owning_browser(
      new Browser(another_window_params));
  another_browser_window->Show();

  // Creating a new window to verify the new placement.
  scoped_ptr<aura::Window> new_window(CreateTestWindowInShellWithId(0));
  scoped_ptr<BrowserWindow> new_browser_window(
      new TestBrowserWindowAura(new_window.get()));
  Browser::CreateParams new_window_params(profile.get());
  new_window_params.window = new_browser_window.get();
  scoped_ptr<Browser> new_browser(new Browser(new_window_params));

  // Make sure the primary root is active.
  ASSERT_EQ(ash::Shell::GetPrimaryRootWindow(),
            ash::Shell::GetActiveRootWindow());

  // First new window should be in the primary.
  {
    gfx::Rect window_bounds;
    GetWindowBounds(p1600x1200, p1600x1200, secondary,
                    gfx::Rect(), secondary,
                    PERSISTED, new_browser.get(), gfx::Rect(), &window_bounds);
    EXPECT_EQ("0,10 300x300", window_bounds.ToString());
  }

  // Move the window to the right side of the secondary display and create a new
  // window. It should be opened then on the left side on the secondary display.
  {
    gfx::Display second_display = gfx::Screen::GetScreenFor(window.get())->
        GetDisplayNearestPoint(gfx::Point(1600 + 100,10));
    window->SetBoundsInScreen(
       gfx::Rect(secondary.CenterPoint().x() + 300, 10, 200, 200),
        second_display);
    browser_window->Activate();
    EXPECT_NE(ash::Shell::GetPrimaryRootWindow(),
              ash::Shell::GetActiveRootWindow());

    gfx::Rect window_bounds;
    GetWindowBounds(p1600x1200, p1600x1200, secondary,
                    gfx::Rect(), secondary,
                    PERSISTED, new_browser.get(), gfx::Rect(), &window_bounds);
    EXPECT_EQ("1600,10 200x200", window_bounds.ToString());
  }

  // Activate another window in the primary display and create a new window.
  // It should be created in the primary display.
  {
    another_browser_window->Activate();
    EXPECT_EQ(ash::Shell::GetPrimaryRootWindow(),
              ash::Shell::GetActiveRootWindow());

    gfx::Rect window_bounds;
    GetWindowBounds(p1600x1200, p1600x1200, secondary,
                    gfx::Rect(), secondary,
                    PERSISTED, new_browser.get(), gfx::Rect(), &window_bounds);
    EXPECT_EQ("0,10 300x300", window_bounds.ToString());
  }
}

// Test that the show state is properly returned for non default cases.
TEST_F(WindowSizerTestWithBrowser, TestShowState) {
  // Creating a browser & window to play with.
  scoped_ptr<aura::Window> window(CreateTestWindowInShellWithId(0));
  window->SetBounds(gfx::Rect(16, 32, 640, 320));

  scoped_ptr<TestingProfile> profile(new TestingProfile());

  scoped_ptr<BrowserWindow> browser_window(
      new TestBrowserWindowAura(window.get()));
  Browser::CreateParams window_params(Browser::TYPE_TABBED, profile.get());
  window_params.window = browser_window.get();
  scoped_ptr<Browser> browser(new Browser(window_params));

  // Create also a popup browser since that behaves different.
  scoped_ptr<aura::Window> popup(CreateTestWindowInShellWithId(1));
  popup->SetBounds(gfx::Rect(16, 32, 128, 256));

  scoped_ptr<BrowserWindow> browser_popup(
      new TestBrowserWindowAura(popup.get()));
  Browser::CreateParams popup_params(Browser::TYPE_POPUP, profile.get());
  popup_params.window = browser_window.get();
  scoped_ptr<Browser> popup_browser(new Browser(popup_params));

  // Tabbed windows should retrieve the saved window state - since there is a
  // top window.
  EXPECT_EQ(ui::SHOW_STATE_MAXIMIZED,
            GetWindowShowState(ui::SHOW_STATE_MAXIMIZED,
                               ui::SHOW_STATE_NORMAL,
                               BOTH,
                               browser.get(),
                               p1600x1200));
  EXPECT_EQ(ui::SHOW_STATE_DEFAULT,
            GetWindowShowState(ui::SHOW_STATE_DEFAULT,
                               ui::SHOW_STATE_NORMAL,
                               BOTH,
                               browser.get(),
                               p1600x1200));
  // Non tabbed windows should always follow the window saved visibility state.
  EXPECT_EQ(ui::SHOW_STATE_MAXIMIZED,
            GetWindowShowState(ui::SHOW_STATE_MAXIMIZED,
                               ui::SHOW_STATE_NORMAL,
                               BOTH,
                               popup_browser.get(),
                               p1600x1200));
  // The non tabbed window will take the status of the last active of its kind.
  EXPECT_EQ(ui::SHOW_STATE_NORMAL,
            GetWindowShowState(ui::SHOW_STATE_DEFAULT,
                               ui::SHOW_STATE_NORMAL,
                               BOTH,
                               popup_browser.get(),
                               p1600x1200));

  // Now create a top level window and check again for both. Only the tabbed
  // window should follow the top level window's state.
  // Creating a browser & window to play with.
  scoped_ptr<aura::Window> window2(CreateTestWindowInShellWithId(0));
  window->SetBounds(gfx::Rect(16, 32, 640, 320));

  scoped_ptr<BrowserWindow> browser_window2(
      new TestBrowserWindowAura(window2.get()));
  Browser::CreateParams window2_params(Browser::TYPE_TABBED, profile.get());
  window2_params.window = browser_window2.get();
  scoped_ptr<Browser> browser2(new Browser(window2_params));

  // A tabbed window should now take the top level window state.
  EXPECT_EQ(ui::SHOW_STATE_DEFAULT,
            GetWindowShowState(ui::SHOW_STATE_MAXIMIZED,
                               ui::SHOW_STATE_DEFAULT,
                               BOTH,
                               browser2.get(),
                               p1600x1200));
  // Non tabbed windows should always follow the window saved visibility state.
  EXPECT_EQ(ui::SHOW_STATE_MAXIMIZED,
            GetWindowShowState(ui::SHOW_STATE_MAXIMIZED,
                               ui::SHOW_STATE_MINIMIZED,
                               BOTH,
                               popup_browser.get(),
                               p1600x1200));

  // In smaller screen resolutions we default to maximized if there is no other
  // window visible.
  int min_size = WindowSizer::GetForceMaximizedWidthLimit() / 2;
  if (min_size > 0) {
    const gfx::Rect tiny_screen(0, 0, min_size, min_size);
    EXPECT_EQ(ui::SHOW_STATE_DEFAULT,
              GetWindowShowState(ui::SHOW_STATE_MAXIMIZED,
                                 ui::SHOW_STATE_DEFAULT,
                                 BOTH,
                                 browser2.get(),
                                 tiny_screen));
    window->Hide();
    EXPECT_EQ(ui::SHOW_STATE_MAXIMIZED,
              GetWindowShowState(ui::SHOW_STATE_MAXIMIZED,
                                 ui::SHOW_STATE_DEFAULT,
                                 BOTH,
                                 browser2.get(),
                                 tiny_screen));

  }
}

// Test that the default show state override behavior is properly handled.
TEST_F(WindowSizerTestWithBrowser, TestShowStateDefaults) {
  // Creating a browser & window to play with.
  scoped_ptr<aura::Window> window(CreateTestWindowInShellWithId(0));
  window->SetBounds(gfx::Rect(16, 32, 640, 320));

  scoped_ptr<TestingProfile> profile(new TestingProfile());

  scoped_ptr<BrowserWindow> browser_window(
      new TestBrowserWindowAura(window.get()));
  Browser::CreateParams window_params(Browser::TYPE_TABBED, profile.get());
  window_params.window = browser_window.get();
  scoped_ptr<Browser> browser(new Browser(window_params));

  // Create also a popup browser since that behaves slightly different for
  // defaults.
  scoped_ptr<aura::Window> popup(CreateTestWindowInShellWithId(1));
  popup->SetBounds(gfx::Rect(16, 32, 128, 256));

  scoped_ptr<BrowserWindow> browser_popup(
      new TestBrowserWindowAura(popup.get()));
  Browser::CreateParams popup_params(Browser::TYPE_POPUP, profile.get());
  popup_params.window = browser_window.get();
  scoped_ptr<Browser> popup_browser(new Browser(popup_params));

  // Check that a browser creation state always get used if not given as
  // SHOW_STATE_DEFAULT.
  EXPECT_EQ(GetWindowShowState(ui::SHOW_STATE_MAXIMIZED,
                               ui::SHOW_STATE_MAXIMIZED,
                               DEFAULT,
                               browser.get(),
                               p1600x1200), ui::SHOW_STATE_DEFAULT);
  browser->set_initial_show_state(ui::SHOW_STATE_MINIMIZED);
  EXPECT_EQ(GetWindowShowState(ui::SHOW_STATE_MAXIMIZED,
                               ui::SHOW_STATE_MAXIMIZED,
                               BOTH,
                               browser.get(),
                               p1600x1200), ui::SHOW_STATE_MINIMIZED);
  browser->set_initial_show_state(ui::SHOW_STATE_NORMAL);
  EXPECT_EQ(GetWindowShowState(ui::SHOW_STATE_MAXIMIZED,
                               ui::SHOW_STATE_MAXIMIZED,
                               BOTH,
                               browser.get(),
                               p1600x1200), ui::SHOW_STATE_NORMAL);
  browser->set_initial_show_state(ui::SHOW_STATE_MAXIMIZED);
  EXPECT_EQ(GetWindowShowState(ui::SHOW_STATE_NORMAL,
                               ui::SHOW_STATE_NORMAL,
                               BOTH,
                               browser.get(),
                               p1600x1200), ui::SHOW_STATE_MAXIMIZED);

  // Check that setting the maximized command line option is forcing the
  // maximized state.
  CommandLine::ForCurrentProcess()->AppendSwitch(switches::kStartMaximized);

  browser->set_initial_show_state(ui::SHOW_STATE_NORMAL);
  EXPECT_EQ(GetWindowShowState(ui::SHOW_STATE_NORMAL,
                               ui::SHOW_STATE_NORMAL,
                               BOTH,
                               browser.get(),
                               p1600x1200), ui::SHOW_STATE_MAXIMIZED);

  // The popup should favor the initial show state over the command line.
  EXPECT_EQ(GetWindowShowState(ui::SHOW_STATE_NORMAL,
                               ui::SHOW_STATE_NORMAL,
                               BOTH,
                               popup_browser.get(),
                               p1600x1200), ui::SHOW_STATE_NORMAL);
}
