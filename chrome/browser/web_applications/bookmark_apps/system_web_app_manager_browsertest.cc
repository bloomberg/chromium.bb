// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/bookmark_apps/system_web_app_manager_browsertest.h"

#include <vector>

#include "base/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/browsertest_util.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/ui/extensions/hosted_app_browser_controller.h"
#include "chrome/browser/web_applications/bookmark_apps/test_web_app_provider.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/extensions/pending_bookmark_app_manager.h"
#include "chrome/browser/web_applications/test/test_system_web_app_manager.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/extensions/manifest_handlers/app_theme_color_info.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_controller_factory.h"
#include "content/public/browser/web_ui_data_source.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace web_app {

namespace {

constexpr char kSystemAppManifestText[] =
    R"({
      "name": "Test System App",
      "display": "standalone",
      "icons": [
        {
          "src": "icon-256.png",
          "sizes": "256x256",
          "type": "image/png"
        }
      ],
      "start_url": "/pwa.html",
      "theme_color": "#00FF00"
    })";

constexpr char kPwaHtml[] =
    R"(
<html>
<head>
  <link rel="manifest" href="manifest.json">
</head>
</html>
)";

// WebUIController that serves a System PWA.
class TestWebUIController : public content::WebUIController {
 public:
  explicit TestWebUIController(content::WebUI* web_ui)
      : WebUIController(web_ui) {
    content::WebUIDataSource* data_source =
        content::WebUIDataSource::Create("test-system-app");
    data_source->AddResourcePath("icon-256.png", IDR_PRODUCT_LOGO_256);
    data_source->SetRequestFilter(base::BindRepeating(
        [](const std::string& id,
           const content::WebUIDataSource::GotDataCallback& callback) {
          scoped_refptr<base::RefCountedString> ref_contents(
              new base::RefCountedString);
          if (id == "manifest.json")
            ref_contents->data() = kSystemAppManifestText;
          else if (id == "pwa.html")
            ref_contents->data() = kPwaHtml;
          else
            return false;

          callback.Run(ref_contents);
          return true;
        }));
    content::WebUIDataSource::Add(web_ui->GetWebContents()->GetBrowserContext(),
                                  data_source);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestWebUIController);
};

}  // namespace

// WebUIControllerFactory that serves our TestWebUIController.
class TestWebUIControllerFactory : public content::WebUIControllerFactory {
 public:
  TestWebUIControllerFactory() {}

  std::unique_ptr<content::WebUIController> CreateWebUIControllerForURL(
      content::WebUI* web_ui,
      const GURL& url) const override {
    return std::make_unique<TestWebUIController>(web_ui);
  }

  content::WebUI::TypeID GetWebUIType(content::BrowserContext* browser_context,
                                      const GURL& url) const override {
    return reinterpret_cast<content::WebUI::TypeID>(1);
  }

  bool UseWebUIForURL(content::BrowserContext* browser_context,
                      const GURL& url) const override {
    return true;
  }
  bool UseWebUIBindingsForURL(content::BrowserContext* browser_context,
                              const GURL& url) const override {
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestWebUIControllerFactory);
};

SystemWebAppManagerBrowserTest::SystemWebAppManagerBrowserTest()
    : factory_(std::make_unique<TestWebUIControllerFactory>()),
      test_web_app_provider_creator_(
          base::BindOnce(&SystemWebAppManagerBrowserTest::CreateWebAppProvider,
                         base::Unretained(this))) {
  scoped_feature_list_.InitWithFeatures(
      {features::kDesktopPWAWindowing, features::kSystemWebApps}, {});
  content::WebUIControllerFactory::RegisterFactory(factory_.get());
}

SystemWebAppManagerBrowserTest::~SystemWebAppManagerBrowserTest() {
  content::WebUIControllerFactory::UnregisterFactoryForTesting(factory_.get());
}

std::unique_ptr<KeyedService>
SystemWebAppManagerBrowserTest::CreateWebAppProvider(Profile* profile) {
  DCHECK(SystemWebAppManager::IsEnabled());

  auto provider = std::make_unique<TestWebAppProvider>(profile);
  // Create all real subsystems but do not start them:
  provider->Init();

  // Override SystemWebAppManager with TestSystemWebAppManager:
  DCHECK(!test_system_web_app_manager_);
  auto test_system_web_app_manager = std::make_unique<TestSystemWebAppManager>(
      profile, &provider->pending_app_manager());
  test_system_web_app_manager_ = test_system_web_app_manager.get();
  provider->SetSystemWebAppManager(std::move(test_system_web_app_manager));

  base::flat_map<SystemAppType, GURL> system_apps;
  system_apps[SystemAppType::SETTINGS] =
      GURL("chrome://test-system-app/pwa.html");
  test_system_web_app_manager_->SetSystemApps(std::move(system_apps));

  // Start registry and all dependent subsystems:
  provider->StartRegistry();

  return provider;
}

Browser* SystemWebAppManagerBrowserTest::WaitForSystemAppInstallAndLaunch() {
  Profile* profile = browser()->profile();

  const extensions::Extension* app =
      extensions::TestExtensionRegistryObserver(
          extensions::ExtensionRegistry::Get(profile))
          .WaitForExtensionInstalled();

  return extensions::browsertest_util::LaunchAppBrowser(profile, app);
}

// Test that System Apps install correctly with a manifest.
IN_PROC_BROWSER_TEST_F(SystemWebAppManagerBrowserTest, Install) {
  const extensions::Extension* app = WaitForSystemAppInstallAndLaunch()
                                         ->hosted_app_controller()
                                         ->GetExtensionForTesting();
  EXPECT_EQ("Test System App", app->name());
  EXPECT_EQ(SkColorSetRGB(0, 0xFF, 0),
            extensions::AppThemeColorInfo::GetThemeColor(app));
  EXPECT_TRUE(app->from_bookmark());
  EXPECT_EQ(extensions::Manifest::EXTERNAL_COMPONENT, app->location());

  // The app should be a PWA.
  EXPECT_EQ(extensions::util::GetInstalledPwaForUrl(
                browser()->profile(), GURL("chrome://test-system-app/")),
            app);
  // The app should be retrievable from the Web Apps system.
  EXPECT_EQ(app->id(),
            WebAppProvider::Get(browser()->profile())
                ->system_web_app_manager()
                .GetAppIdForSystemApp(web_app::SystemAppType::SETTINGS));
}

}  // namespace web_app
