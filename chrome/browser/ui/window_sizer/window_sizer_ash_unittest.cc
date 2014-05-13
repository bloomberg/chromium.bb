// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/scoped_target_root_window.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/test_shell_delegate.h"
#include "ash/wm/window_positioner.h"
#include "ash/wm/window_resizer.h"
#include "ash/wm/window_state.h"
#include "base/compiler_specific.h"
#include "chrome/browser/ui/ash/ash_init.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/window_sizer/window_sizer_common_unittest.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/render_view_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/env.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/gfx/screen.h"
#include "ui/wm/public/activation_client.h"

typedef ash::test::AshTestBase WindowSizerAshTest;

namespace {

// A browser window proxy which is able to associate an aura native window with
// it.
class TestBrowserWindowAura : public TestBrowserWindow {
 public:
  // |native_window| will still be owned by the caller after the constructor
  // was called.
  explicit TestBrowserWindowAura(aura::Window* native_window)
      : native_window_(native_window) {
  }
  virtual ~TestBrowserWindowAura() {}

  // TestBrowserWindow overrides:
  virtual void Show() OVERRIDE {
    native_window_->Show();
    Activate();
  }
  virtual void Hide() OVERRIDE {
    native_window_->Hide();
  }
  virtual void Activate() OVERRIDE {
    aura::client::GetActivationClient(
        native_window_->GetRootWindow())->ActivateWindow(native_window_.get());
  }
  virtual gfx::NativeWindow GetNativeWindow() OVERRIDE {
    return native_window_.get();
  }
  virtual gfx::Rect GetBounds() const OVERRIDE {
    return native_window_->bounds();
  }

  Browser* browser() { return browser_.get(); }

  void CreateBrowser(const Browser::CreateParams& params) {
    Browser::CreateParams create_params = params;
    create_params.window = this;
    browser_.reset(new Browser(create_params));
    if (browser_->is_type_tabbed() || browser_->is_app()) {
      ash::wm::GetWindowState(native_window_.get())->
          set_window_position_managed(true);
    }
  }

 private:
  scoped_ptr<Browser> browser_;
  scoped_ptr<aura::Window> native_window_;

  DISALLOW_COPY_AND_ASSIGN(TestBrowserWindowAura);
};

scoped_ptr<TestBrowserWindowAura> CreateTestBrowserWindow(
    aura::Window* window,
    const gfx::Rect& bounds,
    const Browser::CreateParams& params) {
  if (!bounds.IsEmpty())
    window->SetBounds(bounds);
  scoped_ptr<TestBrowserWindowAura> browser_window(
      new TestBrowserWindowAura(window));
  browser_window->CreateBrowser(params);
  return browser_window.Pass();
}

}  // namespace

// On desktop linux aura, we currently don't use the ash frame, breaking some
// tests which expect ash sizes: http://crbug.com/303862
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
#define MAYBE_DefaultSizeCase DISABLED_DefaultSizeCase
#else
#define MAYBE_DefaultSizeCase DefaultSizeCase
#endif

// Test that the window is sized appropriately for the first run experience
// where the default window bounds calculation is invoked.
TEST_F(WindowSizerAshTest, MAYBE_DefaultSizeCase) {
#if defined(OS_WIN)
  CommandLine::ForCurrentProcess()->AppendSwitch(switches::kOpenAsh);
#endif
  { // 4:3 monitor case, 1024x768, no taskbar
    gfx::Rect window_bounds;
    GetWindowBounds(p1024x768, p1024x768, gfx::Rect(), gfx::Rect(),
                    gfx::Rect(), DEFAULT, NULL, gfx::Rect(), &window_bounds);
    EXPECT_EQ(gfx::Rect(ash::WindowPositioner::kDesktopBorderSize,
                        ash::WindowPositioner::kDesktopBorderSize,
                        1024 - ash::WindowPositioner::kDesktopBorderSize * 2,
                        768 - ash::WindowPositioner::kDesktopBorderSize),
              window_bounds);
  }

  { // 4:3 monitor case, 1024x768, taskbar on bottom
    gfx::Rect window_bounds;
    GetWindowBounds(p1024x768, taskbar_bottom_work_area, gfx::Rect(),
                    gfx::Rect(), gfx::Rect(), DEFAULT, NULL, gfx::Rect(),
                    &window_bounds);
    EXPECT_EQ(gfx::Rect(ash::WindowPositioner::kDesktopBorderSize,
                        ash::WindowPositioner::kDesktopBorderSize,
                        1024 - ash::WindowPositioner::kDesktopBorderSize * 2,
                        taskbar_bottom_work_area.height() -
                          ash::WindowPositioner::kDesktopBorderSize),
              window_bounds);
  }

  { // 4:3 monitor case, 1024x768, taskbar on right
    gfx::Rect window_bounds;
    GetWindowBounds(p1024x768, taskbar_right_work_area, gfx::Rect(),
                    gfx::Rect(), gfx::Rect(), DEFAULT, NULL, gfx::Rect(),
                    &window_bounds);
    EXPECT_EQ(gfx::Rect(ash::WindowPositioner::kDesktopBorderSize,
                        ash::WindowPositioner::kDesktopBorderSize,
                        taskbar_right_work_area.width() -
                          ash::WindowPositioner::kDesktopBorderSize * 2,
                        768 - ash::WindowPositioner::kDesktopBorderSize),
              window_bounds);
  }

  { // 4:3 monitor case, 1024x768, taskbar on left
    gfx::Rect window_bounds;
    GetWindowBounds(p1024x768, taskbar_left_work_area, gfx::Rect(),
                    gfx::Rect(), gfx::Rect(), DEFAULT, NULL, gfx::Rect(),
                    &window_bounds);
    EXPECT_EQ(gfx::Rect(taskbar_left_work_area.x() +
                          ash::WindowPositioner::kDesktopBorderSize,
                        ash::WindowPositioner::kDesktopBorderSize,
                        taskbar_left_work_area.width() -
                          ash::WindowPositioner::kDesktopBorderSize * 2,
                        taskbar_left_work_area.height() -
                          ash::WindowPositioner::kDesktopBorderSize),
              window_bounds);
  }

  { // 4:3 monitor case, 1024x768, taskbar on top
    gfx::Rect window_bounds;
    GetWindowBounds(p1024x768, taskbar_top_work_area, gfx::Rect(),
                    gfx::Rect(), gfx::Rect(), DEFAULT, NULL, gfx::Rect(),
                    &window_bounds);
    EXPECT_EQ(gfx::Rect(ash::WindowPositioner::kDesktopBorderSize,
                        taskbar_top_work_area.y() +
                          ash::WindowPositioner::kDesktopBorderSize,
                        1024 - ash::WindowPositioner::kDesktopBorderSize * 2,
                        taskbar_top_work_area.height() -
                            ash::WindowPositioner::kDesktopBorderSize),
              window_bounds);
  }

  { // 4:3 monitor case, 1280x1024
    gfx::Rect window_bounds;
    GetWindowBounds(p1280x1024, p1280x1024, gfx::Rect(), gfx::Rect(),
                    gfx::Rect(), DEFAULT, NULL, gfx::Rect(), &window_bounds);
    EXPECT_EQ(gfx::Rect((1280 - ash::WindowPositioner::kMaximumWindowWidth) / 2,
                        ash::WindowPositioner::kDesktopBorderSize,
                        ash::WindowPositioner::kMaximumWindowWidth,
                        1024 - ash::WindowPositioner::kDesktopBorderSize),
              window_bounds);
  }

  { // 4:3 monitor case, 1600x1200
    gfx::Rect window_bounds;
    GetWindowBounds(p1600x1200, p1600x1200, gfx::Rect(), gfx::Rect(),
                    gfx::Rect(), DEFAULT, NULL, gfx::Rect(), &window_bounds);
    EXPECT_EQ(gfx::Rect((1600 - ash::WindowPositioner::kMaximumWindowWidth) / 2,
                        ash::WindowPositioner::kDesktopBorderSize,
                        ash::WindowPositioner::kMaximumWindowWidth,
                        1200 - ash::WindowPositioner::kDesktopBorderSize),
              window_bounds);
  }

  { // 16:10 monitor case, 1680x1050
    gfx::Rect window_bounds;
    GetWindowBounds(p1680x1050, p1680x1050, gfx::Rect(), gfx::Rect(),
                    gfx::Rect(), DEFAULT, NULL, gfx::Rect(), &window_bounds);
    EXPECT_EQ(gfx::Rect((1680 - ash::WindowPositioner::kMaximumWindowWidth) / 2,
                        ash::WindowPositioner::kDesktopBorderSize,
                        ash::WindowPositioner::kMaximumWindowWidth,
                        1050 - ash::WindowPositioner::kDesktopBorderSize),
              window_bounds);
  }

  { // 16:10 monitor case, 1920x1200
    gfx::Rect window_bounds;
    GetWindowBounds(p1920x1200, p1920x1200, gfx::Rect(), gfx::Rect(),
                    gfx::Rect(), DEFAULT, NULL, gfx::Rect(), &window_bounds);
    EXPECT_EQ(gfx::Rect((1920 - ash::WindowPositioner::kMaximumWindowWidth) / 2,
                        ash::WindowPositioner::kDesktopBorderSize,
                        ash::WindowPositioner::kMaximumWindowWidth,
                        1200 - ash::WindowPositioner::kDesktopBorderSize),
              window_bounds);
  }
}

// Test that the next opened window is positioned appropriately given the
// bounds of an existing window of the same type.
TEST_F(WindowSizerAshTest, LastWindowBoundsCase) {
  { // normal, in the middle of the screen somewhere.
    gfx::Rect window_bounds;
    GetWindowBounds(
        p1024x768, p1024x768, gfx::Rect(),
        gfx::Rect(ash::WindowPositioner::kDesktopBorderSize,
                  ash::WindowPositioner::kDesktopBorderSize, 500, 400),
        gfx::Rect(), LAST_ACTIVE, NULL, gfx::Rect(),
        &window_bounds);
    EXPECT_EQ(
        gfx::Rect(kWindowTilePixels + ash::WindowPositioner::kDesktopBorderSize,
                  kWindowTilePixels + ash::WindowPositioner::kDesktopBorderSize,
                  500, 400).ToString(),
        window_bounds.ToString());
  }

  { // taskbar on top.
    gfx::Rect window_bounds;
    GetWindowBounds(
        p1024x768, taskbar_top_work_area, gfx::Rect(),
        gfx::Rect(ash::WindowPositioner::kDesktopBorderSize,
                  ash::WindowPositioner::kDesktopBorderSize, 500, 400),
        gfx::Rect(), LAST_ACTIVE, NULL, gfx::Rect(),
        &window_bounds);
    EXPECT_EQ(
        gfx::Rect(kWindowTilePixels + ash::WindowPositioner::kDesktopBorderSize,
                  std::max(kWindowTilePixels +
                           ash::WindowPositioner::kDesktopBorderSize,
                           34 /* toolbar height */),
                  500, 400).ToString(),
        window_bounds.ToString());
  }

  { // Too small to satisify the minimum visibility condition.
    gfx::Rect window_bounds;
    GetWindowBounds(
        p1024x768, p1024x768, gfx::Rect(),
        gfx::Rect(ash::WindowPositioner::kDesktopBorderSize,
                  ash::WindowPositioner::kDesktopBorderSize, 29, 29),
        gfx::Rect(), LAST_ACTIVE, NULL, gfx::Rect(),
        &window_bounds);
    EXPECT_EQ(
        gfx::Rect(kWindowTilePixels + ash::WindowPositioner::kDesktopBorderSize,
                  kWindowTilePixels + ash::WindowPositioner::kDesktopBorderSize,
                  30 /* not 29 */,
                  30 /* not 29 */).ToString(),
        window_bounds.ToString());
  }


  { // Normal.
    gfx::Rect window_bounds;
    GetWindowBounds(
        p1024x768, p1024x768, gfx::Rect(),
        gfx::Rect(ash::WindowPositioner::kDesktopBorderSize,
                  ash::WindowPositioner::kDesktopBorderSize, 500, 400),
        gfx::Rect(), LAST_ACTIVE, NULL, gfx::Rect(),
        &window_bounds);
    EXPECT_EQ(
        gfx::Rect(kWindowTilePixels + ash::WindowPositioner::kDesktopBorderSize,
                  kWindowTilePixels + ash::WindowPositioner::kDesktopBorderSize,
                  500, 400).ToString(),
        window_bounds.ToString());
  }
}

// Test that the window opened is sized appropriately given persisted sizes.
TEST_F(WindowSizerAshTest, PersistedBoundsCase) {
  { // normal, in the middle of the screen somewhere.
    gfx::Rect initial_bounds(
        ash::WindowPositioner::kDesktopBorderSize,
        ash::WindowPositioner::kDesktopBorderSize, 500, 400);

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
    GetWindowBounds(
        p1024x768, p1024x768, gfx::Rect(),
        gfx::Rect(ash::WindowPositioner::kDesktopBorderSize,
                  ash::WindowPositioner::kDesktopBorderSize, 29, 29),
        gfx::Rect(), PERSISTED, NULL, gfx::Rect(), &window_bounds);
    EXPECT_EQ(gfx::Rect(ash::WindowPositioner::kDesktopBorderSize,
                        ash::WindowPositioner::kDesktopBorderSize,
                        30 /* not 29 */, 30 /* not 29 */).ToString(),
              window_bounds.ToString());
  }
}

//////////////////////////////////////////////////////////////////////////////
// The following unittests have different results on Mac/non-Mac because we
// reposition windows aggressively on Mac.  The *WithAggressiveReposition tests
// are run on Mac, and the *WithNonAggressiveRepositioning tests are run on
// other platforms.

TEST_F(WindowSizerAshTest, LastWindowOffscreenWithNonAggressiveRepositioning) {
  { // taskbar on left.
    gfx::Rect window_bounds;
    GetWindowBounds(
        p1024x768, taskbar_left_work_area, gfx::Rect(),
        gfx::Rect(ash::WindowPositioner::kDesktopBorderSize,
                  ash::WindowPositioner::kDesktopBorderSize, 500, 400),
        gfx::Rect(), LAST_ACTIVE, NULL, gfx::Rect(),
        &window_bounds);
    EXPECT_EQ(
        gfx::Rect(kWindowTilePixels + ash::WindowPositioner::kDesktopBorderSize,
                  kWindowTilePixels + ash::WindowPositioner::kDesktopBorderSize,
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

// On desktop linux aura, we currently don't use the ash frame, breaking some
// tests which expect ash sizes: http://crbug.com/303862
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
#define MAYBE_PlaceNewWindows DISABLED_PlaceNewWindows
#else
#define MAYBE_PlaceNewWindows PlaceNewWindows
#endif

// Test the placement of newly created windows.
TEST_F(WindowSizerAshTest, MAYBE_PlaceNewWindows) {
  // Create a browser which we can use to pass into the GetWindowBounds
  // function.
  scoped_ptr<TestingProfile> profile(new TestingProfile());
  // Creating a popup handler here to make sure it does not interfere with the
  // existing windows.
  Browser::CreateParams native_params(profile.get(),
                                      chrome::HOST_DESKTOP_TYPE_ASH);
  scoped_ptr<Browser> browser(
      chrome::CreateBrowserWithTestWindowForParams(&native_params));

  // Creating a popup handler here to make sure it does not interfere with the
  // existing windows.
  scoped_ptr<BrowserWindow> browser_window(CreateTestBrowserWindow(
      CreateTestWindowInShellWithId(0),
      gfx::Rect(16, 32, 640, 320),
      Browser::CreateParams(profile.get(), chrome::HOST_DESKTOP_TYPE_ASH)));

  // Creating a popup to make sure it does not interfere with the positioning.
  scoped_ptr<TestBrowserWindowAura> browser_popup(CreateTestBrowserWindow(
      CreateTestWindowInShellWithId(1),
      gfx::Rect(16, 32, 128, 256),
      Browser::CreateParams(Browser::TYPE_POPUP, profile.get(),
                            chrome::HOST_DESKTOP_TYPE_ASH)));

  // Creating a panel to make sure it does not interfere with the positioning.
  scoped_ptr<BrowserWindow> browser_panel(CreateTestBrowserWindow(
      CreateTestWindowInShellWithId(2),
      gfx::Rect(32, 48, 256, 512),
      Browser::CreateParams(Browser::TYPE_POPUP, profile.get(),
                            chrome::HOST_DESKTOP_TYPE_ASH)));
  browser_window->Show();
  { // Make sure that popups do not get changed.
    gfx::Rect window_bounds;
    GetWindowBounds(p1600x1200, p1600x1200, gfx::Rect(),
                    gfx::Rect(50, 100, 300, 150), bottom_s1600x1200,
                    PERSISTED, browser_popup->browser(),
                    gfx::Rect(), &window_bounds);
    EXPECT_EQ("50,100 300x150", window_bounds.ToString());
  }

  browser_window->Hide();
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
    EXPECT_EQ(
        gfx::Rect(
            std::max(ash::WindowPositioner::kDesktopBorderSize,
                     (1600 - ash::WindowPositioner::kMaximumWindowWidth) / 2),
            ash::WindowPositioner::kDesktopBorderSize,
            std::min(ash::WindowPositioner::kMaximumWindowWidth,
                     1600 - 2 * ash::WindowPositioner::kDesktopBorderSize),
            1200 - ash::WindowPositioner::kDesktopBorderSize).ToString(),
        window_bounds.ToString());
  }
}

// On desktop linux aura, we currently don't use the ash frame, breaking some
// tests which expect ash sizes: http://crbug.com/303862
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
#define MAYBE_PlaceNewBrowserWindowOnEmptyDesktop DISABLED_PlaceNewBrowserWindowOnEmptyDesktop
#else
#define MAYBE_PlaceNewBrowserWindowOnEmptyDesktop PlaceNewBrowserWindowOnEmptyDesktop
#endif

// Test the placement of newly created windows on an empty desktop.
// This test supplements "PlaceNewWindows" by testing the creation of a newly
// created browser window on an empty desktop.
TEST_F(WindowSizerAshTest, MAYBE_PlaceNewBrowserWindowOnEmptyDesktop) {
  // Create a browser which we can use to pass into the GetWindowBounds
  // function.
  scoped_ptr<TestingProfile> profile(new TestingProfile());
  Browser::CreateParams native_params(profile.get(),
                                      chrome::HOST_DESKTOP_TYPE_ASH);
  scoped_ptr<Browser> browser(
      chrome::CreateBrowserWithTestWindowForParams(&native_params));

  // A common screen size for Chrome OS devices where this behavior is
  // desirable.
  const gfx::Rect p1366x768(0, 0, 1366, 768);

  // If there is no previous state the window should get maximized if the
  // screen is less than or equal to our limit (1366 pixels width).
  gfx::Rect window_bounds;
  ui::WindowShowState out_show_state1 = ui::SHOW_STATE_DEFAULT;
  GetWindowBoundsAndShowState(
      p1366x768,                    // The screen resolution.
      p1366x768,                    // The monitor work area.
      gfx::Rect(),                  // The second monitor.
      gfx::Rect(),                  // The (persisted) bounds.
      p1366x768,                    // The overall work area.
      ui::SHOW_STATE_NORMAL,        // The persisted show state.
      ui::SHOW_STATE_DEFAULT,       // The last show state.
      DEFAULT,                      // No persisted values.
      browser.get(),                // Use this browser.
      gfx::Rect(),                  // Don't request valid bounds.
      &window_bounds,
      &out_show_state1);
  EXPECT_EQ(ui::SHOW_STATE_MAXIMIZED, out_show_state1);

  // If there is a stored coordinate however, that should be taken instead.
  ui::WindowShowState out_show_state2 = ui::SHOW_STATE_DEFAULT;
  GetWindowBoundsAndShowState(
      p1366x768,                    // The screen resolution.
      p1366x768,                    // The monitor work area.
      gfx::Rect(),                  // The second monitor.
      gfx::Rect(50, 100, 300, 150), // The (persisted) bounds.
      p1366x768,                    // The overall work area.
      ui::SHOW_STATE_NORMAL,        // The persisted show state.
      ui::SHOW_STATE_DEFAULT,       // The last show state.
      PERSISTED,                    // Set the persisted values.
      browser.get(),                // Use this browser.
      gfx::Rect(),                  // Don't request valid bounds.
      &window_bounds,
      &out_show_state2);
  EXPECT_EQ(ui::SHOW_STATE_NORMAL, out_show_state2);
  EXPECT_EQ("50,100 300x150", window_bounds.ToString());

  // A larger monitor should not trigger auto-maximize.
  ui::WindowShowState out_show_state3 = ui::SHOW_STATE_DEFAULT;
  GetWindowBoundsAndShowState(
      p1600x1200,                   // The screen resolution.
      p1600x1200,                   // The monitor work area.
      gfx::Rect(),                  // The second monitor.
      gfx::Rect(),                  // The (persisted) bounds.
      p1600x1200,                   // The overall work area.
      ui::SHOW_STATE_NORMAL,        // The persisted show state.
      ui::SHOW_STATE_DEFAULT,       // The last show state.
      DEFAULT,                      // No persisted values.
      browser.get(),                // Use this browser.
      gfx::Rect(),                  // Don't request valid bounds.
      &window_bounds,
      &out_show_state3);
#if defined(OS_WIN)
  EXPECT_EQ(ui::SHOW_STATE_MAXIMIZED, out_show_state3);
#else
  EXPECT_EQ(ui::SHOW_STATE_DEFAULT, out_show_state3);
#endif
}

#if defined(OS_CHROMEOS)
#define MAYBE_PlaceNewWindowsOnMultipleDisplays PlaceNewWindowsOnMultipleDisplays
#else
// No multiple displays on windows ash.
#define MAYBE_PlaceNewWindowsOnMultipleDisplays DISABLED_PlaceNewWindowsOnMultipleDisplays
#endif

// Test the placement of newly created windows on multiple dislays.
TEST_F(WindowSizerAshTest, MAYBE_PlaceNewWindowsOnMultipleDisplays) {
  UpdateDisplay("1600x1200,1600x1200");
  gfx::Rect primary_bounds = ash::Shell::GetInstance()->GetScreen()->
      GetPrimaryDisplay().bounds();
  gfx::Rect secondary_bounds = ash::ScreenUtil::GetSecondaryDisplay().bounds();

  ash::Shell::GetInstance()->set_target_root_window(
      ash::Shell::GetPrimaryRootWindow());

  scoped_ptr<TestingProfile> profile(new TestingProfile());

  // Create browser windows that are used as reference.
  scoped_ptr<BrowserWindow> browser_window(CreateTestBrowserWindow(
      CreateTestWindowInShellWithId(0),
      gfx::Rect(10, 10, 200, 200),
      Browser::CreateParams(profile.get(), chrome::HOST_DESKTOP_TYPE_ASH)));
  browser_window->Show();
  EXPECT_EQ(browser_window->GetNativeWindow()->GetRootWindow(),
            ash::Shell::GetTargetRootWindow());

  scoped_ptr<BrowserWindow> another_browser_window(CreateTestBrowserWindow(
      CreateTestWindowInShellWithId(1),
      gfx::Rect(400, 10, 300, 300),
      Browser::CreateParams(profile.get(), chrome::HOST_DESKTOP_TYPE_ASH)));
  another_browser_window->Show();

  // Creating a new window to verify the new placement.
  scoped_ptr<TestBrowserWindowAura> new_browser_window(CreateTestBrowserWindow(
      CreateTestWindowInShellWithId(0),
      gfx::Rect(),
      Browser::CreateParams(profile.get(),
                            chrome::HOST_DESKTOP_TYPE_ASH)));

  // Make sure the primary root is active.
  ASSERT_EQ(ash::Shell::GetPrimaryRootWindow(),
            ash::Shell::GetTargetRootWindow());

  // First new window should be in the primary.
  {
    gfx::Rect window_bounds;
    GetWindowBounds(p1600x1200, p1600x1200, secondary_bounds,
                    gfx::Rect(), secondary_bounds,
                    PERSISTED, new_browser_window->browser(),
                    gfx::Rect(), &window_bounds);
    // TODO(oshima): Use exact bounds when the window_sizer_ash is
    // moved to ash and changed to include the result from
    // RearrangeVisibleWindowOnShow.
    EXPECT_TRUE(primary_bounds.Contains(window_bounds));
  }

  // Move the window to the right side of the secondary display and create a new
  // window. It should be opened then on the secondary display.
  {
    gfx::Display second_display = ash::Shell::GetScreen()->
        GetDisplayNearestPoint(gfx::Point(1600 + 100,10));
    browser_window->GetNativeWindow()->SetBoundsInScreen(
        gfx::Rect(secondary_bounds.CenterPoint().x() - 100, 10, 200, 200),
        second_display);
    browser_window->Activate();
    EXPECT_NE(ash::Shell::GetPrimaryRootWindow(),
              ash::Shell::GetTargetRootWindow());
    gfx::Rect window_bounds;
    GetWindowBounds(p1600x1200, p1600x1200, secondary_bounds,
                    gfx::Rect(), secondary_bounds,
                    PERSISTED, new_browser_window->browser(),
                    gfx::Rect(), &window_bounds);
    // TODO(oshima): Use exact bounds when the window_sizer_ash is
    // moved to ash and changed to include the result from
    // RearrangeVisibleWindowOnShow.
    EXPECT_TRUE(secondary_bounds.Contains(window_bounds));
  }

  // Activate another window in the primary display and create a new window.
  // It should be created in the primary display.
  {
    another_browser_window->Activate();
    EXPECT_EQ(ash::Shell::GetPrimaryRootWindow(),
              ash::Shell::GetTargetRootWindow());

    gfx::Rect window_bounds;
    GetWindowBounds(p1600x1200, p1600x1200, secondary_bounds,
                    gfx::Rect(), secondary_bounds,
                    PERSISTED, new_browser_window->browser(),
                    gfx::Rect(), &window_bounds);
    // TODO(oshima): Use exact bounds when the window_sizer_ash is
    // moved to ash and changed to include the result from
    // RearrangeVisibleWindowOnShow.
    EXPECT_TRUE(primary_bounds.Contains(window_bounds));
  }
}

// On desktop linux aura, we currently don't use the ash frame, breaking some
// tests which expect ash sizes: http://crbug.com/303862
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
#define MAYBE_TestShowState DISABLED_TestShowState
#else
#define MAYBE_TestShowState TestShowState
#endif

// Test that the show state is properly returned for non default cases.
TEST_F(WindowSizerAshTest, MAYBE_TestShowState) {
  scoped_ptr<TestingProfile> profile(new TestingProfile());

  // Creating a browser & window to play with.
  scoped_ptr<TestBrowserWindowAura> browser_window(CreateTestBrowserWindow(
      CreateTestWindowInShellWithId(0),
      gfx::Rect(16, 32, 640, 320),
      Browser::CreateParams(Browser::TYPE_TABBED, profile.get(),
                            chrome::HOST_DESKTOP_TYPE_ASH)));

  // Create also a popup browser since that behaves different.
  scoped_ptr<TestBrowserWindowAura> browser_popup(CreateTestBrowserWindow(
      CreateTestWindowInShellWithId(1),
      gfx::Rect(16, 32, 640, 320),
      Browser::CreateParams(Browser::TYPE_POPUP, profile.get(),
                            chrome::HOST_DESKTOP_TYPE_ASH)));

  // Tabbed windows should retrieve the saved window state - since there is a
  // top window.
  EXPECT_EQ(ui::SHOW_STATE_MAXIMIZED,
            GetWindowShowState(ui::SHOW_STATE_MAXIMIZED,
                               ui::SHOW_STATE_NORMAL,
                               BOTH,
                               browser_window->browser(),
                               p1600x1200));
  EXPECT_EQ(ui::SHOW_STATE_DEFAULT,
            GetWindowShowState(ui::SHOW_STATE_DEFAULT,
                               ui::SHOW_STATE_NORMAL,
                               BOTH,
                               browser_window->browser(),
                               p1600x1200));
  // Non tabbed windows should always follow the window saved visibility state.
  EXPECT_EQ(ui::SHOW_STATE_MAXIMIZED,
            GetWindowShowState(ui::SHOW_STATE_MAXIMIZED,
                               ui::SHOW_STATE_NORMAL,
                               BOTH,
                               browser_popup->browser(),
                               p1600x1200));
  // The non tabbed window will take the status of the last active of its kind.
  EXPECT_EQ(ui::SHOW_STATE_NORMAL,
            GetWindowShowState(ui::SHOW_STATE_DEFAULT,
                               ui::SHOW_STATE_NORMAL,
                               BOTH,
                               browser_popup->browser(),
                               p1600x1200));

  // Now create a top level window and check again for both. Only the tabbed
  // window should follow the top level window's state.
  // Creating a browser & window to play with.
  scoped_ptr<TestBrowserWindowAura> browser_window2(CreateTestBrowserWindow(
      CreateTestWindowInShellWithId(3),
      gfx::Rect(16, 32, 640, 320),
      Browser::CreateParams(Browser::TYPE_TABBED, profile.get(),
                            chrome::HOST_DESKTOP_TYPE_ASH)));

  // A tabbed window should now take the top level window state.
  EXPECT_EQ(ui::SHOW_STATE_DEFAULT,
            GetWindowShowState(ui::SHOW_STATE_MAXIMIZED,
                               ui::SHOW_STATE_DEFAULT,
                               BOTH,
                               browser_window->browser(),
                               p1600x1200));
  // Non tabbed windows should always follow the window saved visibility state.
  EXPECT_EQ(ui::SHOW_STATE_MAXIMIZED,
            GetWindowShowState(ui::SHOW_STATE_MAXIMIZED,
                               ui::SHOW_STATE_MINIMIZED,
                               BOTH,
                               browser_popup->browser(),
                               p1600x1200));

  // In smaller screen resolutions we default to maximized if there is no other
  // window visible.
  int min_size = ash::WindowPositioner::GetForceMaximizedWidthLimit() / 2;
  if (min_size > 0) {
    const gfx::Rect tiny_screen(0, 0, min_size, min_size);
    EXPECT_EQ(ui::SHOW_STATE_DEFAULT,
              GetWindowShowState(ui::SHOW_STATE_MAXIMIZED,
                                 ui::SHOW_STATE_DEFAULT,
                                 BOTH,
                                 browser_window->browser(),
                                 tiny_screen));
    browser_window->Hide();
    EXPECT_EQ(ui::SHOW_STATE_MAXIMIZED,
              GetWindowShowState(ui::SHOW_STATE_MAXIMIZED,
                                 ui::SHOW_STATE_DEFAULT,
                                 BOTH,
                                 browser_window2->browser(),
                                 tiny_screen));

  }
}

// Test that the default show state override behavior is properly handled.
TEST_F(WindowSizerAshTest, TestShowStateDefaults) {
  // Creating a browser & window to play with.
  scoped_ptr<TestingProfile> profile(new TestingProfile());

  scoped_ptr<TestBrowserWindowAura> browser_window(CreateTestBrowserWindow(
      CreateTestWindowInShellWithId(0),
      gfx::Rect(16, 32, 640, 320),
      Browser::CreateParams(Browser::TYPE_TABBED, profile.get(),
                            chrome::HOST_DESKTOP_TYPE_ASH)));

  // Create also a popup browser since that behaves slightly different for
  // defaults.
  scoped_ptr<TestBrowserWindowAura> browser_popup(CreateTestBrowserWindow(
      CreateTestWindowInShellWithId(1),
      gfx::Rect(16, 32, 128, 256),
      Browser::CreateParams(Browser::TYPE_POPUP, profile.get(),
                            chrome::HOST_DESKTOP_TYPE_ASH)));

  // Check that a browser creation state always get used if not given as
  // SHOW_STATE_DEFAULT. On Windows ASH it should be SHOW_STATE_MAXIMIZED.
  ui::WindowShowState window_show_state =
      GetWindowShowState(ui::SHOW_STATE_MAXIMIZED,
                         ui::SHOW_STATE_MAXIMIZED,
                         DEFAULT,
                         browser_window->browser(),
                         p1600x1200);
#if defined(OS_WIN)
  EXPECT_EQ(window_show_state, ui::SHOW_STATE_MAXIMIZED);
#else
  EXPECT_EQ(window_show_state, ui::SHOW_STATE_DEFAULT);
#endif

  browser_window->browser()->set_initial_show_state(ui::SHOW_STATE_MINIMIZED);
  EXPECT_EQ(GetWindowShowState(ui::SHOW_STATE_MAXIMIZED,
                               ui::SHOW_STATE_MAXIMIZED,
                               BOTH,
                               browser_window->browser(),
                               p1600x1200), ui::SHOW_STATE_MINIMIZED);
  browser_window->browser()->set_initial_show_state(ui::SHOW_STATE_NORMAL);
  EXPECT_EQ(GetWindowShowState(ui::SHOW_STATE_MAXIMIZED,
                               ui::SHOW_STATE_MAXIMIZED,
                               BOTH,
                               browser_window->browser(),
                               p1600x1200), ui::SHOW_STATE_NORMAL);
  browser_window->browser()->set_initial_show_state(ui::SHOW_STATE_MAXIMIZED);
  EXPECT_EQ(GetWindowShowState(ui::SHOW_STATE_NORMAL,
                               ui::SHOW_STATE_NORMAL,
                               BOTH,
                               browser_window->browser(),
                               p1600x1200), ui::SHOW_STATE_MAXIMIZED);

  // Check that setting the maximized command line option is forcing the
  // maximized state.
  CommandLine::ForCurrentProcess()->AppendSwitch(switches::kStartMaximized);

  browser_window->browser()->set_initial_show_state(ui::SHOW_STATE_NORMAL);
  EXPECT_EQ(GetWindowShowState(ui::SHOW_STATE_NORMAL,
                               ui::SHOW_STATE_NORMAL,
                               BOTH,
                               browser_window->browser(),
                               p1600x1200), ui::SHOW_STATE_MAXIMIZED);

  // The popup should favor the initial show state over the command line.
  EXPECT_EQ(GetWindowShowState(ui::SHOW_STATE_NORMAL,
                               ui::SHOW_STATE_NORMAL,
                               BOTH,
                               browser_popup->browser(),
                               p1600x1200), ui::SHOW_STATE_NORMAL);
}

// Test that the target root window is used as the destionation of
// the non browser window. This differ from PersistedBoundsCase
// in that this uses real ash shell implementations + StateProvider
// TargetDisplayProvider, rather than mocks.
TEST_F(WindowSizerAshTest, DefaultBoundsInTargetDisplay) {
  if (!SupportsMultipleDisplays() || !chrome::ShouldOpenAshOnStartup())
    return;
  UpdateDisplay("500x500,600x600");
  {
    aura::Window* first_root =
        ash::Shell::GetAllRootWindows()[0];
    ash::ScopedTargetRootWindow tmp(first_root);
    gfx::Rect bounds;
    ui::WindowShowState show_state;
    WindowSizer::GetBrowserWindowBoundsAndShowState(
        std::string(),
        gfx::Rect(),
        NULL,
        &bounds,
        &show_state);
    EXPECT_TRUE(first_root->GetBoundsInScreen().Contains(bounds));
  }
  {
    aura::Window* second_root =
        ash::Shell::GetAllRootWindows()[1];
    ash::ScopedTargetRootWindow tmp(second_root);
    gfx::Rect bounds;
    ui::WindowShowState show_state;
    WindowSizer::GetBrowserWindowBoundsAndShowState(
        std::string(),
        gfx::Rect(),
        NULL,
        &bounds,
        &show_state);
    EXPECT_TRUE(second_root->GetBoundsInScreen().Contains(bounds));
  }
}

TEST_F(WindowSizerAshTest, TrustedPopupBehavior) {
  scoped_ptr<TestingProfile> profile(new TestingProfile());
  Browser::CreateParams trusted_popup_create_params(
      Browser::TYPE_POPUP, profile.get(), chrome::HOST_DESKTOP_TYPE_ASH);
  trusted_popup_create_params.trusted_source = true;

  scoped_ptr<TestBrowserWindowAura> trusted_popup(CreateTestBrowserWindow(
      CreateTestWindowInShellWithId(1),
      gfx::Rect(16, 32, 640, 320),
      trusted_popup_create_params));
  // Trusted popup windows should follow the saved show state and ignore the
  // last show state.
  EXPECT_EQ(ui::SHOW_STATE_DEFAULT,
            GetWindowShowState(ui::SHOW_STATE_DEFAULT,
                               ui::SHOW_STATE_NORMAL,
                               BOTH,
                               trusted_popup->browser(),
                               p1600x1200));
}
