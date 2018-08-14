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
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/web_applications/components/pending_app_manager.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_shortcut_installation_task.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

const char kFooWebAppUrl[] = "https://foo.example";
const char kBarWebAppUrl[] = "https://bar.example";

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

}  // namespace

class TestBookmarkAppShortcutInstallationTask
    : public BookmarkAppShortcutInstallationTask {
 public:
  TestBookmarkAppShortcutInstallationTask(Profile* profile, bool succeeds)
      : BookmarkAppShortcutInstallationTask(profile), succeeds_(succeeds) {}
  ~TestBookmarkAppShortcutInstallationTask() override = default;

  void InstallFromWebContents(
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

  DISALLOW_COPY_AND_ASSIGN(TestBookmarkAppShortcutInstallationTask);
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
    return std::make_unique<TestBookmarkAppShortcutInstallationTask>(profile,
                                                                     true);
  }

  std::unique_ptr<BookmarkAppInstallationTask> CreateFailingInstallationTask(
      Profile* profile) {
    return std::make_unique<TestBookmarkAppShortcutInstallationTask>(profile,
                                                                     false);
  }

  void InstallCallback(const std::string& app_id) {
    install_succeeded_ = !app_id.empty();
  }

 protected:
  void ResetResults() { install_succeeded_ = base::nullopt; }

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

  content::WebContentsTester* web_contents_tester() {
    return web_contents_tester_;
  }

  bool install_succeeded() { return install_succeeded_.value(); }

 private:
  content::WebContentsTester* web_contents_tester_ = nullptr;
  base::Optional<bool> install_succeeded_;

  PendingBookmarkAppManager::WebContentsFactory test_web_contents_creator_;
  PendingBookmarkAppManager::TaskFactory successful_installation_task_creator_;
  PendingBookmarkAppManager::TaskFactory failing_installation_task_creator_;

  DISALLOW_COPY_AND_ASSIGN(PendingBookmarkAppManagerTest);
};

TEST_F(PendingBookmarkAppManagerTest, Install_Succeeds) {
  PendingBookmarkAppManager pending_app_manager(profile());
  pending_app_manager.SetFactoriesForTesting(
      test_web_contents_creator(), successful_installation_task_creator());

  pending_app_manager.Install(
      GetFooAppInfo(),
      base::BindOnce(&PendingBookmarkAppManagerTest::InstallCallback,
                     base::Unretained(this)));

  base::RunLoop().RunUntilIdle();
  web_contents_tester()->NavigateAndCommit(GURL(kFooWebAppUrl));
  web_contents_tester()->TestDidFinishLoad(GURL(kFooWebAppUrl));
  EXPECT_TRUE(install_succeeded());
}

TEST_F(PendingBookmarkAppManagerTest, Install_SucceedsTwice) {
  PendingBookmarkAppManager pending_app_manager(profile());
  pending_app_manager.SetFactoriesForTesting(
      test_web_contents_creator(), successful_installation_task_creator());

  pending_app_manager.Install(
      GetFooAppInfo(),
      base::BindOnce(&PendingBookmarkAppManagerTest::InstallCallback,
                     base::Unretained(this)));

  base::RunLoop().RunUntilIdle();
  web_contents_tester()->NavigateAndCommit(GURL(kFooWebAppUrl));
  web_contents_tester()->TestDidFinishLoad(GURL(kFooWebAppUrl));
  EXPECT_TRUE(install_succeeded());
  ResetResults();

  pending_app_manager.Install(
      GetBarAppInfo(),
      base::BindOnce(&PendingBookmarkAppManagerTest::InstallCallback,
                     base::Unretained(this)));

  base::RunLoop().RunUntilIdle();
  web_contents_tester()->NavigateAndCommit(GURL(kBarWebAppUrl));
  web_contents_tester()->TestDidFinishLoad(GURL(kBarWebAppUrl));
  EXPECT_TRUE(install_succeeded());
}

TEST_F(PendingBookmarkAppManagerTest, Install_PendingSuccessfulTask) {
  PendingBookmarkAppManager pending_app_manager(profile());
  pending_app_manager.SetFactoriesForTesting(
      test_web_contents_creator(), successful_installation_task_creator());

  pending_app_manager.Install(
      GetFooAppInfo(),
      base::BindOnce(&PendingBookmarkAppManagerTest::InstallCallback,
                     base::Unretained(this)));
  pending_app_manager.Install(
      GetBarAppInfo(),
      base::BindOnce(&PendingBookmarkAppManagerTest::InstallCallback,
                     base::Unretained(this)));

  // Finish the first install.
  base::RunLoop().RunUntilIdle();
  web_contents_tester()->NavigateAndCommit(GURL(kFooWebAppUrl));
  web_contents_tester()->TestDidFinishLoad(GURL(kFooWebAppUrl));
  EXPECT_TRUE(install_succeeded());
  ResetResults();

  // Finish the second install.
  base::RunLoop().RunUntilIdle();
  web_contents_tester()->NavigateAndCommit(GURL(kBarWebAppUrl));
  web_contents_tester()->TestDidFinishLoad(GURL(kBarWebAppUrl));
  EXPECT_TRUE(install_succeeded());
}

TEST_F(PendingBookmarkAppManagerTest, Install_PendingFailingTask) {
  PendingBookmarkAppManager pending_app_manager(profile());
  pending_app_manager.SetFactoriesForTesting(
      test_web_contents_creator(), successful_installation_task_creator());

  pending_app_manager.Install(
      GetFooAppInfo(),
      base::BindOnce(&PendingBookmarkAppManagerTest::InstallCallback,
                     base::Unretained(this)));
  pending_app_manager.Install(
      GetBarAppInfo(),
      base::BindOnce(&PendingBookmarkAppManagerTest::InstallCallback,
                     base::Unretained(this)));

  // Fail the first install.
  base::RunLoop().RunUntilIdle();
  web_contents_tester()->NavigateAndCommit(GURL(kBarWebAppUrl));
  web_contents_tester()->TestDidFinishLoad(GURL(kBarWebAppUrl));
  EXPECT_FALSE(install_succeeded());
  ResetResults();

  // Finish the second install.
  base::RunLoop().RunUntilIdle();
  web_contents_tester()->NavigateAndCommit(GURL(kBarWebAppUrl));
  web_contents_tester()->TestDidFinishLoad(GURL(kBarWebAppUrl));
  EXPECT_TRUE(install_succeeded());
}

TEST_F(PendingBookmarkAppManagerTest, Install_ReentrantCallback) {
  PendingBookmarkAppManager pending_app_manager(profile());
  pending_app_manager.SetFactoriesForTesting(
      test_web_contents_creator(), successful_installation_task_creator());

  // Call install with a callback that tries to install another app.
  pending_app_manager.Install(
      GetFooAppInfo(),
      base::BindLambdaForTesting([&](const std::string& app_id) {
        InstallCallback(app_id);
        pending_app_manager.Install(
            GetBarAppInfo(),
            base::BindOnce(&PendingBookmarkAppManagerTest::InstallCallback,
                           base::Unretained(this)));
      }));
  // Finish the first install.
  base::RunLoop().RunUntilIdle();
  web_contents_tester()->NavigateAndCommit(GURL(kFooWebAppUrl));
  web_contents_tester()->TestDidFinishLoad(GURL(kFooWebAppUrl));
  EXPECT_TRUE(install_succeeded());
  ResetResults();

  base::RunLoop().RunUntilIdle();
  web_contents_tester()->NavigateAndCommit(GURL(kBarWebAppUrl));
  web_contents_tester()->TestDidFinishLoad(GURL(kBarWebAppUrl));
  EXPECT_TRUE(install_succeeded());
}

TEST_F(PendingBookmarkAppManagerTest, Install_FailsSameInstallPending) {
  PendingBookmarkAppManager pending_app_manager(profile());
  pending_app_manager.SetFactoriesForTesting(
      test_web_contents_creator(), successful_installation_task_creator());

  pending_app_manager.Install(
      GetFooAppInfo(),
      base::BindOnce(&PendingBookmarkAppManagerTest::InstallCallback,
                     base::Unretained(this)));

  pending_app_manager.Install(
      GetFooAppInfo(),
      base::BindOnce(&PendingBookmarkAppManagerTest::InstallCallback,
                     base::Unretained(this)));

  // The second install should fail; the app is already getting installed.
  EXPECT_FALSE(install_succeeded());
  ResetResults();

  // The original install should still be able to succeed.
  base::RunLoop().RunUntilIdle();
  web_contents_tester()->NavigateAndCommit(GURL(kFooWebAppUrl));
  web_contents_tester()->TestDidFinishLoad(GURL(kFooWebAppUrl));
  EXPECT_TRUE(install_succeeded());
}

TEST_F(PendingBookmarkAppManagerTest, Install_FailsLoadIncorrectURL) {
  PendingBookmarkAppManager pending_app_manager(profile());
  pending_app_manager.SetFactoriesForTesting(
      test_web_contents_creator(), successful_installation_task_creator());

  pending_app_manager.Install(
      GetFooAppInfo(),
      base::BindOnce(&PendingBookmarkAppManagerTest::InstallCallback,
                     base::Unretained(this)));

  base::RunLoop().RunUntilIdle();
  web_contents_tester()->NavigateAndCommit(GURL(kWrongUrl));
  web_contents_tester()->TestDidFinishLoad(GURL(kWrongUrl));
  EXPECT_FALSE(install_succeeded());
}

}  // namespace extensions
