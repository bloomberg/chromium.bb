// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_view.h"

#include "build/build_config.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "ui/views/focus/focus_manager.h"

#if defined(USE_AURA)
#include "chrome/browser/ui/browser_window_state.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/display/screen.h"
#endif

using views::FocusManager;

typedef InProcessBrowserTest BrowserViewTest;

// Active window and focus testing is not reliable on Windows crbug.com/79493
#if defined(OS_WIN)
#define MAYBE_FullscreenClearsFocus DISABLED_FullscreenClearsFocus
#else
#define MAYBE_FullscreenClearsFocus FullscreenClearsFocus
#endif
IN_PROC_BROWSER_TEST_F(BrowserViewTest, MAYBE_FullscreenClearsFocus) {
  BrowserView* browser_view = static_cast<BrowserView*>(browser()->window());
  LocationBarView* location_bar_view = browser_view->GetLocationBarView();
  FocusManager* focus_manager = browser_view->GetFocusManager();

  // Focus starts in the location bar or one of its children.
  EXPECT_TRUE(location_bar_view->Contains(focus_manager->GetFocusedView()));

  chrome::ToggleFullscreenMode(browser());
  EXPECT_TRUE(browser_view->IsFullscreen());

  // Focus is released from the location bar.
  EXPECT_FALSE(location_bar_view->Contains(focus_manager->GetFocusedView()));
}

#if defined(USE_AURA)
namespace {

class BrowserViewTestParam : public BrowserViewTest,
                             public testing::WithParamInterface<bool> {
 public:
  bool TestApp() { return GetParam(); }
};

}  // namespace

// Test that docked state is remembered for app browser windows and not
// remembered for tabbed browser windows.
IN_PROC_BROWSER_TEST_P(BrowserViewTestParam, BrowserRemembersDockedState) {
  // Open a new browser window (app or tabbed depending on a parameter).
  bool test_app = TestApp();
  Browser::CreateParams params =
      test_app ? Browser::CreateParams::CreateForApp(
                     "test_browser_app", true /* trusted_source */, gfx::Rect(),
                     browser()->profile())
               : Browser::CreateParams(browser()->profile());
  params.initial_show_state = ui::SHOW_STATE_DEFAULT;
#if defined(USE_ASH)
  const bool kIsAsh = true;
#else
  const bool kIsAsh = false;
#endif
  // Default |browser()| is not used by this test.
  browser()->window()->Close();

  // Create a new app browser
  Browser* browser = new Browser(params);
  ASSERT_TRUE(browser);
  gfx::NativeWindow window = browser->window()->GetNativeWindow();
  gfx::Rect original_bounds(gfx::Rect(150, 250, 400, 100));
  window->SetBounds(original_bounds);
  window->Show();
  // Dock the browser window using |kShowStateKey| property.
  gfx::Rect work_area = display::Screen::GetScreen()
                            ->GetDisplayNearestPoint(window->bounds().origin())
                            .work_area();
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_DOCKED);

  // Saved placement should reflect docked state (for app windows only in Ash).
  gfx::Rect bounds;
  ui::WindowShowState show_state = ui::SHOW_STATE_DEFAULT;
  const views::Widget* widget = views::Widget::GetWidgetForNativeWindow(window);
  widget->widget_delegate()->GetSavedWindowPlacement(widget, &bounds,
                                                     &show_state);
  EXPECT_EQ(kIsAsh && test_app ? ui::SHOW_STATE_DOCKED : ui::SHOW_STATE_DEFAULT,
            show_state);
  // Docking is only relevant on Ash desktop.
  if (!kIsAsh)
    return;

  // Saved placement should reflect restore bounds.
  ASSERT_NE(nullptr, window->GetProperty(aura::client::kRestoreBoundsKey));
  original_bounds = *window->GetProperty(aura::client::kRestoreBoundsKey);
  gfx::Rect expected_bounds = work_area;
  expected_bounds.ClampToCenteredSize(original_bounds.size());
  expected_bounds.set_y(original_bounds.y());
  EXPECT_EQ(expected_bounds.ToString(), bounds.ToString());
  EXPECT_EQ(expected_bounds.ToString(), original_bounds.ToString());

  // Browser window should be docked.
  int width = 250;  // same as DockedWindowLayoutManager::kIdealWidth.
  if (window->delegate() && window->delegate()->GetMinimumSize().width() != 0)
    width = std::max(width, window->delegate()->GetMinimumSize().width());
  expected_bounds = work_area;
  expected_bounds.set_width(width);
  expected_bounds.set_x(work_area.right() - expected_bounds.width());
  EXPECT_EQ(expected_bounds.ToString(), window->GetTargetBounds().ToString());
  EXPECT_EQ(ui::SHOW_STATE_DOCKED,
            window->GetProperty(aura::client::kShowStateKey));
  browser->window()->Close();

  // Newly created browser with the same app name should retain docked state
  // for app browser window but leave it as normal for a tabbed browser.
  browser = new Browser(params);
  ASSERT_TRUE(browser);
  browser->window()->Show();
  window = browser->window()->GetNativeWindow();
  EXPECT_EQ(test_app ? expected_bounds.ToString() : original_bounds.ToString(),
            window->GetTargetBounds().ToString());
  EXPECT_EQ(test_app ? ui::SHOW_STATE_DOCKED : ui::SHOW_STATE_NORMAL,
            window->GetProperty(aura::client::kShowStateKey));

  // Undocking the browser window should restore original size and vertical
  // offset while centering the window horizontally.
  // Tabbed window is already not docked.
  expected_bounds = work_area;
  expected_bounds.ClampToCenteredSize(original_bounds.size());
  expected_bounds.set_y(original_bounds.y());
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_NORMAL);
  EXPECT_EQ(expected_bounds.ToString(), window->GetTargetBounds().ToString());
  EXPECT_EQ(ui::SHOW_STATE_NORMAL,
            window->GetProperty(aura::client::kShowStateKey));
  browser->window()->Close();

  // Re-create the browser window with the same app name.
  browser = new Browser(params);
  ASSERT_TRUE(browser);
  browser->window()->Show();

  // Newly created browser should retain undocked state and bounds.
  window = browser->window()->GetNativeWindow();
  EXPECT_EQ(expected_bounds.ToString(), window->GetTargetBounds().ToString());
  EXPECT_EQ(ui::SHOW_STATE_NORMAL,
            window->GetProperty(aura::client::kShowStateKey));
}

INSTANTIATE_TEST_CASE_P(BrowserViewTestTabbedOrApp,
                        BrowserViewTestParam,
                        testing::Bool());
#endif
