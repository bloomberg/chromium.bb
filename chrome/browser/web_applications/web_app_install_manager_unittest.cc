// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_install_manager.h"

#include <memory>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/flat_set.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_icon_generator.h"
#include "chrome/browser/web_applications/components/web_app_utils.h"
#include "chrome/browser/web_applications/test/test_app_registrar.h"
#include "chrome/browser/web_applications/test/test_data_retriever.h"
#include "chrome/browser/web_applications/test/test_install_finalizer.h"
#include "chrome/browser/web_applications/test/test_web_app_url_loader.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace web_app {

namespace {

const GURL kIconUrl{"https://example.com/app.ico"};

}  // namespace

class WebAppInstallManagerTest : public WebAppTest {
 public:
  void SetUp() override {
    WebAppTest::SetUp();

    registrar_ = std::make_unique<TestAppRegistrar>(profile());

    install_finalizer_ = std::make_unique<TestInstallFinalizer>();

    install_manager_ = std::make_unique<WebAppInstallManager>(
        profile(), registrar_.get(), install_finalizer_.get());

    auto test_url_loader = std::make_unique<TestWebAppUrlLoader>();
    test_url_loader_ = test_url_loader.get();
    install_manager_->SetUrlLoaderForTesting(std::move(test_url_loader));
  }

  WebAppInstallManager& install_manager() { return *install_manager_; }
  TestInstallFinalizer& finalizer() { return *install_finalizer_; }
  TestWebAppUrlLoader& url_loader() { return *test_url_loader_; }

  std::unique_ptr<WebApplicationInfo> CreateWebAppInfo(const GURL& url) {
    auto web_app_info = std::make_unique<WebApplicationInfo>();
    web_app_info->app_url = url;
    WebApplicationInfo::IconInfo icon_info;
    icon_info.url = kIconUrl;
    icon_info.width = icon_size::k256;
    web_app_info->icons.push_back(std::move(icon_info));
    return web_app_info;
  }

 private:
  std::unique_ptr<TestAppRegistrar> registrar_;
  std::unique_ptr<WebAppInstallManager> install_manager_;
  std::unique_ptr<TestInstallFinalizer> install_finalizer_;

  // A weak ptr. The original is owned by install_manager_.
  TestWebAppUrlLoader* test_url_loader_ = nullptr;
};

TEST_F(WebAppInstallManagerTest,
       InstallOrUpdateWebAppFromSync_ConcurrentInstalls) {
  const GURL url1{"https://example.com/path"};
  const AppId app1_id = GenerateAppIdFromURL(url1);

  const GURL url2{"https://example.org/path"};
  const AppId app2_id = GenerateAppIdFromURL(url2);

  url_loader().SetNextLoadUrlResult(GURL("about:blank"),
                                    WebAppUrlLoader::Result::kUrlLoaded);

  base::RunLoop run_loop;
  auto barrier_closure = base::BarrierClosure(2u, run_loop.QuitClosure());

  // 1 InstallTask == 1 DataRetriever, their lifetime matches.
  base::flat_set<TestDataRetriever*> task_data_retrievers;

  install_manager().SetDataRetrieverFactoryForTesting(
      base::BindLambdaForTesting([&]() {
        auto data_retriever = std::make_unique<TestDataRetriever>();

        IconsMap icons_map;
        AddIconToIconsMap(kIconUrl, icon_size::k256, SK_ColorBLUE, &icons_map);
        data_retriever->SetIcons(std::move(icons_map));

        TestDataRetriever* data_retriever_ptr = data_retriever.get();
        task_data_retrievers.insert(data_retriever_ptr);

        data_retriever->SetDestructionCallback(base::BindLambdaForTesting(
            [&task_data_retrievers, data_retriever_ptr]() {
              // The task has been executed, all icons have been retrieved.
              EXPECT_FALSE(data_retriever_ptr->HasIcons());
              task_data_retrievers.erase(data_retriever_ptr);
            }));

        return std::unique_ptr<WebAppDataRetriever>(std::move(data_retriever));
      }));

  // Enqueue a request to install the 1st app.
  install_manager().InstallOrUpdateWebAppFromSync(
      app1_id, CreateWebAppInfo(url1),
      base::BindLambdaForTesting(
          [&](const AppId& installed_app_id, InstallResultCode code) {
            EXPECT_EQ(InstallResultCode::kSuccess, code);
            EXPECT_EQ(app1_id, installed_app_id);
            barrier_closure.Run();
          }));

  EXPECT_EQ(0u, finalizer().finalize_options_list().size());
  EXPECT_EQ(1u, task_data_retrievers.size());
  // All tasks are queued, no icons retrieved yet.
  for (auto* data_retriever : task_data_retrievers)
    EXPECT_TRUE(data_retriever->HasIcons());

  // Immediately enqueue a request to install the 2nd app, WebContents is not
  // ready.
  install_manager().InstallOrUpdateWebAppFromSync(
      app2_id, CreateWebAppInfo(url2),
      base::BindLambdaForTesting(
          [&](const AppId& installed_app_id, InstallResultCode code) {
            EXPECT_EQ(InstallResultCode::kSuccess, code);
            EXPECT_EQ(app2_id, installed_app_id);
            barrier_closure.Run();
          }));

  EXPECT_EQ(0u, finalizer().finalize_options_list().size());
  EXPECT_EQ(2u, task_data_retrievers.size());
  // All tasks are queued, no icons retrieved yet.
  for (auto* data_retriever : task_data_retrievers)
    EXPECT_TRUE(data_retriever->HasIcons());

  run_loop.Run();

  EXPECT_EQ(0u, task_data_retrievers.size());
  EXPECT_EQ(2u, finalizer().finalize_options_list().size());
}

}  // namespace web_app
