// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/extensions/bookmark_app_installation_task.h"

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/extensions/bookmark_app_helper.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/installable/installable_data.h"
#include "chrome/browser/web_applications/bookmark_apps/bookmark_app_install_manager.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/externally_installed_web_app_prefs.h"
#include "chrome/browser/web_applications/components/install_finalizer.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_data_retriever.h"
#include "chrome/browser/web_applications/components/web_app_provider_base.h"
#include "chrome/browser/web_applications/test/test_app_registrar.h"
#include "chrome/browser/web_applications/test/test_data_retriever.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/web_application_info.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/crx_file/id_util.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_id.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "url/gurl.h"

namespace extensions {

using Result = BookmarkAppInstallationTask::Result;

namespace {

const char kWebAppTitle[] = "Foo Title";
const GURL kWebAppUrl("https://foo.example");

// TODO(ortuno): Move this to ExternallyInstalledWebAppPrefs or replace with a
// method in ExternallyInstalledWebAppPrefs once there is one.
bool IsPlaceholderApp(Profile* profile, const GURL& url) {
  const base::Value* map =
      profile->GetPrefs()->GetDictionary(prefs::kWebAppsExtensionIDs);

  const base::Value* entry = map->FindKey(url.spec());

  return entry->FindBoolKey("is_placeholder").value();
}

}  // namespace

class TestBookmarkAppHelper : public BookmarkAppHelper {
 public:
  TestBookmarkAppHelper(Profile* profile,
                        WebApplicationInfo web_app_info,
                        content::WebContents* contents,
                        WebappInstallSource install_source)
      : BookmarkAppHelper(profile, web_app_info, contents, install_source) {}
  ~TestBookmarkAppHelper() override {}

  void CompleteInstallation() {
    CompleteInstallableCheck();
    content::RunAllTasksUntilIdle();
    CompleteIconDownload();
    content::RunAllTasksUntilIdle();
  }

  void CompleteInstallableCheck() {
    blink::Manifest manifest;
    InstallableData data = {
        {NO_MANIFEST},
        GURL::EmptyGURL(),
        &manifest,
        GURL::EmptyGURL(),
        nullptr,
        false,
        GURL::EmptyGURL(),
        nullptr,
        false,
        false,
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

class TestBookmarkAppInstallFinalizer : public web_app::InstallFinalizer {
 public:
  explicit TestBookmarkAppInstallFinalizer(web_app::TestAppRegistrar* registrar)
      : registrar_(registrar) {}
  ~TestBookmarkAppInstallFinalizer() override = default;

  // Returns what would be the AppId if an app is installed with |url|.
  web_app::AppId GetAppIdForUrl(const GURL& url) {
    return crx_file::id_util::GenerateId("fake_app_id_for:" + url.spec());
  }

  void SetNextFinalizeInstallResult(const GURL& url,
                                    web_app::InstallResultCode code) {
    DCHECK(!base::ContainsKey(next_finalize_install_results_, url));

    web_app::AppId app_id;
    if (code == web_app::InstallResultCode::kSuccess) {
      app_id = GetAppIdForUrl(url);
    }
    next_finalize_install_results_[url] = {app_id, code};
  }

  void SetNextCreateOsShortcutsResult(const web_app::AppId& app_id,
                                      bool shortcut_created) {
    DCHECK(!base::ContainsKey(next_create_os_shortcuts_results_, app_id));
    next_create_os_shortcuts_results_[app_id] = shortcut_created;
  }

  const std::vector<WebApplicationInfo>& web_app_info_list() {
    return web_app_info_list_;
  }

  const std::vector<FinalizeOptions>& finalize_options_list() {
    return finalize_options_list_;
  }

  size_t num_create_os_shortcuts_calls() {
    return num_create_os_shortcuts_calls_;
  }

  // InstallFinalizer
  void FinalizeInstall(const WebApplicationInfo& web_app_info,
                       const FinalizeOptions& options,
                       InstallFinalizedCallback callback) override {
    DCHECK(base::ContainsKey(next_finalize_install_results_,
                             web_app_info.app_url));

    web_app_info_list_.push_back(web_app_info);
    finalize_options_list_.push_back(options);

    web_app::AppId app_id;
    web_app::InstallResultCode code;
    std::tie(app_id, code) =
        next_finalize_install_results_[web_app_info.app_url];
    next_finalize_install_results_.erase(web_app_info.app_url);

    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(
            base::BindLambdaForTesting(
                [&, app_id, code](InstallFinalizedCallback callback) {
                  registrar_->AddAsInstalled(app_id);
                  std::move(callback).Run(app_id, code);
                }),
            std::move(callback)));
  }

  bool CanCreateOsShortcuts() const override { return true; }

  void CreateOsShortcuts(const web_app::AppId& app_id,
                         bool add_to_desktop,
                         CreateOsShortcutsCallback callback) override {
    DCHECK(base::ContainsKey(next_create_os_shortcuts_results_, app_id));
    ++num_create_os_shortcuts_calls_;
    bool shortcut_created = next_create_os_shortcuts_results_[app_id];
    next_create_os_shortcuts_results_.erase(app_id);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), shortcut_created));
  }

  bool CanPinAppToShelf() const override { return true; }

  void PinAppToShelf(const web_app::AppId& app_id) override {
    ++num_pin_app_to_shelf_calls_;
  }

  bool CanReparentTab(const web_app::AppId& app_id,
                      bool shortcut_created) const override {
    NOTIMPLEMENTED();
    return true;
  }

  void ReparentTab(const web_app::AppId& app_id,
                   content::WebContents* web_contents) override {
    NOTIMPLEMENTED();
  }

  bool CanRevealAppShim() const override {
    NOTIMPLEMENTED();
    return true;
  }

  void RevealAppShim(const web_app::AppId& app_id) override {
    NOTIMPLEMENTED();
  }

  bool CanSkipAppUpdateForSync(
      const web_app::AppId& app_id,
      const WebApplicationInfo& web_app_info) const override {
    NOTIMPLEMENTED();
    return true;
  }

 private:
  web_app::TestAppRegistrar* registrar_ = nullptr;

  std::vector<WebApplicationInfo> web_app_info_list_;
  std::vector<FinalizeOptions> finalize_options_list_;

  size_t num_create_os_shortcuts_calls_ = 0;
  size_t num_pin_app_to_shelf_calls_ = 0;

  std::map<GURL, std::pair<web_app::AppId, web_app::InstallResultCode>>
      next_finalize_install_results_;
  std::map<web_app::AppId, bool> next_create_os_shortcuts_results_;

  DISALLOW_COPY_AND_ASSIGN(TestBookmarkAppInstallFinalizer);
};

class BookmarkAppInstallationTaskTest : public ChromeRenderViewHostTestHarness {
 public:
  BookmarkAppInstallationTaskTest() {}

  ~BookmarkAppInstallationTaskTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    registrar_ =
        std::make_unique<web_app::TestAppRegistrar>(/*profile=*/nullptr);
    install_finalizer_ =
        std::make_unique<TestBookmarkAppInstallFinalizer>(registrar_.get());

    // CrxInstaller in BookmarkAppInstaller needs an ExtensionService, so
    // create one for the profile.
    TestExtensionSystem* test_system =
        static_cast<TestExtensionSystem*>(ExtensionSystem::Get(profile()));
    test_system->CreateExtensionService(base::CommandLine::ForCurrentProcess(),
                                        profile()->GetPath(),
                                        false /* autoupdate_enabled */);
    SetUpTestingFactories();
  }

 protected:
  TestBookmarkAppHelper& test_helper() {
    DCHECK(test_helper_);
    return *test_helper_;
  }

  web_app::TestAppRegistrar* registrar() { return registrar_.get(); }

  TestBookmarkAppInstallFinalizer* install_finalizer() {
    return install_finalizer_.get();
  }

 private:
  void SetUpTestingFactories() {
    auto* provider = web_app::WebAppProviderBase::GetProviderBase(profile());
    BookmarkAppInstallManager* install_manager =
        static_cast<BookmarkAppInstallManager*>(&provider->install_manager());

    install_manager->SetBookmarkAppHelperFactoryForTesting(
        base::BindLambdaForTesting(
            [this](Profile* profile, const WebApplicationInfo& web_app_info,
                   content::WebContents* web_contents,
                   WebappInstallSource install_source) {
              auto test_helper = std::make_unique<TestBookmarkAppHelper>(
                  profile, web_app_info, web_contents, install_source);
              test_helper_ = test_helper.get();

              return std::unique_ptr<BookmarkAppHelper>(std::move(test_helper));
            }));

    install_manager->SetDataRetrieverFactoryForTesting(
        base::BindLambdaForTesting([]() {
          auto info = std::make_unique<WebApplicationInfo>();
          info->app_url = kWebAppUrl;
          info->title = base::UTF8ToUTF16(kWebAppTitle);
          auto data_retriever = std::make_unique<web_app::TestDataRetriever>();
          data_retriever->SetRendererWebApplicationInfo(std::move(info));

          return std::unique_ptr<web_app::WebAppDataRetriever>(
              std::move(data_retriever));
        }));
  }

  std::unique_ptr<web_app::TestAppRegistrar> registrar_;
  std::unique_ptr<TestBookmarkAppInstallFinalizer> install_finalizer_;
  TestBookmarkAppHelper* test_helper_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(BookmarkAppInstallationTaskTest);
};

TEST_F(BookmarkAppInstallationTaskTest,
       WebAppOrShortcutFromContents_InstallationSucceeds) {
  auto task = std::make_unique<BookmarkAppInstallationTask>(
      profile(), registrar(), install_finalizer(),
      web_app::InstallOptions(kWebAppUrl, web_app::LaunchContainer::kDefault,
                              web_app::InstallSource::kInternal));

  bool callback_called = false;
  task->Install(
      web_contents(),
      base::BindLambdaForTesting(
          [&](BookmarkAppInstallationTask::Result result) {
            base::Optional<web_app::AppId> id =
                web_app::ExternallyInstalledWebAppPrefs(profile()->GetPrefs())
                    .LookupAppId(kWebAppUrl);

            EXPECT_EQ(web_app::InstallResultCode::kSuccess, result.code);
            EXPECT_TRUE(result.app_id.has_value());

            EXPECT_FALSE(IsPlaceholderApp(profile(), kWebAppUrl));

            EXPECT_EQ(result.app_id.value(), id.value());

            EXPECT_TRUE(test_helper().add_to_quick_launch_bar());
            EXPECT_TRUE(test_helper().add_to_desktop());
            EXPECT_TRUE(test_helper().add_to_quick_launch_bar());
            EXPECT_FALSE(test_helper().forced_launch_type().has_value());
            EXPECT_TRUE(test_helper().is_default_app());
            EXPECT_FALSE(test_helper().is_policy_installed_app());
            callback_called = true;
          }));

  content::RunAllTasksUntilIdle();

  test_helper().CompleteInstallableCheck();
  content::RunAllTasksUntilIdle();

  test_helper().CompleteIconDownload();
  content::RunAllTasksUntilIdle();

  EXPECT_TRUE(callback_called);
}

TEST_F(BookmarkAppInstallationTaskTest,
       WebAppOrShortcutFromContents_InstallationFails) {
  auto task = std::make_unique<BookmarkAppInstallationTask>(
      profile(), registrar(), install_finalizer(),
      web_app::InstallOptions(kWebAppUrl, web_app::LaunchContainer::kWindow,
                              web_app::InstallSource::kInternal));

  bool callback_called = false;
  task->Install(
      web_contents(),
      base::BindLambdaForTesting(
          [&](BookmarkAppInstallationTask::Result result) {
            base::Optional<web_app::AppId> id =
                web_app::ExternallyInstalledWebAppPrefs(profile()->GetPrefs())
                    .LookupAppId(kWebAppUrl);

            EXPECT_NE(web_app::InstallResultCode::kSuccess, result.code);
            EXPECT_FALSE(result.app_id.has_value());

            EXPECT_FALSE(id.has_value());
            callback_called = true;
          }));

  content::RunAllTasksUntilIdle();

  test_helper().CompleteInstallableCheck();
  content::RunAllTasksUntilIdle();

  test_helper().FailIconDownload();
  content::RunAllTasksUntilIdle();

  EXPECT_TRUE(callback_called);
}

TEST_F(BookmarkAppInstallationTaskTest,
       WebAppOrShortcutFromContents_NoDesktopShortcut) {
  web_app::InstallOptions install_options(kWebAppUrl,
                                          web_app::LaunchContainer::kWindow,
                                          web_app::InstallSource::kInternal);
  install_options.add_to_desktop = false;
  auto task = std::make_unique<BookmarkAppInstallationTask>(
      profile(), registrar(), install_finalizer(), std::move(install_options));

  bool callback_called = false;
  task->Install(web_contents(),
                base::BindLambdaForTesting(
                    [&](BookmarkAppInstallationTask::Result result) {
                      EXPECT_EQ(web_app::InstallResultCode::kSuccess,
                                result.code);
                      EXPECT_TRUE(result.app_id.has_value());

                      EXPECT_TRUE(test_helper().add_to_applications_menu());
                      EXPECT_TRUE(test_helper().add_to_quick_launch_bar());
                      EXPECT_FALSE(test_helper().add_to_desktop());

                      callback_called = true;
                    }));
  content::RunAllTasksUntilIdle();

  test_helper().CompleteInstallation();
  EXPECT_TRUE(callback_called);
}

TEST_F(BookmarkAppInstallationTaskTest,
       WebAppOrShortcutFromContents_NoQuickLaunchBarShortcut) {
  web_app::InstallOptions install_options(kWebAppUrl,
                                          web_app::LaunchContainer::kWindow,
                                          web_app::InstallSource::kInternal);
  install_options.add_to_quick_launch_bar = false;
  auto task = std::make_unique<BookmarkAppInstallationTask>(
      profile(), registrar(), install_finalizer(), std::move(install_options));

  bool callback_called = false;
  task->Install(web_contents(),
                base::BindLambdaForTesting(
                    [&](BookmarkAppInstallationTask::Result result) {
                      EXPECT_EQ(web_app::InstallResultCode::kSuccess,
                                result.code);
                      EXPECT_TRUE(result.app_id.has_value());

                      EXPECT_TRUE(test_helper().add_to_applications_menu());
                      EXPECT_FALSE(test_helper().add_to_quick_launch_bar());
                      EXPECT_TRUE(test_helper().add_to_desktop());

                      callback_called = true;
                    }));

  content::RunAllTasksUntilIdle();

  test_helper().CompleteInstallation();
  EXPECT_TRUE(callback_called);
}

TEST_F(
    BookmarkAppInstallationTaskTest,
    WebAppOrShortcutFromContents_NoDesktopShortcutAndNoQuickLaunchBarShortcut) {
  web_app::InstallOptions install_options(kWebAppUrl,
                                          web_app::LaunchContainer::kWindow,
                                          web_app::InstallSource::kInternal);
  install_options.add_to_desktop = false;
  install_options.add_to_quick_launch_bar = false;
  auto task = std::make_unique<BookmarkAppInstallationTask>(
      profile(), registrar(), install_finalizer(), std::move(install_options));

  bool callback_called = false;
  task->Install(web_contents(),
                base::BindLambdaForTesting(
                    [&](BookmarkAppInstallationTask::Result result) {
                      EXPECT_EQ(web_app::InstallResultCode::kSuccess,
                                result.code);
                      EXPECT_TRUE(result.app_id.has_value());

                      EXPECT_TRUE(test_helper().add_to_applications_menu());
                      EXPECT_FALSE(test_helper().add_to_quick_launch_bar());
                      EXPECT_FALSE(test_helper().add_to_desktop());

                      callback_called = true;
                    }));

  content::RunAllTasksUntilIdle();

  test_helper().CompleteInstallation();
  EXPECT_TRUE(callback_called);
}

TEST_F(BookmarkAppInstallationTaskTest,
       WebAppOrShortcutFromContents_ForcedContainerWindow) {
  auto install_options =
      web_app::InstallOptions(kWebAppUrl, web_app::LaunchContainer::kWindow,
                              web_app::InstallSource::kInternal);
  auto task = std::make_unique<BookmarkAppInstallationTask>(
      profile(), registrar(), install_finalizer(), std::move(install_options));

  bool callback_called = false;
  task->Install(web_contents(),
                base::BindLambdaForTesting(
                    [&](BookmarkAppInstallationTask::Result result) {
                      EXPECT_EQ(web_app::InstallResultCode::kSuccess,
                                result.code);
                      EXPECT_TRUE(result.app_id.has_value());

                      EXPECT_EQ(LAUNCH_TYPE_WINDOW,
                                test_helper().forced_launch_type().value());
                      callback_called = true;
                    }));

  content::RunAllTasksUntilIdle();

  test_helper().CompleteInstallation();
  EXPECT_TRUE(callback_called);
}

TEST_F(BookmarkAppInstallationTaskTest,
       WebAppOrShortcutFromContents_ForcedContainerTab) {
  auto install_options =
      web_app::InstallOptions(kWebAppUrl, web_app::LaunchContainer::kTab,
                              web_app::InstallSource::kInternal);
  auto task = std::make_unique<BookmarkAppInstallationTask>(
      profile(), registrar(), install_finalizer(), std::move(install_options));

  bool callback_called = false;
  task->Install(web_contents(),
                base::BindLambdaForTesting(
                    [&](BookmarkAppInstallationTask::Result result) {
                      EXPECT_EQ(web_app::InstallResultCode::kSuccess,
                                result.code);
                      EXPECT_TRUE(result.app_id.has_value());

                      EXPECT_EQ(LAUNCH_TYPE_REGULAR,
                                test_helper().forced_launch_type().value());
                      callback_called = true;
                    }));

  content::RunAllTasksUntilIdle();

  test_helper().CompleteInstallation();
  EXPECT_TRUE(callback_called);
}

TEST_F(BookmarkAppInstallationTaskTest,
       WebAppOrShortcutFromContents_DefaultApp) {
  auto install_options =
      web_app::InstallOptions(kWebAppUrl, web_app::LaunchContainer::kDefault,
                              web_app::InstallSource::kInternal);
  auto task = std::make_unique<BookmarkAppInstallationTask>(
      profile(), registrar(), install_finalizer(), std::move(install_options));

  bool callback_called = false;
  task->Install(web_contents(),
                base::BindLambdaForTesting(
                    [&](BookmarkAppInstallationTask::Result result) {
                      EXPECT_EQ(web_app::InstallResultCode::kSuccess,
                                result.code);
                      EXPECT_TRUE(result.app_id.has_value());

                      EXPECT_TRUE(test_helper().is_default_app());
                      callback_called = true;
                    }));

  content::RunAllTasksUntilIdle();

  test_helper().CompleteInstallation();
  EXPECT_TRUE(callback_called);
}

TEST_F(BookmarkAppInstallationTaskTest,
       WebAppOrShortcutFromContents_AppFromPolicy) {
  auto install_options =
      web_app::InstallOptions(kWebAppUrl, web_app::LaunchContainer::kDefault,
                              web_app::InstallSource::kExternalPolicy);
  auto task = std::make_unique<BookmarkAppInstallationTask>(
      profile(), registrar(), install_finalizer(), std::move(install_options));

  bool callback_called = false;
  task->Install(web_contents(),
                base::BindLambdaForTesting(
                    [&](BookmarkAppInstallationTask::Result result) {
                      EXPECT_EQ(web_app::InstallResultCode::kSuccess,
                                result.code);
                      EXPECT_TRUE(result.app_id.has_value());

                      EXPECT_TRUE(test_helper().is_policy_installed_app());
                      callback_called = true;
                    }));

  content::RunAllTasksUntilIdle();

  test_helper().CompleteInstallation();
  EXPECT_TRUE(callback_called);
}

TEST_F(BookmarkAppInstallationTaskTest, InstallPlaceholder) {
  web_app::InstallOptions options(kWebAppUrl, web_app::LaunchContainer::kWindow,
                                  web_app::InstallSource::kExternalPolicy);
  auto task = std::make_unique<BookmarkAppInstallationTask>(
      profile(), registrar(), install_finalizer(), std::move(options));
  install_finalizer()->SetNextFinalizeInstallResult(
      kWebAppUrl, web_app::InstallResultCode::kSuccess);
  install_finalizer()->SetNextCreateOsShortcutsResult(
      install_finalizer()->GetAppIdForUrl(kWebAppUrl), true);

  base::RunLoop run_loop;
  task->InstallPlaceholder(base::BindLambdaForTesting(
      [&](BookmarkAppInstallationTask::Result result) {
        EXPECT_EQ(web_app::InstallResultCode::kSuccess, result.code);
        EXPECT_TRUE(result.app_id.has_value());

        EXPECT_TRUE(IsPlaceholderApp(profile(), kWebAppUrl));

        EXPECT_EQ(1u, install_finalizer()->num_create_os_shortcuts_calls());
        EXPECT_EQ(1u, install_finalizer()->finalize_options_list().size());
        EXPECT_TRUE(
            install_finalizer()->finalize_options_list()[0].policy_installed);
        const WebApplicationInfo& web_app_info =
            install_finalizer()->web_app_info_list().at(0);

        EXPECT_EQ(base::UTF8ToUTF16(kWebAppUrl.spec()), web_app_info.title);
        EXPECT_EQ(kWebAppUrl, web_app_info.app_url);
        EXPECT_TRUE(web_app_info.open_as_window);
        EXPECT_TRUE(web_app_info.icons.empty());

        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(BookmarkAppInstallationTaskTest, InstallPlaceholderTwice) {
  web_app::InstallOptions options(kWebAppUrl, web_app::LaunchContainer::kWindow,
                                  web_app::InstallSource::kExternalPolicy);
  std::string placeholder_app_id;

  // Install a placeholder app.
  {
    auto task = std::make_unique<BookmarkAppInstallationTask>(
        profile(), registrar(), install_finalizer(), options);
    install_finalizer()->SetNextFinalizeInstallResult(
        kWebAppUrl, web_app::InstallResultCode::kSuccess);
    install_finalizer()->SetNextCreateOsShortcutsResult(
        install_finalizer()->GetAppIdForUrl(kWebAppUrl), true);

    base::RunLoop run_loop;
    task->InstallPlaceholder(base::BindLambdaForTesting(
        [&](BookmarkAppInstallationTask::Result result) {
          EXPECT_EQ(web_app::InstallResultCode::kSuccess, result.code);
          placeholder_app_id = result.app_id.value();

          EXPECT_EQ(1u, install_finalizer()->finalize_options_list().size());
          run_loop.Quit();
        }));
    run_loop.Run();
  }

  // Try to install it again.
  auto task = std::make_unique<BookmarkAppInstallationTask>(
      profile(), registrar(), install_finalizer(), options);
  base::RunLoop run_loop;
  task->InstallPlaceholder(base::BindLambdaForTesting(
      [&](BookmarkAppInstallationTask::Result result) {
        EXPECT_EQ(web_app::InstallResultCode::kSuccess, result.code);
        EXPECT_EQ(placeholder_app_id, result.app_id.value());

        // There shouldn't be a second call to the finalizer.
        EXPECT_EQ(1u, install_finalizer()->finalize_options_list().size());

        run_loop.Quit();
      }));
  run_loop.Run();
}

}  // namespace extensions
