// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/ash_switches.h"
#include "base/command_line.h"
#include "chrome/browser/ui/ash/tablet_mode_client.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/web_preferences.h"
#include "content/public/test/browser_test_utils.h"

namespace {

class TabletModePageBehaviorTest : public InProcessBrowserTest {
 public:
  TabletModePageBehaviorTest() = default;
  ~TabletModePageBehaviorTest() override = default;

  // InProcessBrowserTest:
  void SetUpDefaultCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpDefaultCommandLine(command_line);

    command_line->AppendSwitch(ash::switches::kAshEnableTabletMode);
  }

  void ToggleTabletMode() {
    auto* tablet_mode_client = TabletModeClient::Get();
    tablet_mode_client->OnTabletModeToggled(
        !tablet_mode_client->tablet_mode_enabled());
  }

  bool GetTabletModeEnabled() const {
    return TabletModeClient::Get()->tablet_mode_enabled();
  }

  content::WebContents* GetActiveWebContents(Browser* browser) const {
    return browser->tab_strip_model()->GetActiveWebContents();
  }

  content::WebPreferences GetWebKitPreferences(
      const content::WebContents* web_contents) const {
    return web_contents->GetRenderViewHost()->GetWebkitPreferences();
  }

  void ValidateWebPrefs(const content::WebPreferences& web_prefs,
                        bool tablet_mode_enabled) const {
    if (tablet_mode_enabled) {
      EXPECT_TRUE(web_prefs.viewport_enabled);
      EXPECT_TRUE(web_prefs.viewport_meta_enabled);
      EXPECT_TRUE(web_prefs.shrinks_viewport_contents_to_fit);
      EXPECT_EQ(web_prefs.viewport_style, content::ViewportStyle::MOBILE);
      EXPECT_TRUE(web_prefs.main_frame_resizes_are_orientation_changes);
      EXPECT_FLOAT_EQ(web_prefs.default_minimum_page_scale_factor, 0.25f);
      EXPECT_FLOAT_EQ(web_prefs.default_maximum_page_scale_factor, 5.0f);
    } else {
      EXPECT_FALSE(web_prefs.viewport_enabled);
      EXPECT_FALSE(web_prefs.viewport_meta_enabled);
      EXPECT_FALSE(web_prefs.shrinks_viewport_contents_to_fit);
      EXPECT_NE(web_prefs.viewport_style, content::ViewportStyle::MOBILE);
      EXPECT_FALSE(web_prefs.main_frame_resizes_are_orientation_changes);
      EXPECT_FLOAT_EQ(web_prefs.default_minimum_page_scale_factor, 1.0f);
      EXPECT_FLOAT_EQ(web_prefs.default_maximum_page_scale_factor, 4.0f);
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TabletModePageBehaviorTest);
};

IN_PROC_BROWSER_TEST_F(TabletModePageBehaviorTest,
                       TestWebKitPrefsWithTabletModeToggles) {
  EXPECT_FALSE(GetTabletModeEnabled());
  AddBlankTabAndShow(browser());
  auto* web_contents = GetActiveWebContents(browser());
  ASSERT_TRUE(web_contents);

  // Validate that before tablet mode is enabled, mobile-behavior-related prefs
  // are disabled.
  ValidateWebPrefs(GetWebKitPreferences(web_contents),
                   false /* tablet_mode_enabled */);

  // Now enable tablet mode, and expect that the same page's web prefs get
  // updated.
  ToggleTabletMode();
  ASSERT_TRUE(GetTabletModeEnabled());
  ValidateWebPrefs(GetWebKitPreferences(web_contents),
                   true /* tablet_mode_enabled */);

  // Any newly added pages should have the correct tablet mode prefs.
  Browser* browser_2 = CreateBrowser(browser()->profile());
  auto* web_contents_2 = GetActiveWebContents(browser_2);
  ASSERT_TRUE(web_contents_2);
  ValidateWebPrefs(GetWebKitPreferences(web_contents_2),
                   true /* tablet_mode_enabled */);

  // Disable tablet mode and expect both pages's prefs are updated.
  ToggleTabletMode();
  ASSERT_FALSE(GetTabletModeEnabled());
  ValidateWebPrefs(GetWebKitPreferences(web_contents),
                   false /* tablet_mode_enabled */);
  ValidateWebPrefs(GetWebKitPreferences(web_contents_2),
                   false /* tablet_mode_enabled */);
}

}  // namespace
