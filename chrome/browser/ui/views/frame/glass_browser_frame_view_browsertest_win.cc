// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/glass_browser_frame_view.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/browsertest_util.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/hosted_app_button_container.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/web_application_info.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/test_navigation_observer.h"

class HostedAppGlassBrowserFrameViewTest : public InProcessBrowserTest {
 public:
  HostedAppGlassBrowserFrameViewTest() = default;
  ~HostedAppGlassBrowserFrameViewTest() override = default;

  GURL GetAppURL() { return GURL("https://test.org"); }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    scoped_feature_list_.InitAndEnableFeature(features::kDesktopPWAWindowing);
    HostedAppButtonContainer::DisableAnimationForTesting();
  }

  // Windows 7 does not use GlassBrowserFrameView when Aero glass is not
  // enabled. Skip testing in this scenario.
  // TODO(https://crbug.com/863278): Force Aero glass on Windows 7 for this
  // test.
  bool InstallAndLaunchHostedApp() {
    WebApplicationInfo web_app_info;
    web_app_info.app_url = GetAppURL();
    web_app_info.scope = GetAppURL().GetWithoutFilename();
    if (theme_color_)
      web_app_info.theme_color = *theme_color_;

    const extensions::Extension* app =
        extensions::browsertest_util::InstallBookmarkApp(browser()->profile(),
                                                         web_app_info);
    content::TestNavigationObserver navigation_observer(GetAppURL());
    navigation_observer.StartWatchingNewWebContents();
    app_browser_ = extensions::browsertest_util::LaunchAppBrowser(
        browser()->profile(), app);
    navigation_observer.WaitForNavigationFinished();

    browser_view_ = BrowserView::GetBrowserViewForBrowser(app_browser_);
    views::NonClientFrameView* frame_view =
        browser_view_->GetWidget()->non_client_view()->frame_view();

    if (frame_view->GetClassName() != GlassBrowserFrameView::kClassName)
      return false;
    glass_frame_view_ = static_cast<GlassBrowserFrameView*>(frame_view);

    hosted_app_button_container_ =
        glass_frame_view_->GetHostedAppButtonContainerForTesting();
    DCHECK(hosted_app_button_container_);
    DCHECK(hosted_app_button_container_->visible());
    return true;
  }

  base::Optional<SkColor> theme_color_ = SK_ColorBLUE;
  Browser* app_browser_ = nullptr;
  BrowserView* browser_view_ = nullptr;
  GlassBrowserFrameView* glass_frame_view_ = nullptr;
  HostedAppButtonContainer* hosted_app_button_container_ = nullptr;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(HostedAppGlassBrowserFrameViewTest);
};

IN_PROC_BROWSER_TEST_F(HostedAppGlassBrowserFrameViewTest, ThemeColor) {
  if (!InstallAndLaunchHostedApp())
    return;

  DCHECK_EQ(glass_frame_view_->GetTitlebarColor(), *theme_color_);
}

IN_PROC_BROWSER_TEST_F(HostedAppGlassBrowserFrameViewTest, NoThemeColor) {
  theme_color_ = base::nullopt;
  if (!InstallAndLaunchHostedApp())
    return;

  DCHECK_EQ(
      glass_frame_view_->GetTitlebarColor(),
      ThemeProperties::GetDefaultColor(ThemeProperties::COLOR_FRAME, false));
}
