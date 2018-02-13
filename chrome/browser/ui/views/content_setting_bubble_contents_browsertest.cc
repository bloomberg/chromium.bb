// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/content_setting_bubble_contents.h"

#include <algorithm>

#include "base/run_loop.h"
#include "build/build_config.h"
#include "chrome/browser/permissions/permission_request_manager_test_api.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/content_settings/content_setting_image_model.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"

class ContentSettingBubbleContentsBrowserTest : public InProcessBrowserTest {
 protected:
  using Widgets = views::Widget::Widgets;

  GURL GetTestPageUrl(const std::string& name) {
    return ui_test_utils::GetTestUrl(
        base::FilePath().AppendASCII("content_setting_bubble"),
        base::FilePath().AppendASCII(name));
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  bool ExecuteScript(const std::string& script) {
    return content::ExecuteScript(GetWebContents(), script);
  }

  Widgets GetWidgetsNotIn(Widgets widgets) {
    Widgets new_widgets = views::test::WidgetTest::GetAllWidgets();
    Widgets result;
    std::set_difference(new_widgets.begin(), new_widgets.end(), widgets.begin(),
                        widgets.end(), std::inserter(result, result.begin()));
    return result;
  }
};

#if defined(OS_CHROMEOS)
// https://crbug.com/811940
#define MAYBE_HidesAtWebContentsClose DISABLED_HidesAtWebContentsClose
#else
#define MAYBE_HidesAtWebContentsClose HidesAtWebContentsClose
#endif

IN_PROC_BROWSER_TEST_F(ContentSettingBubbleContentsBrowserTest,
                       MAYBE_HidesAtWebContentsClose) {
  // Create a second tab, so that closing the test tab doesn't close the entire
  // browser.
  chrome::NewTab(browser());

  // Navigate to the test page, and have it request and be denied geolocation
  // permissions.
  ui_test_utils::NavigateToURL(browser(), GetTestPageUrl("geolocation.html"));
  PermissionRequestManager::FromWebContents(GetWebContents())
      ->set_auto_response_for_test(PermissionRequestManager::DISMISS);
  ExecuteScript("geolocate();");

  // Press the geolocation icon in the location bar, and make sure a new widget
  // (the content setting bubble) appears.
  LocationBarTesting* bar =
      browser()->window()->GetLocationBar()->GetLocationBarForTesting();
  Widgets before = GetWidgetsNotIn({});
  EXPECT_TRUE(bar->TestContentSettingImagePressed(
      static_cast<size_t>(ContentSettingImageModel::ImageType::GEOLOCATION)));
  EXPECT_EQ(1u, GetWidgetsNotIn(before).size());

  // Now close the tab, and make sure the content setting bubble's widget is
  // gone. Note that window closure in Aura is asynchronous, so it's necessary
  // to spin the runloop here.
  chrome::CloseTab(browser());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, GetWidgetsNotIn(before).size());
}
