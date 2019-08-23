// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/pwa_install_view.h"

#include "base/files/file_path.h"
#include "base/test/bind_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/banners/test_app_banner_manager_desktop.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/location_bar/star_view.h"
#include "chrome/browser/ui/views/page_action/omnibox_page_action_icon_container_view.h"
#include "chrome/browser/ui/web_applications/web_app_dialog_utils.h"
#include "chrome/browser/web_applications/components/install_bounce_metric.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/common/referrer.h"
#include "extensions/browser/extension_system.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/network/public/cpp/network_switches.h"
#include "ui/gfx/color_utils.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "components/arc/arc_util.h"
#include "components/arc/test/connection_holder_util.h"
#include "components/arc/test/fake_app_instance.h"
#endif  // defined(OS_CHROMEOS)

class PwaInstallViewBrowserTest : public extensions::ExtensionBrowserTest {
 public:
  PwaInstallViewBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}
  ~PwaInstallViewBrowserTest() override {}

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        features::kDesktopPWAsOmniboxInstall);

    https_server_.AddDefaultHandlers(GetChromeTestDataDir());
    https_server_.RegisterRequestHandler(
        base::BindRepeating(&PwaInstallViewBrowserTest::RequestInterceptor,
                            base::Unretained(this)));
    ASSERT_TRUE(https_server_.Start());

    extensions::ExtensionBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    extensions::ExtensionBrowserTest::SetUpCommandLine(command_line);

#if defined(OS_CHROMEOS)
    arc::SetArcAvailableCommandLineForTesting(command_line);
#endif  // defined(OS_CHROMEOS)

    command_line->AppendSwitchASCII(
        network::switches::kUnsafelyTreatInsecureOriginAsSecure,
        GetInstallableAppURL().GetOrigin().spec());
  }

  void SetUpInProcessBrowserTestFixture() override {
#if defined(OS_CHROMEOS)
    arc::ArcSessionManager::SetUiEnabledForTesting(false);
#endif  // defined(OS_CHROMEOS)
  }

  void SetUpOnMainThread() override {
    extensions::ExtensionBrowserTest::SetUpOnMainThread();

    pwa_install_view_ =
        BrowserView::GetBrowserViewForBrowser(browser())
            ->toolbar_button_provider()
            ->GetOmniboxPageActionIconContainerView()
            ->GetPageActionIconView(PageActionIconType::kPwaInstall);
    EXPECT_FALSE(pwa_install_view_->GetVisible());

    web_contents_ = GetCurrentTab();
    app_banner_manager_ =
        banners::TestAppBannerManagerDesktop::CreateForWebContents(
            web_contents_);
  }

  std::unique_ptr<net::test_server::HttpResponse> RequestInterceptor(
      const net::test_server::HttpRequest& request) {
    if (request.relative_url != intercept_request_path_)
      return nullptr;
    auto http_response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    http_response->set_code(net::HTTP_FOUND);
    http_response->set_content(intercept_request_response_);
    return std::move(http_response);
  }

  content::WebContents* GetCurrentTab() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  content::WebContents* OpenNewTab(const GURL& url,
                                   bool expected_installability) {
    chrome::NewTab(browser());
    content::WebContents* web_contents = GetCurrentTab();
    auto* app_banner_manager =
        banners::TestAppBannerManagerDesktop::CreateForWebContents(
            web_contents);
    DCHECK(!app_banner_manager->WaitForInstallableCheck());

    ui_test_utils::NavigateToURL(browser(), url);
    DCHECK_EQ(app_banner_manager->WaitForInstallableCheck(),
              expected_installability);

    return web_contents;
  }

  GURL GetInstallableAppURL() {
    return https_server_.GetURL("/banners/manifest_test_page.html");
  }

  GURL GetNonInstallableAppURL() {
    return https_server_.GetURL("app.com", "/simple.html");
  }

  void NavigateToURL(const GURL& url) {
    browser()->OpenURL(content::OpenURLParams(
        url, content::Referrer(), WindowOpenDisposition::CURRENT_TAB,
        ui::PAGE_TRANSITION_TYPED, false /* is_renderer_initiated */));
    app_banner_manager_->WaitForInstallableCheckTearDown();
  }

  web_app::AppId ExecutePwaInstallIcon() {
    chrome::SetAutoAcceptPWAInstallConfirmationForTesting(true);

    web_app::AppId app_id;
    base::RunLoop run_loop;
    web_app::SetInstalledCallbackForTesting(base::BindLambdaForTesting(
        [&app_id, &run_loop](const web_app::AppId& installed_app_id,
                             web_app::InstallResultCode code) {
          app_id = installed_app_id;
          run_loop.Quit();
        }));

    pwa_install_view_->ExecuteForTesting();

    run_loop.Run();

    chrome::SetAutoAcceptPWAInstallConfirmationForTesting(false);

    return app_id;
  }

  // Tests that we measure when a user uninstalls a PWA within a "bounce" period
  // of time after installation.
  void TestInstallBounce(base::TimeDelta install_duration, int expected_count) {
    base::HistogramTester histogram_tester;
    base::Time test_time = base::Time::Now();

    NavigateToURL(GetInstallableAppURL());
    ASSERT_TRUE(app_banner_manager_->WaitForInstallableCheck());

    web_app::SetInstallBounceMetricTimeForTesting(test_time);

    const web_app::AppId app_id = ExecutePwaInstallIcon();

    web_app::SetInstallBounceMetricTimeForTesting(test_time + install_duration);

    ASSERT_TRUE(
        extensions::ExtensionSystem::Get(browser()->profile())
            ->extension_service()
            ->UninstallExtension(
                app_id, extensions::UNINSTALL_REASON_FOR_TESTING, nullptr));

    web_app::SetInstallBounceMetricTimeForTesting(base::nullopt);

    std::vector<base::Bucket> expected_buckets;
    if (expected_count > 0) {
      expected_buckets.push_back(
          {static_cast<int>(WebappInstallSource::OMNIBOX_INSTALL_ICON),
           expected_count});
    }
    EXPECT_EQ(histogram_tester.GetAllSamples("Webapp.Install.InstallBounce"),
              expected_buckets);
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;

  net::EmbeddedTestServer https_server_;
  std::string intercept_request_path_;
  std::string intercept_request_response_;

  PageActionIconView* pwa_install_view_ = nullptr;
  content::WebContents* web_contents_ = nullptr;
  banners::TestAppBannerManagerDesktop* app_banner_manager_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(PwaInstallViewBrowserTest);
};

// Tests that the plus icon updates its visibiliy when switching between
// installable/non-installable tabs.
IN_PROC_BROWSER_TEST_F(PwaInstallViewBrowserTest,
                       IconVisibilityAfterTabSwitching) {
  content::WebContents* installable_web_contents =
      OpenNewTab(GetInstallableAppURL(), true);
  content::WebContents* non_installable_web_contents =
      OpenNewTab(GetNonInstallableAppURL(), false);

  chrome::SelectPreviousTab(browser());
  ASSERT_EQ(installable_web_contents, GetCurrentTab());
  EXPECT_TRUE(pwa_install_view_->GetVisible());

  chrome::SelectNextTab(browser());
  ASSERT_EQ(non_installable_web_contents, GetCurrentTab());
  EXPECT_FALSE(pwa_install_view_->GetVisible());
}

// Tests that the plus icon updates its visibiliy once the installability check
// completes.
IN_PROC_BROWSER_TEST_F(PwaInstallViewBrowserTest,
                       IconVisibilityAfterInstallabilityCheck) {
  NavigateToURL(GetInstallableAppURL());
  EXPECT_FALSE(pwa_install_view_->GetVisible());
  ASSERT_TRUE(app_banner_manager_->WaitForInstallableCheck());
  EXPECT_TRUE(pwa_install_view_->GetVisible());

  NavigateToURL(GetNonInstallableAppURL());
  EXPECT_FALSE(pwa_install_view_->GetVisible());
  ASSERT_FALSE(app_banner_manager_->WaitForInstallableCheck());
  EXPECT_FALSE(pwa_install_view_->GetVisible());
}

// Tests that the plus icon animates its label when the installability check
// passes but doesn't animate more than once for the same installability check.
IN_PROC_BROWSER_TEST_F(PwaInstallViewBrowserTest, LabelAnimation) {
  NavigateToURL(GetInstallableAppURL());
  EXPECT_FALSE(pwa_install_view_->GetVisible());
  ASSERT_TRUE(app_banner_manager_->WaitForInstallableCheck());
  EXPECT_TRUE(pwa_install_view_->GetVisible());
  EXPECT_TRUE(pwa_install_view_->is_animating_label());

  chrome::NewTab(browser());
  EXPECT_FALSE(pwa_install_view_->GetVisible());

  chrome::SelectPreviousTab(browser());
  EXPECT_TRUE(pwa_install_view_->GetVisible());
  EXPECT_FALSE(pwa_install_view_->is_animating_label());
}

// Tests that the icon persists while loading the same scope and omits running
// the label animation again.
IN_PROC_BROWSER_TEST_F(PwaInstallViewBrowserTest, NavigateToSameScope) {
  NavigateToURL(https_server_.GetURL("/banners/scope_a/page_1.html"));
  EXPECT_FALSE(pwa_install_view_->GetVisible());
  ASSERT_TRUE(app_banner_manager_->WaitForInstallableCheck());
  EXPECT_TRUE(pwa_install_view_->GetVisible());
  EXPECT_TRUE(pwa_install_view_->is_animating_label());

  NavigateToURL(https_server_.GetURL("/banners/scope_a/page_2.html"));
  EXPECT_TRUE(pwa_install_view_->GetVisible());
  ASSERT_TRUE(app_banner_manager_->WaitForInstallableCheck());
  EXPECT_TRUE(pwa_install_view_->GetVisible());
  EXPECT_FALSE(pwa_install_view_->is_animating_label());
}

// Tests that the icon persists while loading the same scope but goes away when
// the installability check fails.
IN_PROC_BROWSER_TEST_F(PwaInstallViewBrowserTest,
                       NavigateToSameScopeNonInstallable) {
  NavigateToURL(https_server_.GetURL("/banners/scope_a/page_1.html"));
  EXPECT_FALSE(pwa_install_view_->GetVisible());
  ASSERT_TRUE(app_banner_manager_->WaitForInstallableCheck());
  EXPECT_TRUE(pwa_install_view_->GetVisible());
  EXPECT_TRUE(pwa_install_view_->is_animating_label());

  NavigateToURL(https_server_.GetURL("/banners/scope_a/bad_manifest.html"));
  EXPECT_TRUE(pwa_install_view_->GetVisible());
  ASSERT_FALSE(app_banner_manager_->WaitForInstallableCheck());
  EXPECT_FALSE(pwa_install_view_->GetVisible());
  EXPECT_FALSE(pwa_install_view_->is_animating_label());
}

// Tests that the icon and animation resets while loading a different scope.
IN_PROC_BROWSER_TEST_F(PwaInstallViewBrowserTest, NavigateToDifferentScope) {
  NavigateToURL(https_server_.GetURL("/banners/scope_a/page_1.html"));
  EXPECT_FALSE(pwa_install_view_->GetVisible());
  ASSERT_TRUE(app_banner_manager_->WaitForInstallableCheck());
  EXPECT_TRUE(pwa_install_view_->GetVisible());
  EXPECT_TRUE(pwa_install_view_->is_animating_label());

  NavigateToURL(https_server_.GetURL("/banners/scope_b/scope_b.html"));
  EXPECT_FALSE(pwa_install_view_->GetVisible());
  ASSERT_TRUE(app_banner_manager_->WaitForInstallableCheck());
  EXPECT_TRUE(pwa_install_view_->GetVisible());
  EXPECT_TRUE(pwa_install_view_->is_animating_label());
}

// Tests that the icon and animation resets while loading a different empty
// scope.
IN_PROC_BROWSER_TEST_F(PwaInstallViewBrowserTest,
                       NavigateToDifferentEmptyScope) {
  NavigateToURL(https_server_.GetURL("/banners/scope_a/page_1.html"));
  EXPECT_FALSE(pwa_install_view_->GetVisible());
  ASSERT_TRUE(app_banner_manager_->WaitForInstallableCheck());
  EXPECT_TRUE(pwa_install_view_->GetVisible());
  EXPECT_TRUE(pwa_install_view_->is_animating_label());

  NavigateToURL(https_server_.GetURL("/banners/manifest_test_page.html"));
  EXPECT_FALSE(pwa_install_view_->GetVisible());
  ASSERT_TRUE(app_banner_manager_->WaitForInstallableCheck());
  EXPECT_TRUE(pwa_install_view_->GetVisible());
  EXPECT_TRUE(pwa_install_view_->is_animating_label());
}

// Tests that the animation is suppressed for navigations within the same scope
// for an exponentially increasing period of time.
IN_PROC_BROWSER_TEST_F(PwaInstallViewBrowserTest, AnimationSuppression) {
  std::vector<bool> animation_shown_for_day = {
      true,  true,  false, true,  false, false, false, true,
      false, false, false, false, false, false, false, true,
  };
  for (size_t day = 0; day < animation_shown_for_day.size(); ++day) {
    SCOPED_TRACE(testing::Message() << "day: " << day);

    banners::AppBannerManager::SetTimeDeltaForTesting(day);

    NavigateToURL(GetInstallableAppURL());
    ASSERT_TRUE(app_banner_manager_->WaitForInstallableCheck());
    EXPECT_EQ(pwa_install_view_->is_animating_label(),
              animation_shown_for_day[day]);
  }
}

// Tests that the icon label is visible against the omnibox background after the
// native widget becomes active.
IN_PROC_BROWSER_TEST_F(PwaInstallViewBrowserTest, TextContrast) {
  NavigateToURL(GetInstallableAppURL());
  ASSERT_TRUE(app_banner_manager_->WaitForInstallableCheck());
  EXPECT_TRUE(pwa_install_view_->GetVisible());
  EXPECT_TRUE(pwa_install_view_->is_animating_label());

  pwa_install_view_->GetWidget()->OnNativeWidgetActivationChanged(true);

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  SkColor omnibox_background = browser_view->GetLocationBarView()->GetColor(
      OmniboxPart::LOCATION_BAR_BACKGROUND);
  SkColor label_color = pwa_install_view_->GetLabelColorForTesting();
  EXPECT_EQ(SkColorGetA(label_color), SK_AlphaOPAQUE);
  EXPECT_GT(color_utils::GetContrastRatio(omnibox_background, label_color),
            color_utils::kMinimumReadableContrastRatio);
}

IN_PROC_BROWSER_TEST_F(PwaInstallViewBrowserTest, BouncedInstallMeasured) {
  TestInstallBounce(base::TimeDelta::FromMinutes(50), 1);
}

IN_PROC_BROWSER_TEST_F(PwaInstallViewBrowserTest, BouncedInstallIgnored) {
  TestInstallBounce(base::TimeDelta::FromMinutes(70), 0);
}

// Omnibox install promotion should show if there are no viable related apps
// even if prefer_related_applications is true.
IN_PROC_BROWSER_TEST_F(PwaInstallViewBrowserTest, PreferRelatedAppUnknown) {
  NavigateToURL(
      https_server_.GetURL("/banners/manifest_test_page.html?manifest="
                           "manifest_prefer_related_apps_unknown.json"));
  ASSERT_TRUE(app_banner_manager_->WaitForInstallableCheck());

  EXPECT_TRUE(pwa_install_view_->GetVisible());
}

// Omnibox install promotion should not show if prefer_related_applications is
// false but a related Chrome app is installed.
IN_PROC_BROWSER_TEST_F(PwaInstallViewBrowserTest, PreferRelatedChromeApp) {
  NavigateToURL(
      https_server_.GetURL("/banners/manifest_test_page.html?manifest="
                           "manifest_prefer_related_chrome_app.json"));
  ASSERT_TRUE(app_banner_manager_->WaitForInstallableCheck());

  EXPECT_FALSE(pwa_install_view_->GetVisible());
  EXPECT_TRUE(base::EqualsASCII(
      banners::AppBannerManager::GetInstallableWebAppName(web_contents_),
      "Manifest prefer related chrome app"));
}

// Omnibox install promotion should not show if prefer_related_applications is
// true and a Chrome app listed as related.
IN_PROC_BROWSER_TEST_F(PwaInstallViewBrowserTest,
                       ListedRelatedChromeAppInstalled) {
  const extensions::Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("app"));

  intercept_request_path_ = "/banners/manifest_listing_related_chrome_app.json";
  intercept_request_response_ = R"(
    {
      "name": "Manifest listing related chrome app",
      "icons": [
        {
          "src": "/banners/image-512px.png",
          "sizes": "512x512",
          "type": "image/png"
        }
      ],
      "scope": ".",
      "start_url": ".",
      "display": "standalone",
      "prefer_related_applications": false,
      "related_applications": [{
        "platform": "chrome_web_store",
        "id": ")";
  intercept_request_response_ += extension->id();
  intercept_request_response_ += R"(",
        "comment": "This is the id of test/data/extensions/app"
      }]
    }
  )";

  NavigateToURL(https_server_.GetURL(
      "/banners/manifest_test_page.html?manifest=" + intercept_request_path_));
  ASSERT_TRUE(app_banner_manager_->WaitForInstallableCheck());

  EXPECT_FALSE(pwa_install_view_->GetVisible());
  EXPECT_TRUE(base::EqualsASCII(
      banners::AppBannerManager::GetInstallableWebAppName(web_contents_),
      "Manifest listing related chrome app"));
}

#if defined(OS_CHROMEOS)
// Omnibox install promotion should not show if prefer_related_applications is
// true and an ARC app listed as related.
IN_PROC_BROWSER_TEST_F(PwaInstallViewBrowserTest,
                       ListedRelatedAndroidAppInstalled) {
  arc::SetArcPlayStoreEnabledForProfile(browser()->profile(), true);
  ArcAppListPrefs* arc_app_list_prefs =
      ArcAppListPrefs::Get(browser()->profile());
  auto app_instance =
      std::make_unique<arc::FakeAppInstance>(arc_app_list_prefs);
  arc_app_list_prefs->app_connection_holder()->SetInstance(app_instance.get());
  WaitForInstanceReady(arc_app_list_prefs->app_connection_holder());

  // Install test Android app.
  arc::mojom::ArcPackageInfo package;
  package.package_name = "com.example.app";
  app_instance->InstallPackage(package.Clone());

  NavigateToURL(
      https_server_.GetURL("/banners/manifest_test_page.html?manifest="
                           "manifest_listing_related_android_app.json"));
  ASSERT_TRUE(app_banner_manager_->WaitForInstallableCheck());

  EXPECT_FALSE(pwa_install_view_->GetVisible());
  EXPECT_TRUE(base::EqualsASCII(
      banners::AppBannerManager::GetInstallableWebAppName(web_contents_),
      "Manifest listing related android app"));
}
#endif  // defined(OS_CHROMEOS)
