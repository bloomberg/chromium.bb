// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/bookmark_apps/bookmark_app_install_manager.h"

#include <memory>
#include <utility>

#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/installable/installable_manager.h"
#include "chrome/browser/installable/installable_metrics.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/common/web_application_info.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/browser/extension_system.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

class BookmarkAppInstallManagerTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    install_manager_ = std::make_unique<BookmarkAppInstallManager>(profile());

    extensions::TestExtensionSystem* test_extension_system =
        static_cast<extensions::TestExtensionSystem*>(
            extensions::ExtensionSystem::Get(profile()));

    test_extension_system->CreateExtensionService(
        base::CommandLine::ForCurrentProcess(),
        profile()->GetPath().Append(FILE_PATH_LITERAL("Extensions")),
        /*autoupdate_enabled=*/false);

    InstallableManager::CreateForWebContents(web_contents());
    // Required by InstallableManager.
    // Causes eligibility check to return NOT_FROM_SECURE_ORIGIN for GetData.
    SecurityStateTabHelper::CreateForWebContents(web_contents());
  }

  void TearDown() override {
    // Do not destroy profile and its scoped temp dir: ensure that
    // ConvertWebAppOnFileThread finishes.
    RunExtensionServiceTaskRunner();

    ChromeRenderViewHostTestHarness::TearDown();
  }

  // Waits for a round trip between file task runner used by the profile's
  // extension service and the main thread - used to ensure that all pending
  // file runner task finish,
  void RunExtensionServiceTaskRunner() {
    base::RunLoop run_loop;
    GetExtensionFileTaskRunner()->PostTaskAndReply(FROM_HERE, base::DoNothing(),
                                                   run_loop.QuitClosure());
    run_loop.Run();
  }

 protected:
  std::unique_ptr<BookmarkAppInstallManager> install_manager_;
};

TEST_F(BookmarkAppInstallManagerTest, FromBanner_WebContentsDestroyed) {
  NavigateAndCommit(GURL("https://example.com/path"));

  base::RunLoop run_loop;
  bool callback_called = false;

  install_manager_->InstallWebAppFromBanner(
      web_contents(), WebappInstallSource::MENU_BROWSER_TAB, base::DoNothing(),
      base::BindLambdaForTesting([&](const web_app::AppId& installed_app_id,
                                     web_app::InstallResultCode code) {
        EXPECT_EQ(web_app::InstallResultCode::kWebContentsDestroyed, code);
        EXPECT_EQ(web_app::AppId(), installed_app_id);
        callback_called = true;
        run_loop.Quit();
      }));

  // Destroy WebContents.
  DeleteContents();
  EXPECT_EQ(nullptr, web_contents());

  run_loop.Run();

  EXPECT_TRUE(callback_called);
}

TEST_F(BookmarkAppInstallManagerTest, FromInfo_InstallManagerDestroyed) {
  const GURL url("https://example.com/path");
  NavigateAndCommit(url);

  auto web_application_info = std::make_unique<WebApplicationInfo>();
  web_application_info->app_url = url;

  base::RunLoop run_loop;
  bool callback_called = false;

  install_manager_->InstallWebAppFromInfo(
      std::move(web_application_info),
      /*no_network_install=*/true, WebappInstallSource::ARC,
      base::BindLambdaForTesting([&](const web_app::AppId& installed_app_id,
                                     web_app::InstallResultCode code) {
        EXPECT_EQ(web_app::InstallResultCode::kInstallManagerDestroyed, code);
        EXPECT_EQ(web_app::AppId(), installed_app_id);
        callback_called = true;
        run_loop.Quit();
      }));

  // Destroy InstallManager.
  install_manager_->Reset();
  run_loop.Run();

  install_manager_.reset();
  EXPECT_TRUE(callback_called);
}

}  // namespace extensions
