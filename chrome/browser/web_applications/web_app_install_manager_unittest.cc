// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_install_manager.h"

#include <memory>

#include "base/callback.h"
#include "base/run_loop.h"
#include "base/strings/nullable_string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/installable/fake_installable_manager.h"
#include "chrome/browser/installable/installable_data.h"
#include "chrome/browser/installable/installable_manager.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/test/test_data_retriever.h"
#include "chrome/browser/web_applications/test/test_web_app_database.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "url/gurl.h"

namespace web_app {

class WebAppInstallManagerTest : public WebAppTest {
 public:
  void SetUp() override {
    WebAppTest::SetUp();

    database_ = std::make_unique<TestWebAppDatabase>();
    registrar_ = std::make_unique<WebAppRegistrar>(database_.get());
    install_manager_ =
        std::make_unique<WebAppInstallManager>(profile(), registrar_.get());
  }

  void CreateRendererAppInfo(const GURL& url,
                             const std::string name,
                             const std::string description,
                             const GURL& scope,
                             base::Optional<SkColor> theme_color) {
    auto web_app_info = std::make_unique<WebApplicationInfo>();

    web_app_info->app_url = url;
    web_app_info->title = base::UTF8ToUTF16(name);
    web_app_info->description = base::UTF8ToUTF16(description);
    web_app_info->scope = scope;
    web_app_info->theme_color = theme_color;

    install_manager_->SetDataRetrieverForTesting(
        std::make_unique<TestDataRetriever>(std::move(web_app_info)));
  }

  void CreateRendererAppInfo(const GURL& url,
                             const std::string name,
                             const std::string description) {
    CreateRendererAppInfo(url, name, description, GURL(), base::nullopt);
  }

  void CreateDefaultInstallableManager() {
    InstallableManager::CreateForWebContents(web_contents());
    // Required by InstallableManager.
    // Causes eligibility check to return NOT_FROM_SECURE_ORIGIN for GetData.
    SecurityStateTabHelper::CreateForWebContents(web_contents());
  }

  static base::NullableString16 ToNullableUTF16(const std::string& str) {
    return base::NullableString16(base::UTF8ToUTF16(str), false);
  }

 protected:
  std::unique_ptr<TestWebAppDatabase> database_;
  std::unique_ptr<WebAppRegistrar> registrar_;
  std::unique_ptr<WebAppInstallManager> install_manager_;
};

TEST_F(WebAppInstallManagerTest, InstallFromWebContents) {
  EXPECT_EQ(true, AllowWebAppInstallation(profile()));

  const GURL url = GURL("https://example.com/path");
  const std::string name = "Name";
  const std::string description = "Description";
  const GURL scope = GURL("https://example.com/scope");
  const base::Optional<SkColor> theme_color = 0xAABBCCDD;

  const AppId app_id = GenerateAppIdFromURL(url);

  CreateRendererAppInfo(url, name, description, scope, theme_color);
  CreateDefaultInstallableManager();

  base::RunLoop run_loop;
  bool callback_called = false;
  const bool force_shortcut_app = false;

  install_manager_->InstallWebApp(
      web_contents(), force_shortcut_app,
      base::BindLambdaForTesting(
          [&](const AppId& installed_app_id, InstallResultCode code) {
            EXPECT_EQ(InstallResultCode::kSuccess, code);
            EXPECT_EQ(app_id, installed_app_id);
            callback_called = true;
            run_loop.Quit();
          }));
  run_loop.Run();

  EXPECT_TRUE(callback_called);

  WebApp* web_app = registrar_->GetAppById(app_id);
  EXPECT_NE(nullptr, web_app);

  EXPECT_EQ(app_id, web_app->app_id());
  EXPECT_EQ(name, web_app->name());
  EXPECT_EQ(description, web_app->description());
  EXPECT_EQ(url, web_app->launch_url());
  EXPECT_EQ(scope, web_app->scope());
  EXPECT_EQ(theme_color, web_app->theme_color());
}

TEST_F(WebAppInstallManagerTest, GetWebApplicationInfoFailed) {
  install_manager_->SetDataRetrieverForTesting(
      std::make_unique<TestDataRetriever>(
          std::unique_ptr<WebApplicationInfo>()));

  CreateDefaultInstallableManager();

  base::RunLoop run_loop;
  bool callback_called = false;
  const bool force_shortcut_app = false;

  install_manager_->InstallWebApp(
      web_contents(), force_shortcut_app,
      base::BindLambdaForTesting(
          [&](const AppId& installed_app_id, InstallResultCode code) {
            EXPECT_EQ(InstallResultCode::kGetWebApplicationInfoFailed, code);
            EXPECT_EQ(AppId(), installed_app_id);
            callback_called = true;
            run_loop.Quit();
          }));
  run_loop.Run();

  EXPECT_TRUE(callback_called);
}

TEST_F(WebAppInstallManagerTest, WebContentsDestroyed) {
  CreateRendererAppInfo(GURL("https://example.com/path"), "Name",
                        "Description");
  CreateDefaultInstallableManager();

  base::RunLoop run_loop;
  bool callback_called = false;
  const bool force_shortcut_app = false;

  install_manager_->InstallWebApp(
      web_contents(), force_shortcut_app,
      base::BindLambdaForTesting(
          [&](const AppId& installed_app_id, InstallResultCode code) {
            EXPECT_EQ(InstallResultCode::kWebContentsDestroyed, code);
            EXPECT_EQ(AppId(), installed_app_id);
            callback_called = true;
            run_loop.Quit();
          }));

  // Destroy WebContents.
  DeleteContents();
  EXPECT_EQ(nullptr, web_contents());

  run_loop.Run();

  EXPECT_TRUE(callback_called);
}

// TODO(loyso): Convert more tests from bookmark_app_helper_unittest.cc
TEST_F(WebAppInstallManagerTest, InstallableCheck) {
  const std::string renderer_description = "RendererDescription";
  CreateRendererAppInfo(GURL("https://renderer.com/path"), "RendererName",
                        renderer_description,
                        GURL("https://renderer.com/scope"), 0x00);

  const GURL manifest_start_url = GURL("https://example.com/start");
  const AppId app_id = GenerateAppIdFromURL(manifest_start_url);
  const std::string manifest_name = "Name from Manifest";
  const GURL manifest_scope = GURL("https://example.com/scope");
  const base::Optional<SkColor> manifest_theme_color = 0xAABBCCDD;

  auto manifest = std::make_unique<blink::Manifest>();
  manifest->short_name = ToNullableUTF16("Short Name from Manifest");
  manifest->name = ToNullableUTF16(manifest_name);
  manifest->start_url = manifest_start_url;
  manifest->scope = manifest_scope;
  manifest->theme_color = manifest_theme_color;

  FakeInstallableManager::CreateForWebContentsWithManifest(
      web_contents(), NO_ERROR_DETECTED, GURL("https://example.com/manifest"),
      std::move(manifest));

  base::RunLoop run_loop;
  bool callback_called = false;
  const bool force_shortcut_app = false;

  install_manager_->InstallWebApp(
      web_contents(), force_shortcut_app,
      base::BindLambdaForTesting(
          [&](const AppId& installed_app_id, InstallResultCode code) {
            EXPECT_EQ(InstallResultCode::kSuccess, code);
            EXPECT_EQ(app_id, installed_app_id);
            callback_called = true;
            run_loop.Quit();
          }));
  run_loop.Run();

  EXPECT_TRUE(callback_called);

  WebApp* web_app = registrar_->GetAppById(app_id);
  EXPECT_NE(nullptr, web_app);

  // Manifest data overrides Renderer data, except |description|.
  EXPECT_EQ(app_id, web_app->app_id());
  EXPECT_EQ(manifest_name, web_app->name());
  EXPECT_EQ(manifest_start_url, web_app->launch_url());
  EXPECT_EQ(renderer_description, web_app->description());
  EXPECT_EQ(manifest_scope, web_app->scope());
  EXPECT_EQ(manifest_theme_color, web_app->theme_color());
}

}  // namespace web_app
