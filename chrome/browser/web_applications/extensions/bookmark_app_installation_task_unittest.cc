// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/extensions/bookmark_app_installation_task.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/message_loop/message_loop.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_data_retriever.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_shortcut_installation_task.h"
#include "chrome/common/web_application_info.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/manifest/manifest.h"

namespace extensions {

namespace {

const char kWebAppUrl[] = "https://foo.example";
const char kWebAppTitle[] = "Foo Title";

}  // namespace

class BookmarkAppInstallationTaskTest : public ChromeRenderViewHostTestHarness {
 public:
  BookmarkAppInstallationTaskTest() = default;
  ~BookmarkAppInstallationTaskTest() override = default;

  void OnInstallationTaskResult(base::OnceClosure quit_closure,
                                BookmarkAppInstallationTask::Result result) {
    app_installation_result_ = result;
    std::move(quit_closure).Run();
  }

 protected:
  bool app_installed() {
    return app_installation_result_.value() ==
           BookmarkAppInstallationTask::Result::kSuccess;
  }

  BookmarkAppInstallationTask::Result app_installation_result() {
    return app_installation_result_.value();
  }

 private:
  base::Optional<BookmarkAppInstallationTask::Result> app_installation_result_;

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

TEST_F(BookmarkAppInstallationTaskTest, ShortcutFromContents_Delete) {
  auto installer = std::make_unique<BookmarkAppShortcutInstallationTask>();
  installer->SetDataRetrieverForTesting(
      std::make_unique<TestDataRetriever>(nullptr));

  base::RunLoop run_loop;
  installer->InstallFromWebContents(
      web_contents(),
      base::BindOnce(&BookmarkAppInstallationTaskTest::OnInstallationTaskResult,
                     base::Unretained(this), run_loop.QuitClosure()));
  installer.reset();
  run_loop.RunUntilIdle();

  // Shouldn't crash.
}

TEST_F(BookmarkAppInstallationTaskTest, ShortcutFromContents_NoWebAppInfo) {
  auto installer = std::make_unique<BookmarkAppShortcutInstallationTask>();
  installer->SetDataRetrieverForTesting(
      std::make_unique<TestDataRetriever>(nullptr));

  base::RunLoop run_loop;
  installer->InstallFromWebContents(
      web_contents(),
      base::BindOnce(&BookmarkAppInstallationTaskTest::OnInstallationTaskResult,
                     base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_FALSE(app_installed());
  EXPECT_EQ(BookmarkAppInstallationTask::Result::kGetWebApplicationInfoFailed,
            app_installation_result());
}

TEST_F(BookmarkAppInstallationTaskTest, ShortcutFromContents_NoManifest) {
  auto installer = std::make_unique<BookmarkAppShortcutInstallationTask>();

  WebApplicationInfo info;
  info.app_url = GURL(kWebAppUrl);
  info.title = base::UTF8ToUTF16(kWebAppTitle);
  installer->SetDataRetrieverForTesting(std::make_unique<TestDataRetriever>(
      std::make_unique<WebApplicationInfo>(std::move(info))));

  base::RunLoop run_loop;
  installer->InstallFromWebContents(
      web_contents(),
      base::BindOnce(&BookmarkAppInstallationTaskTest::OnInstallationTaskResult,
                     base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_TRUE(app_installed());
  // TODO(crbug.com/864904): Test that the right app got installed.
}

}  // namespace extensions
