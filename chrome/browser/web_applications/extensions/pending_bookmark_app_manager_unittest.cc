// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/extensions/pending_bookmark_app_manager.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/test/bind_test_util.h"
#include "base/timer/mock_timer.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/web_applications/components/pending_app_manager.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_installation_task.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

const char kFooWebAppUrl[] = "https://foo.example";
const char kBarWebAppUrl[] = "https://bar.example";
const char kQuxWebAppUrl[] = "https://qux.example";

const char kWrongUrl[] = "https://foobar.example";

web_app::PendingAppManager::AppInfo GetFooAppInfo() {
  return web_app::PendingAppManager::AppInfo(
      GURL(kFooWebAppUrl), web_app::PendingAppManager::LaunchContainer::kTab);
}

web_app::PendingAppManager::AppInfo GetBarAppInfo() {
  return web_app::PendingAppManager::AppInfo(
      GURL(kBarWebAppUrl),
      web_app::PendingAppManager::LaunchContainer::kWindow);
}

web_app::PendingAppManager::AppInfo GetQuxAppInfo() {
  return web_app::PendingAppManager::AppInfo(
      GURL(kQuxWebAppUrl),
      web_app::PendingAppManager::LaunchContainer::kWindow);
}

}  // namespace

class TestBookmarkAppInstallationTask : public BookmarkAppInstallationTask {
 public:
  TestBookmarkAppInstallationTask(Profile* profile, bool succeeds)
      : BookmarkAppInstallationTask(profile), succeeds_(succeeds) {}
  ~TestBookmarkAppInstallationTask() override = default;

  void InstallWebAppOrShortcutFromWebContents(
      content::WebContents* web_contents,
      BookmarkAppInstallationTask::ResultCallback callback) override {
    std::move(callback).Run(
        succeeds_
            ? BookmarkAppInstallationTask::Result(
                  BookmarkAppInstallationTask::ResultCode::kSuccess, "12345")
            : BookmarkAppInstallationTask::Result(
                  BookmarkAppInstallationTask::ResultCode::kInstallationFailed,
                  std::string()));
  }

 private:
  bool succeeds_;

  DISALLOW_COPY_AND_ASSIGN(TestBookmarkAppInstallationTask);
};

class PendingBookmarkAppManagerTest : public ChromeRenderViewHostTestHarness {
 public:
  PendingBookmarkAppManagerTest()
      : test_web_contents_creator_(base::BindRepeating(
            &PendingBookmarkAppManagerTest::CreateTestWebContents,
            base::Unretained(this))),
        successful_installation_task_creator_(base::BindRepeating(
            &PendingBookmarkAppManagerTest::CreateSuccessfulInstallationTask,
            base::Unretained(this))),
        failing_installation_task_creator_(base::BindRepeating(
            &PendingBookmarkAppManagerTest::CreateFailingInstallationTask,
            base::Unretained(this))) {}

  ~PendingBookmarkAppManagerTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    // CrxInstaller in BookmarkAppInstaller needs an ExtensionService, so
    // create one for the profile.
    TestExtensionSystem* test_system =
        static_cast<TestExtensionSystem*>(ExtensionSystem::Get(profile()));
    test_system->CreateExtensionService(base::CommandLine::ForCurrentProcess(),
                                        profile()->GetPath(),
                                        false /* autoupdate_enabled */);
  }

  std::unique_ptr<content::WebContents> CreateTestWebContents(
      Profile* profile) {
    auto web_contents =
        content::WebContentsTester::CreateTestWebContents(profile, nullptr);
    web_contents_tester_ = content::WebContentsTester::For(web_contents.get());
    return web_contents;
  }

  std::unique_ptr<BookmarkAppInstallationTask> CreateSuccessfulInstallationTask(
      Profile* profile) {
    return std::make_unique<TestBookmarkAppInstallationTask>(profile, true);
  }

  std::unique_ptr<BookmarkAppInstallationTask> CreateFailingInstallationTask(
      Profile* profile) {
    return std::make_unique<TestBookmarkAppInstallationTask>(profile, false);
  }

  void InstallCallback(const GURL& url, const std::string& app_id) {
    install_callback_url_ = url;
    install_succeeded_ = !app_id.empty();
  }

 protected:
  void ResetResults() {
    install_succeeded_.reset();
    install_callback_url_.reset();
  }

  const PendingBookmarkAppManager::WebContentsFactory&
  test_web_contents_creator() {
    return test_web_contents_creator_;
  }

  const PendingBookmarkAppManager::TaskFactory&
  successful_installation_task_creator() {
    return successful_installation_task_creator_;
  }

  const PendingBookmarkAppManager::TaskFactory&
  failing_installation_task_creator() {
    return failing_installation_task_creator_;
  }

  std::unique_ptr<PendingBookmarkAppManager>
  GetPendingBookmarkAppManagerWithTestFactories() {
    auto manager = std::make_unique<PendingBookmarkAppManager>(profile());
    manager->SetFactoriesForTesting(test_web_contents_creator(),
                                    successful_installation_task_creator());
    return manager;
  }

  void SuccessfullyLoad(const GURL& url) {
    web_contents_tester_->NavigateAndCommit(url);
    web_contents_tester_->TestDidFinishLoad(url);
  }

  content::WebContentsTester* web_contents_tester() {
    return web_contents_tester_;
  }

  bool install_succeeded() { return install_succeeded_.value(); }

  const GURL& install_callback_url() { return install_callback_url_.value(); }

 private:
  content::WebContentsTester* web_contents_tester_ = nullptr;
  base::Optional<bool> install_succeeded_;
  base::Optional<GURL> install_callback_url_;

  PendingBookmarkAppManager::WebContentsFactory test_web_contents_creator_;
  PendingBookmarkAppManager::TaskFactory successful_installation_task_creator_;
  PendingBookmarkAppManager::TaskFactory failing_installation_task_creator_;

  DISALLOW_COPY_AND_ASSIGN(PendingBookmarkAppManagerTest);
};

TEST_F(PendingBookmarkAppManagerTest, Install_Succeeds) {
  auto pending_app_manager = GetPendingBookmarkAppManagerWithTestFactories();
  pending_app_manager->Install(
      GetFooAppInfo(),
      base::BindOnce(&PendingBookmarkAppManagerTest::InstallCallback,
                     base::Unretained(this)));

  base::RunLoop().RunUntilIdle();
  SuccessfullyLoad(GURL(kFooWebAppUrl));

  EXPECT_TRUE(install_succeeded());
  EXPECT_EQ(GURL(kFooWebAppUrl), install_callback_url());
}

TEST_F(PendingBookmarkAppManagerTest, Install_SucceedsTwice) {
  auto pending_app_manager = GetPendingBookmarkAppManagerWithTestFactories();
  pending_app_manager->Install(
      GetFooAppInfo(),
      base::BindOnce(&PendingBookmarkAppManagerTest::InstallCallback,
                     base::Unretained(this)));

  base::RunLoop().RunUntilIdle();
  SuccessfullyLoad(GURL(kFooWebAppUrl));

  EXPECT_TRUE(install_succeeded());
  EXPECT_EQ(GURL(kFooWebAppUrl), install_callback_url());
  ResetResults();

  pending_app_manager->Install(
      GetBarAppInfo(),
      base::BindOnce(&PendingBookmarkAppManagerTest::InstallCallback,
                     base::Unretained(this)));

  base::RunLoop().RunUntilIdle();
  SuccessfullyLoad(GURL(kBarWebAppUrl));

  EXPECT_TRUE(install_succeeded());
  EXPECT_EQ(GURL(kBarWebAppUrl), install_callback_url());
}

TEST_F(PendingBookmarkAppManagerTest, Install_ConcurrentCalls) {
  auto pending_app_manager = GetPendingBookmarkAppManagerWithTestFactories();
  pending_app_manager->Install(
      GetFooAppInfo(),
      base::BindOnce(&PendingBookmarkAppManagerTest::InstallCallback,
                     base::Unretained(this)));
  pending_app_manager->Install(
      GetBarAppInfo(),
      base::BindOnce(&PendingBookmarkAppManagerTest::InstallCallback,
                     base::Unretained(this)));

  // The last call to Install gets higher priority.
  base::RunLoop().RunUntilIdle();
  SuccessfullyLoad(GURL(kBarWebAppUrl));

  EXPECT_TRUE(install_succeeded());
  ResetResults();

  // Then the first call to Install gets processed.
  base::RunLoop().RunUntilIdle();
  SuccessfullyLoad(GURL(kFooWebAppUrl));

  EXPECT_TRUE(install_succeeded());
  EXPECT_EQ(GURL(kFooWebAppUrl), install_callback_url());
}

TEST_F(PendingBookmarkAppManagerTest, Install_PendingSuccessfulTask) {
  auto pending_app_manager = GetPendingBookmarkAppManagerWithTestFactories();
  pending_app_manager->Install(
      GetFooAppInfo(),
      base::BindOnce(&PendingBookmarkAppManagerTest::InstallCallback,
                     base::Unretained(this)));
  // Make sure the installation has started.
  base::RunLoop().RunUntilIdle();

  pending_app_manager->Install(
      GetBarAppInfo(),
      base::BindOnce(&PendingBookmarkAppManagerTest::InstallCallback,
                     base::Unretained(this)));

  // Finish the first install.
  base::RunLoop().RunUntilIdle();
  SuccessfullyLoad(GURL(kFooWebAppUrl));

  EXPECT_TRUE(install_succeeded());
  EXPECT_EQ(GURL(kFooWebAppUrl), install_callback_url());
  ResetResults();

  // Finish the second install.
  base::RunLoop().RunUntilIdle();
  SuccessfullyLoad(GURL(kBarWebAppUrl));

  EXPECT_TRUE(install_succeeded());
  EXPECT_EQ(GURL(kBarWebAppUrl), install_callback_url());
}

TEST_F(PendingBookmarkAppManagerTest, Install_PendingFailingTask) {
  auto pending_app_manager = GetPendingBookmarkAppManagerWithTestFactories();
  pending_app_manager->Install(
      GetFooAppInfo(),
      base::BindOnce(&PendingBookmarkAppManagerTest::InstallCallback,
                     base::Unretained(this)));
  // Make sure the installation has started.
  base::RunLoop().RunUntilIdle();

  pending_app_manager->Install(
      GetBarAppInfo(),
      base::BindOnce(&PendingBookmarkAppManagerTest::InstallCallback,
                     base::Unretained(this)));

  // Fail the first install.
  base::RunLoop().RunUntilIdle();
  SuccessfullyLoad(GURL(kWrongUrl));

  EXPECT_FALSE(install_succeeded());
  EXPECT_EQ(GURL(kFooWebAppUrl), install_callback_url());
  ResetResults();

  // Finish the second install.
  base::RunLoop().RunUntilIdle();
  SuccessfullyLoad(GURL(kBarWebAppUrl));

  EXPECT_TRUE(install_succeeded());
  EXPECT_EQ(GURL(kBarWebAppUrl), install_callback_url());
}

TEST_F(PendingBookmarkAppManagerTest, Install_ReentrantCallback) {
  auto pending_app_manager = GetPendingBookmarkAppManagerWithTestFactories();
  // Call install with a callback that tries to install another app.
  pending_app_manager->Install(
      GetFooAppInfo(),
      base::BindLambdaForTesting(
          [&](const GURL& provided_url, const std::string& app_id) {
            InstallCallback(provided_url, app_id);
            pending_app_manager->Install(
                GetBarAppInfo(),
                base::BindOnce(&PendingBookmarkAppManagerTest::InstallCallback,
                               base::Unretained(this)));
          }));

  // Finish the first install.
  base::RunLoop().RunUntilIdle();
  SuccessfullyLoad(GURL(kFooWebAppUrl));

  EXPECT_TRUE(install_succeeded());
  EXPECT_EQ(GURL(kFooWebAppUrl), install_callback_url());
  ResetResults();

  base::RunLoop().RunUntilIdle();
  SuccessfullyLoad(GURL(kBarWebAppUrl));

  EXPECT_TRUE(install_succeeded());
  EXPECT_EQ(GURL(kBarWebAppUrl), install_callback_url());
}

TEST_F(PendingBookmarkAppManagerTest, Install_FailsSameInstallPending) {
  auto pending_app_manager = GetPendingBookmarkAppManagerWithTestFactories();
  pending_app_manager->Install(
      GetFooAppInfo(),
      base::BindOnce(&PendingBookmarkAppManagerTest::InstallCallback,
                     base::Unretained(this)));

  pending_app_manager->Install(
      GetFooAppInfo(),
      base::BindOnce(&PendingBookmarkAppManagerTest::InstallCallback,
                     base::Unretained(this)));

  // The second install should fail; the app is already getting installed.
  EXPECT_FALSE(install_succeeded());
  EXPECT_EQ(GURL(kFooWebAppUrl), install_callback_url());
  ResetResults();

  // The original install should still be able to succeed.
  base::RunLoop().RunUntilIdle();
  SuccessfullyLoad(GURL(kFooWebAppUrl));

  EXPECT_TRUE(install_succeeded());
  EXPECT_EQ(GURL(kFooWebAppUrl), install_callback_url());
}

TEST_F(PendingBookmarkAppManagerTest, Install_FailsLoadIncorrectURL) {
  auto pending_app_manager = GetPendingBookmarkAppManagerWithTestFactories();
  pending_app_manager->Install(
      GetFooAppInfo(),
      base::BindOnce(&PendingBookmarkAppManagerTest::InstallCallback,
                     base::Unretained(this)));

  base::RunLoop().RunUntilIdle();
  SuccessfullyLoad(GURL(kWrongUrl));
  EXPECT_FALSE(install_succeeded());
  EXPECT_EQ(GURL(kFooWebAppUrl), install_callback_url());
}

TEST_F(PendingBookmarkAppManagerTest, InstallApps_Succeeds) {
  auto pending_app_manager = GetPendingBookmarkAppManagerWithTestFactories();
  std::vector<web_app::PendingAppManager::AppInfo> apps_to_install;
  apps_to_install.push_back(GetFooAppInfo());

  pending_app_manager->InstallApps(
      std::move(apps_to_install),
      base::BindRepeating(&PendingBookmarkAppManagerTest::InstallCallback,
                          base::Unretained(this)));

  base::RunLoop().RunUntilIdle();
  SuccessfullyLoad(GURL(kFooWebAppUrl));

  EXPECT_TRUE(install_succeeded());
  EXPECT_EQ(GURL(kFooWebAppUrl), install_callback_url());
}

TEST_F(PendingBookmarkAppManagerTest, InstallApps_Fails) {
  auto pending_app_manager = GetPendingBookmarkAppManagerWithTestFactories();
  std::vector<web_app::PendingAppManager::AppInfo> apps_to_install;
  apps_to_install.push_back(GetFooAppInfo());

  pending_app_manager->InstallApps(
      std::move(apps_to_install),
      base::BindRepeating(&PendingBookmarkAppManagerTest::InstallCallback,
                          base::Unretained(this)));

  base::RunLoop().RunUntilIdle();
  SuccessfullyLoad(GURL(kWrongUrl));

  EXPECT_FALSE(install_succeeded());
  EXPECT_EQ(GURL(kFooWebAppUrl), install_callback_url());
}

TEST_F(PendingBookmarkAppManagerTest, InstallApps_Multiple) {
  auto pending_app_manager = GetPendingBookmarkAppManagerWithTestFactories();

  std::vector<web_app::PendingAppManager::AppInfo> apps_to_install;
  apps_to_install.push_back(GetFooAppInfo());
  apps_to_install.push_back(GetBarAppInfo());

  pending_app_manager->InstallApps(
      std::move(apps_to_install),
      base::BindRepeating(&PendingBookmarkAppManagerTest::InstallCallback,
                          base::Unretained(this)));

  // Finish the first install.
  base::RunLoop().RunUntilIdle();
  SuccessfullyLoad(GURL(kFooWebAppUrl));

  EXPECT_TRUE(install_succeeded());
  EXPECT_EQ(GURL(kFooWebAppUrl), install_callback_url());
  ResetResults();

  // Finish the second install.
  base::RunLoop().RunUntilIdle();
  SuccessfullyLoad(GURL(kBarWebAppUrl));

  EXPECT_TRUE(install_succeeded());
  EXPECT_EQ(GURL(kBarWebAppUrl), install_callback_url());
}

TEST_F(PendingBookmarkAppManagerTest, InstallApps_PendingInstallApps) {
  auto pending_app_manager = GetPendingBookmarkAppManagerWithTestFactories();

  {
    std::vector<web_app::PendingAppManager::AppInfo> apps_to_install;
    apps_to_install.push_back(GetFooAppInfo());

    pending_app_manager->InstallApps(
        std::move(apps_to_install),
        base::BindRepeating(&PendingBookmarkAppManagerTest::InstallCallback,
                            base::Unretained(this)));
  }

  {
    std::vector<web_app::PendingAppManager::AppInfo> apps_to_install;
    apps_to_install.push_back(GetBarAppInfo());

    pending_app_manager->InstallApps(
        std::move(apps_to_install),
        base::BindRepeating(&PendingBookmarkAppManagerTest::InstallCallback,
                            base::Unretained(this)));
  }

  // Finish the first install.
  base::RunLoop().RunUntilIdle();
  SuccessfullyLoad(GURL(kFooWebAppUrl));

  EXPECT_TRUE(install_succeeded());
  EXPECT_EQ(GURL(kFooWebAppUrl), install_callback_url());
  ResetResults();

  // Finish the second install.
  base::RunLoop().RunUntilIdle();
  SuccessfullyLoad(GURL(kBarWebAppUrl));

  EXPECT_TRUE(install_succeeded());
  EXPECT_EQ(GURL(kBarWebAppUrl), install_callback_url());
}

TEST_F(PendingBookmarkAppManagerTest, Install_PendingMulitpleInstallApps) {
  auto pending_app_manager = GetPendingBookmarkAppManagerWithTestFactories();

  std::vector<web_app::PendingAppManager::AppInfo> apps_to_install;
  apps_to_install.push_back(GetFooAppInfo());
  apps_to_install.push_back(GetBarAppInfo());

  // Queue through InstallApps.
  pending_app_manager->InstallApps(
      std::move(apps_to_install),
      base::BindRepeating(&PendingBookmarkAppManagerTest::InstallCallback,
                          base::Unretained(this)));

  // Queue through Install.
  pending_app_manager->Install(
      GetQuxAppInfo(),
      base::BindOnce(&PendingBookmarkAppManagerTest::InstallCallback,
                     base::Unretained(this)));

  // The install request from Install should be processed first.
  base::RunLoop().RunUntilIdle();
  SuccessfullyLoad(GURL(kQuxWebAppUrl));

  EXPECT_TRUE(install_succeeded());
  EXPECT_EQ(GURL(kQuxWebAppUrl), install_callback_url());
  ResetResults();

  // The install requests from InstallApps should be processed next.
  base::RunLoop().RunUntilIdle();
  SuccessfullyLoad(GURL(kFooWebAppUrl));

  EXPECT_TRUE(install_succeeded());
  EXPECT_EQ(GURL(kFooWebAppUrl), install_callback_url());
  ResetResults();

  base::RunLoop().RunUntilIdle();
  SuccessfullyLoad(GURL(kBarWebAppUrl));

  EXPECT_TRUE(install_succeeded());
  EXPECT_EQ(GURL(kBarWebAppUrl), install_callback_url());
}

TEST_F(PendingBookmarkAppManagerTest, InstallApps_PendingInstall) {
  auto pending_app_manager = GetPendingBookmarkAppManagerWithTestFactories();
  // Queue through Install.
  pending_app_manager->Install(
      GetQuxAppInfo(),
      base::BindOnce(&PendingBookmarkAppManagerTest::InstallCallback,
                     base::Unretained(this)));

  // Queue through InstallApps.
  std::vector<web_app::PendingAppManager::AppInfo> apps_to_install;
  apps_to_install.push_back(GetFooAppInfo());
  apps_to_install.push_back(GetBarAppInfo());

  pending_app_manager->InstallApps(
      std::move(apps_to_install),
      base::BindRepeating(&PendingBookmarkAppManagerTest::InstallCallback,
                          base::Unretained(this)));

  // The install request from Install should be processed first.
  base::RunLoop().RunUntilIdle();
  SuccessfullyLoad(GURL(kQuxWebAppUrl));

  EXPECT_TRUE(install_succeeded());
  EXPECT_EQ(GURL(kQuxWebAppUrl), install_callback_url());
  ResetResults();

  // The install requests from InstallApps should be processed next.
  base::RunLoop().RunUntilIdle();
  SuccessfullyLoad(GURL(kFooWebAppUrl));

  EXPECT_TRUE(install_succeeded());
  EXPECT_EQ(GURL(kFooWebAppUrl), install_callback_url());
  ResetResults();

  base::RunLoop().RunUntilIdle();
  SuccessfullyLoad(GURL(kBarWebAppUrl));

  EXPECT_TRUE(install_succeeded());
  EXPECT_EQ(GURL(kBarWebAppUrl), install_callback_url());
}

TEST_F(PendingBookmarkAppManagerTest, WebContentsLoadTimedOut) {
  auto pending_app_manager = GetPendingBookmarkAppManagerWithTestFactories();
  auto timer_to_pass = std::make_unique<base::MockOneShotTimer>();
  auto* timer = timer_to_pass.get();

  pending_app_manager->SetTimerForTesting(std::move(timer_to_pass));

  // Queue through Install.
  pending_app_manager->Install(
      GetQuxAppInfo(),
      base::BindOnce(&PendingBookmarkAppManagerTest::InstallCallback,
                     base::Unretained(this)));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(timer->IsRunning());

  // Verify that the timer is stopped after a successful load.
  SuccessfullyLoad(GURL(kQuxWebAppUrl));
  EXPECT_FALSE(timer->IsRunning());
  EXPECT_TRUE(install_succeeded());
  EXPECT_EQ(GURL(kQuxWebAppUrl), install_callback_url());
  ResetResults();

  // Queue through Install.
  pending_app_manager->Install(
      GetQuxAppInfo(),
      base::BindOnce(&PendingBookmarkAppManagerTest::InstallCallback,
                     base::Unretained(this)));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(timer->IsRunning());

  // Fire the timer to simulate a failed load.
  timer->Fire();
  EXPECT_FALSE(install_succeeded());
  EXPECT_EQ(GURL(kQuxWebAppUrl), install_callback_url());
  ResetResults();

  // Queue through InstallApps.
  std::vector<web_app::PendingAppManager::AppInfo> apps_to_install;
  apps_to_install.push_back(GetFooAppInfo());
  apps_to_install.push_back(GetBarAppInfo());

  pending_app_manager->InstallApps(
      std::move(apps_to_install),
      base::BindRepeating(&PendingBookmarkAppManagerTest::InstallCallback,
                          base::Unretained(this)));

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(timer->IsRunning());

  // Fire the timer to simulate a failed load.
  timer->Fire();
  EXPECT_FALSE(install_succeeded());
  EXPECT_EQ(GURL(kFooWebAppUrl), install_callback_url());
  ResetResults();

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(timer->IsRunning());

  // Fire the timer to simulate a failed load.
  timer->Fire();
  EXPECT_FALSE(install_succeeded());
  EXPECT_EQ(GURL(kBarWebAppUrl), install_callback_url());
  ResetResults();

  // Ensure a successful load after a timer fire works.
  pending_app_manager->Install(
      GetBarAppInfo(),
      base::BindOnce(&PendingBookmarkAppManagerTest::InstallCallback,
                     base::Unretained(this)));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(timer->IsRunning());

  // Verify that the timer is stopped after a successful load.
  SuccessfullyLoad(GURL(kBarWebAppUrl));
  EXPECT_FALSE(timer->IsRunning());
  EXPECT_TRUE(install_succeeded());
  EXPECT_EQ(GURL(kBarWebAppUrl), install_callback_url());
}

}  // namespace extensions
