// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_view.h"

#include "base/command_line.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "ui/views/focus/focus_manager.h"

#if defined(USE_ASH)
#include "ash/common/ash_switches.h"
#endif

#if defined(USE_AURA)
#include "chrome/browser/ui/browser_window_state.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window_delegate.h"
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
#if defined(USE_ASH)
  // Enable window docking for this test.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      ash::switches::kAshEnableDockedWindows);
  const bool kIsAsh = true;
#else
  const bool kIsAsh = false;
#endif  // defined(USE_ASH)

  // Open a new browser window (app or tabbed depending on a parameter).
  bool test_app = TestApp();
  Browser::CreateParams params =
      test_app ? Browser::CreateParams::CreateForApp(
                     "test_browser_app", true /* trusted_source */, gfx::Rect(),
                     browser()->profile(), true)
               : Browser::CreateParams(browser()->profile(), true);
  params.initial_show_state = ui::SHOW_STATE_DEFAULT;

  // Default |browser()| is not used by this test.
  browser()->window()->Close();

  // Create a new app browser
  Browser* browser = new Browser(params);
  gfx::NativeWindow window = browser->window()->GetNativeWindow();
  gfx::Rect original_bounds(gfx::Rect(150, 250, 400, 100));
  window->SetBounds(original_bounds);
  window->Show();
  // Dock the browser window using |kShowStateKey| property.
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

  // Newly created browser with the same app name should retain docked state
  // for app browser window but leave it as normal for a tabbed browser.
  browser = new Browser(params);
  browser->window()->Show();
  window = browser->window()->GetNativeWindow();
  EXPECT_EQ(test_app ? ui::SHOW_STATE_DOCKED : ui::SHOW_STATE_NORMAL,
            window->GetProperty(aura::client::kShowStateKey));

  // Undocking the browser window.
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_NORMAL);
  EXPECT_EQ(ui::SHOW_STATE_NORMAL,
            window->GetProperty(aura::client::kShowStateKey));
  browser->window()->Close();

  // Re-create the browser window with the same app name.
  browser = new Browser(params);
  browser->window()->Show();

  // Newly created browser should retain undocked state.
  window = browser->window()->GetNativeWindow();
  EXPECT_EQ(ui::SHOW_STATE_NORMAL,
            window->GetProperty(aura::client::kShowStateKey));
}

INSTANTIATE_TEST_CASE_P(BrowserViewTestTabbedOrApp,
                        BrowserViewTestParam,
                        testing::Bool());
#endif
