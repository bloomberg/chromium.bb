// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_frame_ash.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "ui/aura/window.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace {

class BrowserTestParam : public InProcessBrowserTest,
                         public testing::WithParamInterface<bool> {
 public:
  BrowserTestParam() {}
  ~BrowserTestParam() {}

  bool CreateV1App() { return GetParam(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserTestParam);
};

}  // namespace

// Test that in Chrome OS, for app browser (v1app) windows, the auto management
// logic is disabled while for tabbed browser windows, it is enabled.
IN_PROC_BROWSER_TEST_P(BrowserTestParam,
                       TabbedOrAppBrowserWindowAutoManagementTest) {
  // Default |browser()| is not used by this test.
  browser()->window()->Close();

  // Open a new browser window (app or tabbed depending on a parameter).
  bool test_app = CreateV1App();
  Browser::CreateParams params =
      test_app ? Browser::CreateParams::CreateForApp(
                     "test_browser_app", true /* trusted_source */, gfx::Rect(),
                     browser()->profile(), true)
               : Browser::CreateParams(browser()->profile(), true);
  params.initial_show_state = ui::SHOW_STATE_DEFAULT;
  Browser* browser = new Browser(params);
  gfx::NativeWindow window = browser->window()->GetNativeWindow();
  gfx::Rect original_bounds(gfx::Rect(150, 250, 400, 100));
  window->SetBounds(original_bounds);
  window->Show();

  // For tabbed browser window, it will be centered to work area by auto window
  // mangement logic; for app browser window, it will remain the given bounds.
  gfx::Rect work_area = display::Screen::GetScreen()
                            ->GetDisplayNearestPoint(window->bounds().origin())
                            .work_area();
  gfx::Rect tabbed_expected_bounds = work_area;
  tabbed_expected_bounds.ClampToCenteredSize(original_bounds.size());
  tabbed_expected_bounds.set_y(original_bounds.y());
  if (test_app)
    EXPECT_EQ(original_bounds, window->bounds());
  else
    EXPECT_EQ(tabbed_expected_bounds, window->bounds());

  // Close the browser and re-create the browser window with the same app name.
  browser->window()->Close();
  browser = new Browser(params);
  browser->window()->Show();

  // The newly created browser window should remain the same bounds for each.
  window = browser->window()->GetNativeWindow();
  if (test_app)
    EXPECT_EQ(original_bounds, window->bounds());
  else
    EXPECT_EQ(tabbed_expected_bounds, window->bounds());
}

INSTANTIATE_TEST_CASE_P(BrowserTestTabbedOrApp,
                        BrowserTestParam,
                        testing::Bool());
