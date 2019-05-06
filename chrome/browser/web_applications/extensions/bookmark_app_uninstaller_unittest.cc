// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/extensions/bookmark_app_uninstaller.h"

#include <memory>
#include <string>
#include <vector>

#include "base/scoped_observer.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_registrar.h"
#include "chrome/browser/web_applications/test/test_app_registrar.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/crx_file/id_util.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/extension_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

const GURL kFooWebAppUrl("https://foo.example");
const GURL kBarWebAppUrl("https://bar.example");

class TestExtensionRegistryObserver : public ExtensionRegistryObserver {
 public:
  explicit TestExtensionRegistryObserver(ExtensionRegistry* registry) {
    extension_registry_observer_.Add(registry);
  }

  ~TestExtensionRegistryObserver() override = default;

  const std::vector<std::string>& uninstalled_extension_ids() {
    return uninstalled_extension_ids_;
  }

  void ResetResults() { uninstalled_extension_ids_.clear(); }

  // ExtensionRegistryObserver
  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const Extension* extension,
                              UninstallReason reason) override {
    uninstalled_extension_ids_.push_back(extension->id());
  }

 private:
  std::vector<std::string> uninstalled_extension_ids_;

  ScopedObserver<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observer_{this};

  DISALLOW_COPY_AND_ASSIGN(TestExtensionRegistryObserver);
};

std::string GenerateFakeAppId(const GURL& url) {
  return crx_file::id_util::GenerateId("fake_app_id_for:" + url.spec());
}

}  // namespace

class BookmarkAppUninstallerTest : public ChromeRenderViewHostTestHarness {
 public:
  BookmarkAppUninstallerTest() = default;
  ~BookmarkAppUninstallerTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    TestExtensionSystem* test_system =
        static_cast<TestExtensionSystem*>(ExtensionSystem::Get(profile()));
    test_system->CreateExtensionService(base::CommandLine::ForCurrentProcess(),
                                        profile()->GetPath(),
                                        false /* autoupdate_enabled */);
    test_extension_registry_observer_ =
        std::make_unique<TestExtensionRegistryObserver>(
            ExtensionRegistry::Get(profile()));

    registrar_ = std::make_unique<BookmarkAppRegistrar>(profile());

    uninstaller_ =
        std::make_unique<BookmarkAppUninstaller>(profile(), registrar_.get());
  }

  void TearDown() override {
    // Delete the observer before ExtensionRegistry is deleted.
    test_extension_registry_observer_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  std::string SimulateInstalledApp(const GURL& app_url) {
    std::string app_id = GenerateFakeAppId(app_url);

    auto extension = ExtensionBuilder("FooBar")
                         .SetLocation(Manifest::EXTERNAL_POLICY)
                         .SetID(app_id)
                         .Build();
    ExtensionRegistry::Get(profile())->AddEnabled(extension);
    web_app::ExternallyInstalledWebAppPrefs(profile()->GetPrefs())
        .Insert(app_url, app_id, web_app::InstallSource::kExternalPolicy);
    return app_id;
  }

  void SimulateExternalAppUninstalledByUser(const GURL& app_url) {
    const std::string app_id = GenerateFakeAppId(app_url);
    ExtensionRegistry::Get(profile())->RemoveEnabled(app_id);
    ExtensionPrefs::Get(profile())->OnExtensionUninstalled(
        app_id, Manifest::EXTERNAL_POLICY, false /* external_uninstall */);
  }

  void ResetResults() { test_extension_registry_observer_->ResetResults(); }

  BookmarkAppUninstaller& uninstaller() { return *uninstaller_; }

  const std::vector<std::string>& uninstalled_extension_ids() {
    return test_extension_registry_observer_->uninstalled_extension_ids();
  }

  const ExtensionSet& enabled_extensions() {
    return ExtensionRegistry::Get(profile())->enabled_extensions();
  }

 private:
  std::unique_ptr<TestExtensionRegistryObserver>
      test_extension_registry_observer_;

  std::unique_ptr<BookmarkAppRegistrar> registrar_;
  std::unique_ptr<BookmarkAppUninstaller> uninstaller_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkAppUninstallerTest);
};

TEST_F(BookmarkAppUninstallerTest, Uninstall_Successful) {
  SimulateInstalledApp(kFooWebAppUrl);
  ASSERT_EQ(1u, enabled_extensions().size());

  EXPECT_TRUE(uninstaller().UninstallApp(kFooWebAppUrl));
  content::RunAllTasksUntilIdle();

  EXPECT_EQ(1u, uninstalled_extension_ids().size());
  EXPECT_EQ(0u, enabled_extensions().size());
}

TEST_F(BookmarkAppUninstallerTest, Uninstall_Multiple) {
  auto foo_app_id = SimulateInstalledApp(kFooWebAppUrl);
  auto bar_app_id = SimulateInstalledApp(kBarWebAppUrl);
  ASSERT_EQ(2u, enabled_extensions().size());

  EXPECT_TRUE(uninstaller().UninstallApp(kBarWebAppUrl));
  content::RunAllTasksUntilIdle();

  EXPECT_EQ(1u, uninstalled_extension_ids().size());
  EXPECT_EQ(1u, enabled_extensions().size());
  EXPECT_FALSE(enabled_extensions().Contains(bar_app_id));
  EXPECT_TRUE(enabled_extensions().Contains(foo_app_id));

  ResetResults();

  EXPECT_TRUE(uninstaller().UninstallApp(kFooWebAppUrl));
  content::RunAllTasksUntilIdle();

  EXPECT_EQ(1u, uninstalled_extension_ids().size());
  EXPECT_TRUE(enabled_extensions().is_empty());
}

TEST_F(BookmarkAppUninstallerTest, Uninstall_UninstalledExternalApp) {
  SimulateInstalledApp(kFooWebAppUrl);
  SimulateExternalAppUninstalledByUser(kFooWebAppUrl);

  EXPECT_FALSE(uninstaller().UninstallApp(kFooWebAppUrl));
}

// Tests trying to uninstall an app that was never installed.
TEST_F(BookmarkAppUninstallerTest, Uninstall_FailsNeverInstalled) {
  EXPECT_FALSE(uninstaller().UninstallApp(kFooWebAppUrl));
}

// Tests trying to uninstall an app that was previously uninstalled.
TEST_F(BookmarkAppUninstallerTest, Uninstall_FailsAlreadyUninstalled) {
  SimulateInstalledApp(kFooWebAppUrl);

  EXPECT_TRUE(uninstaller().UninstallApp(kFooWebAppUrl));
  content::RunAllTasksUntilIdle();

  EXPECT_EQ(1u, uninstalled_extension_ids().size());
  EXPECT_TRUE(enabled_extensions().is_empty());

  ResetResults();

  EXPECT_FALSE(uninstaller().UninstallApp(kFooWebAppUrl));
  content::RunAllTasksUntilIdle();

  EXPECT_EQ(0u, uninstalled_extension_ids().size());
}

}  // namespace extensions
