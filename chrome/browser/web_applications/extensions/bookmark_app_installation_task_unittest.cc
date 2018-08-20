// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/extensions/bookmark_app_installation_task.h"

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/extensions/bookmark_app_helper.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/installable/installable_data.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_data_retriever.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_installer.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_shortcut_installation_task.h"
#include "chrome/common/web_application_info.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/common/extension_id.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace extensions {

using Result = BookmarkAppInstallationTask::Result;
using ResultCode = BookmarkAppInstallationTask::ResultCode;

namespace {

const char kWebAppTitle[] = "Foo Title";
const char kWebAppUrl[] = "https://foo.example";

}  // namespace

class TestBookmarkAppHelper : public BookmarkAppHelper {
 public:
  TestBookmarkAppHelper(Profile* profile,
                        WebApplicationInfo web_app_info,
                        content::WebContents* contents,
                        WebappInstallSource install_source)
      : BookmarkAppHelper(profile, web_app_info, contents, install_source) {}
  ~TestBookmarkAppHelper() override {}

  void CompleteInstallableCheck() {
    blink::Manifest manifest;
    InstallableData data = {
        NO_MANIFEST, GURL(),  &manifest, GURL(), nullptr,
        GURL(),      nullptr, false,     false,
    };
    BookmarkAppHelper::OnDidPerformInstallableCheck(data);
  }

  void CompleteIconDownload() {
    BookmarkAppHelper::OnIconsDownloaded(
        true, std::map<GURL, std::vector<SkBitmap>>());
  }

  void FailIconDownload() {
    BookmarkAppHelper::OnIconsDownloaded(
        false, std::map<GURL, std::vector<SkBitmap>>());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestBookmarkAppHelper);
};

class BookmarkAppInstallationTaskTest : public ChromeRenderViewHostTestHarness {
 public:
  BookmarkAppInstallationTaskTest() = default;
  ~BookmarkAppInstallationTaskTest() override = default;

  void OnInstallationTaskResult(base::OnceClosure quit_closure, Result result) {
    app_installation_result_ = std::make_unique<Result>(std::move(result));
    std::move(quit_closure).Run();
  }

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

 protected:
  BookmarkAppInstallationTask::BookmarkAppHelperFactory helper_factory() {
    return base::BindRepeating(
        &BookmarkAppInstallationTaskTest::CreateTestBookmarkAppHelper,
        base::Unretained(this));
  }

  bool app_installed() {
    bool app_installed = app_installation_result_->code == ResultCode::kSuccess;
    EXPECT_NE(app_installed, app_installation_result_->app_id.empty());
    return app_installed;
  }

  TestBookmarkAppHelper& test_helper() { return *test_helper_; }

  const Result& app_installation_result() { return *app_installation_result_; }

 private:
  std::unique_ptr<BookmarkAppHelper> CreateTestBookmarkAppHelper(
      Profile* profile,
      const WebApplicationInfo& web_app_info,
      content::WebContents* web_contents,
      WebappInstallSource install_source) {
    auto helper = std::make_unique<TestBookmarkAppHelper>(
        profile, web_app_info, web_contents, install_source);
    test_helper_ = helper.get();
    return helper;
  }

  TestBookmarkAppHelper* test_helper_;

  std::unique_ptr<Result> app_installation_result_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkAppInstallationTaskTest);
};

// All BookmarkAppDataRetriever operations are async, so this class posts tasks
// when running callbacks to simulate async behavior in tests as well.
class TestDataRetriever : public BookmarkAppDataRetriever {
 public:
  explicit TestDataRetriever(std::unique_ptr<WebApplicationInfo> web_app_info)
      : web_app_info_(std::move(web_app_info)) {}

  ~TestDataRetriever() override = default;

  void GetWebApplicationInfo(content::WebContents* web_contents,
                             GetWebApplicationInfoCallback callback) override {
    DCHECK(web_contents);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), std::move(web_app_info_)));
  }

  void GetIcons(const GURL& app_url,
                const std::vector<GURL>& icon_urls,
                GetIconsCallback callback) override {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  std::vector<WebApplicationInfo::IconInfo>()));
  }

 private:
  std::unique_ptr<WebApplicationInfo> web_app_info_;

  DISALLOW_COPY_AND_ASSIGN(TestDataRetriever);
};

class TestInstaller : public BookmarkAppInstaller {
 public:
  explicit TestInstaller(Profile* profile, bool succeeds)
      : BookmarkAppInstaller(profile), succeeds_(succeeds) {}

  ~TestInstaller() override = default;

  void Install(const WebApplicationInfo& web_app_info,
               ResultCallback callback) override {
    web_app_info_ = web_app_info;
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  succeeds_ ? "12345" : std::string()));
  }

  const WebApplicationInfo& web_app_info() { return web_app_info_.value(); }

 private:
  const bool succeeds_;
  base::Optional<WebApplicationInfo> web_app_info_;

  DISALLOW_COPY_AND_ASSIGN(TestInstaller);
};

TEST_F(BookmarkAppInstallationTaskTest, ShortcutFromContents_Delete) {
  auto task = std::make_unique<BookmarkAppShortcutInstallationTask>(profile());
  task->SetDataRetrieverForTesting(
      std::make_unique<TestDataRetriever>(nullptr));

  base::RunLoop run_loop;
  task->InstallFromWebContents(
      web_contents(),
      base::BindOnce(&BookmarkAppInstallationTaskTest::OnInstallationTaskResult,
                     base::Unretained(this), run_loop.QuitClosure()));
  task.reset();
  run_loop.RunUntilIdle();

  // Shouldn't crash.
}

TEST_F(BookmarkAppInstallationTaskTest, ShortcutFromContents_NoWebAppInfo) {
  auto task = std::make_unique<BookmarkAppShortcutInstallationTask>(profile());
  task->SetDataRetrieverForTesting(
      std::make_unique<TestDataRetriever>(nullptr));

  base::RunLoop run_loop;
  task->InstallFromWebContents(
      web_contents(),
      base::BindOnce(&BookmarkAppInstallationTaskTest::OnInstallationTaskResult,
                     base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_FALSE(app_installed());
  EXPECT_EQ(
      BookmarkAppInstallationTask::ResultCode::kGetWebApplicationInfoFailed,
      app_installation_result().code);
}

TEST_F(BookmarkAppInstallationTaskTest, ShortcutFromContents_NoManifest) {
  auto task = std::make_unique<BookmarkAppShortcutInstallationTask>(profile());

  WebApplicationInfo info;
  info.app_url = GURL(kWebAppUrl);
  info.title = base::UTF8ToUTF16(kWebAppTitle);
  task->SetDataRetrieverForTesting(std::make_unique<TestDataRetriever>(
      std::make_unique<WebApplicationInfo>(std::move(info))));

  auto installer =
      std::make_unique<TestInstaller>(profile(), true /* succeeds */);
  auto* installer_ptr = installer.get();
  task->SetInstallerForTesting(std::move(installer));

  base::RunLoop run_loop;
  task->InstallFromWebContents(
      web_contents(),
      base::BindOnce(&BookmarkAppInstallationTaskTest::OnInstallationTaskResult,
                     base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_TRUE(app_installed());

  const auto& installed_info = installer_ptr->web_app_info();
  EXPECT_EQ(info.app_url, installed_info.app_url);
  EXPECT_EQ(info.title, installed_info.title);
}

TEST_F(BookmarkAppInstallationTaskTest,
       ShortcutFromContents_InstallationFails) {
  auto task = std::make_unique<BookmarkAppShortcutInstallationTask>(profile());

  WebApplicationInfo info;
  info.app_url = GURL(kWebAppUrl);
  info.title = base::UTF8ToUTF16(kWebAppTitle);
  task->SetDataRetrieverForTesting(std::make_unique<TestDataRetriever>(
      std::make_unique<WebApplicationInfo>(std::move(info))));
  task->SetInstallerForTesting(
      std::make_unique<TestInstaller>(profile(), false /* succeeds */));

  base::RunLoop run_loop;
  task->InstallFromWebContents(
      web_contents(),
      base::BindOnce(&BookmarkAppInstallationTaskTest::OnInstallationTaskResult,
                     base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_FALSE(app_installed());
  EXPECT_EQ(BookmarkAppInstallationTask::ResultCode::kInstallationFailed,
            app_installation_result().code);
}

TEST_F(BookmarkAppInstallationTaskTest,
       WebAppOrShortcutFromContents_InstallationSucceeds) {
  auto task = std::make_unique<BookmarkAppInstallationTask>(profile());
  WebApplicationInfo info;
  info.app_url = GURL(kWebAppUrl);
  info.title = base::UTF8ToUTF16(kWebAppTitle);
  task->SetDataRetrieverForTesting(std::make_unique<TestDataRetriever>(
      std::make_unique<WebApplicationInfo>(std::move(info))));
  task->SetBookmarkAppHelperFactoryForTesting(helper_factory());

  task->InstallWebAppOrShortcutFromWebContents(
      web_contents(),
      base::BindOnce(&BookmarkAppInstallationTaskTest::OnInstallationTaskResult,
                     base::Unretained(this), base::DoNothing().Once()));
  content::RunAllTasksUntilIdle();

  test_helper().CompleteInstallableCheck();
  content::RunAllTasksUntilIdle();

  test_helper().CompleteIconDownload();
  content::RunAllTasksUntilIdle();

  EXPECT_TRUE(app_installed());
}

TEST_F(BookmarkAppInstallationTaskTest,
       WebAppOrShortcutFromContents_InstallationFails) {
  auto task = std::make_unique<BookmarkAppInstallationTask>(profile());
  WebApplicationInfo info;
  info.app_url = GURL(kWebAppUrl);
  info.title = base::UTF8ToUTF16(kWebAppTitle);
  task->SetDataRetrieverForTesting(std::make_unique<TestDataRetriever>(
      std::make_unique<WebApplicationInfo>(std::move(info))));
  task->SetBookmarkAppHelperFactoryForTesting(helper_factory());

  task->InstallWebAppOrShortcutFromWebContents(
      web_contents(),
      base::BindOnce(&BookmarkAppInstallationTaskTest::OnInstallationTaskResult,
                     base::Unretained(this), base::DoNothing().Once()));
  content::RunAllTasksUntilIdle();

  test_helper().CompleteInstallableCheck();
  content::RunAllTasksUntilIdle();

  test_helper().FailIconDownload();
  content::RunAllTasksUntilIdle();

  EXPECT_FALSE(app_installed());
}

}  // namespace extensions
