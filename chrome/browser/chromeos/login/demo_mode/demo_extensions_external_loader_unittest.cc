// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/demo_mode/demo_extensions_external_loader.h"

#include <map>
#include <memory>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_writer.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_session.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/extensions/external_provider_impl.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_image_loader_client.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/browser/external_install_info.h"
#include "extensions/browser/external_provider_interface.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

// Information about found external extension file: {version, crx_path}.
using TestCrxInfo = std::tuple<std::string, std::string>;

class TestExternalProviderVisitor
    : public extensions::ExternalProviderInterface::VisitorInterface {
 public:
  TestExternalProviderVisitor() = default;
  ~TestExternalProviderVisitor() override = default;

  const std::map<std::string, TestCrxInfo>& loaded_crx_files() const {
    return loaded_crx_files_;
  }

  void WaitForReady() {
    if (ready_)
      return;
    ready_waiter_ = std::make_unique<base::RunLoop>();
    ready_waiter_->Run();
    ready_waiter_.reset();
  }

  // extensions::ExternalProviderInterface::VisitorInterface:
  bool OnExternalExtensionFileFound(
      const extensions::ExternalInstallInfoFile& info) override {
    EXPECT_EQ(0u, loaded_crx_files_.count(info.extension_id));
    EXPECT_EQ(extensions::Manifest::INTERNAL, info.crx_location)
        << info.extension_id;

    loaded_crx_files_.emplace(
        info.extension_id,
        TestCrxInfo(info.version.GetString(), info.path.value()));
    return true;
  }

  bool OnExternalExtensionUpdateUrlFound(
      const extensions::ExternalInstallInfoUpdateUrl& info,
      bool is_initial_load) override {
    ADD_FAILURE() << "Found extensions with update URL";
    return false;
  }

  void OnExternalProviderReady(
      const extensions::ExternalProviderInterface* provider) override {
    ready_ = true;
    if (ready_waiter_)
      ready_waiter_->Quit();
  }

  void OnExternalProviderUpdateComplete(
      const extensions::ExternalProviderInterface* provider,
      const std::vector<extensions::ExternalInstallInfoUpdateUrl>&
          update_url_extensions,
      const std::vector<extensions::ExternalInstallInfoFile>& file_extensions,
      const std::set<std::string>& removed_extensions) override {
    ADD_FAILURE() << "Found updated extensions.";
  }

 private:
  bool ready_ = false;
  std::map<std::string, TestCrxInfo> loaded_crx_files_;

  std::unique_ptr<base::RunLoop> ready_waiter_;

  DISALLOW_COPY_AND_ASSIGN(TestExternalProviderVisitor);
};

}  // namespace

class DemoExtensionsExternalLoaderTest : public testing::Test {
 public:
  DemoExtensionsExternalLoaderTest() = default;
  ~DemoExtensionsExternalLoaderTest() override = default;

  void SetUp() override {
    DemoSession::SetDemoModeEnrollmentTypeForTesting(
        DemoSession::EnrollmentType::kOnline);

    ASSERT_TRUE(offline_demo_resources_.CreateUniqueTempDir());

    auto image_loader_client = std::make_unique<FakeImageLoaderClient>();
    image_loader_client_ = image_loader_client.get();
    DBusThreadManager::GetSetterForTesting()->SetImageLoaderClient(
        std::move(image_loader_client));
  }

  void TearDown() override {
    profile_.reset();

    image_loader_client_ = nullptr;
    DBusThreadManager::Shutdown();

    DemoSession::ShutDownIfInitialized();
    DemoSession::SetDemoModeEnrollmentTypeForTesting(
        DemoSession::EnrollmentType::kNone);
  }

 protected:
  void InitializeSession(bool mount_demo_resources,
                         bool wait_for_offline_resources_load) {
    if (mount_demo_resources) {
      image_loader_client_->SetMountPathForComponent(
          "demo-mode-resources", offline_demo_resources_.GetPath());
    }
    ASSERT_TRUE(DemoSession::StartIfInDemoMode());

    if (wait_for_offline_resources_load)
      WaitForOfflineResourcesLoad();

    profile_ = std::make_unique<TestingProfile>();
  }

  void WaitForOfflineResourcesLoad() {
    base::RunLoop run_loop;
    DemoSession::Get()->EnsureOfflineResourcesLoaded(run_loop.QuitClosure());
    run_loop.Run();
  }

  std::string GetTestResourcePath(const std::string& rel_path) {
    return offline_demo_resources_.GetPath().Append(rel_path).value();
  }

  bool SetExtensionsConfig(const base::Value& config) {
    std::string config_str;
    if (!base::JSONWriter::Write(config, &config_str))
      return false;

    base::FilePath config_path =
        offline_demo_resources_.GetPath().Append("demo_extensions.json");
    int written =
        base::WriteFile(config_path, config_str.data(), config_str.size());
    return written == static_cast<int>(config_str.size());
  }

  void AddExtensionToConfig(const std::string& id,
                            const base::Optional<std::string>& version,
                            const base::Optional<std::string>& path,
                            base::Value* config) {
    ASSERT_TRUE(config->is_dict());

    base::Value extension(base::Value::Type::DICTIONARY);
    if (version.has_value()) {
      extension.SetKey(extensions::ExternalProviderImpl::kExternalVersion,
                       base::Value(version.value()));
    }
    if (path.has_value()) {
      extension.SetKey(extensions::ExternalProviderImpl::kExternalCrx,
                       base::Value(path.value()));
    }
    config->SetKey(id, std::move(extension));
  }

  std::unique_ptr<extensions::ExternalProviderImpl> CreateExternalProvider(
      extensions::ExternalProviderInterface::VisitorInterface* visitor) {
    return std::make_unique<extensions::ExternalProviderImpl>(
        visitor, base::MakeRefCounted<DemoExtensionsExternalLoader>(),
        profile_.get(), extensions::Manifest::INTERNAL,
        extensions::Manifest::INTERNAL,
        extensions::Extension::FROM_WEBSTORE |
            extensions::Extension::WAS_INSTALLED_BY_DEFAULT);
  }

 protected:
  TestExternalProviderVisitor external_provider_visitor_;

 private:
  content::TestBrowserThreadBundle thread_bundle_;

  std::unique_ptr<TestingProfile> profile_;

  // Image loader client injected into, and owned by DBusThreadManager.
  FakeImageLoaderClient* image_loader_client_ = nullptr;

  base::ScopedTempDir offline_demo_resources_;

  DISALLOW_COPY_AND_ASSIGN(DemoExtensionsExternalLoaderTest);
};

TEST_F(DemoExtensionsExternalLoaderTest, NoDemoExtensionsConfig) {
  InitializeSession(true /*mount_demo_resources*/,
                    true /*wait_for_offline_resources_load*/);

  std::unique_ptr<extensions::ExternalProviderImpl> external_provider =
      CreateExternalProvider(&external_provider_visitor_);

  external_provider->VisitRegisteredExtension();
  external_provider_visitor_.WaitForReady();

  EXPECT_TRUE(external_provider->IsReady());
  EXPECT_TRUE(external_provider_visitor_.loaded_crx_files().empty());
}

TEST_F(DemoExtensionsExternalLoaderTest, InvalidDemoExtensionsConfig) {
  InitializeSession(true /*mount_demo_resources*/,
                    true /*wait_for_offline_resources_load*/);

  ASSERT_TRUE(SetExtensionsConfig(base::Value("invalid_config")));

  std::unique_ptr<extensions::ExternalProviderImpl> external_provider =
      CreateExternalProvider(&external_provider_visitor_);

  external_provider->VisitRegisteredExtension();
  external_provider_visitor_.WaitForReady();

  EXPECT_TRUE(external_provider->IsReady());
  EXPECT_TRUE(external_provider_visitor_.loaded_crx_files().empty());
}

TEST_F(DemoExtensionsExternalLoaderTest, SingleDemoExtension) {
  InitializeSession(true /*mount_demo_resources*/,
                    true /*wait_for_offline_resources_load*/);

  base::Value config = base::Value(base::Value::Type::DICTIONARY);
  AddExtensionToConfig(std::string(32, 'a'), base::make_optional("1.0.0"),
                       base::make_optional("extensions/a.crx"), &config);
  ASSERT_TRUE(SetExtensionsConfig(std::move(config)));

  std::unique_ptr<extensions::ExternalProviderImpl> external_provider =
      CreateExternalProvider(&external_provider_visitor_);

  external_provider->VisitRegisteredExtension();
  external_provider_visitor_.WaitForReady();

  EXPECT_TRUE(external_provider->IsReady());
  std::map<std::string, TestCrxInfo> expected_info = {
      {std::string(32, 'a'),
       TestCrxInfo("1.0.0", GetTestResourcePath("extensions/a.crx"))}};
  EXPECT_EQ(expected_info, external_provider_visitor_.loaded_crx_files());
}

TEST_F(DemoExtensionsExternalLoaderTest, MultipleDemoExtension) {
  InitializeSession(true /*mount_demo_resources*/,
                    true /*wait_for_offline_resources_load*/);

  base::Value config = base::Value(base::Value::Type::DICTIONARY);
  AddExtensionToConfig(std::string(32, 'a'), base::make_optional("1.0.0"),
                       base::make_optional("extensions/a.crx"), &config);
  AddExtensionToConfig(std::string(32, 'b'), base::make_optional("1.1.0"),
                       base::make_optional("b.crx"), &config);
  AddExtensionToConfig(std::string(32, 'c'), base::make_optional("2.0.0"),
                       base::make_optional("c.crx"), &config);
  ASSERT_TRUE(SetExtensionsConfig(std::move(config)));

  std::unique_ptr<extensions::ExternalProviderImpl> external_provider =
      CreateExternalProvider(&external_provider_visitor_);

  external_provider->VisitRegisteredExtension();
  external_provider_visitor_.WaitForReady();

  EXPECT_TRUE(external_provider->IsReady());

  std::map<std::string, TestCrxInfo> expected_info = {
      {std::string(32, 'a'),
       TestCrxInfo("1.0.0", GetTestResourcePath("extensions/a.crx"))},
      {std::string(32, 'b'),
       TestCrxInfo("1.1.0", GetTestResourcePath("b.crx"))},
      {std::string(32, 'c'),
       TestCrxInfo("2.0.0", GetTestResourcePath("c.crx"))},
  };
  EXPECT_EQ(expected_info, external_provider_visitor_.loaded_crx_files());
}

TEST_F(DemoExtensionsExternalLoaderTest, CrxPathWithAbsolutePath) {
  InitializeSession(true /*mount_demo_resources*/,
                    true /*wait_for_offline_resources_load*/);

  base::Value config = base::Value(base::Value::Type::DICTIONARY);
  AddExtensionToConfig(std::string(32, 'a'), base::make_optional("1.0.0"),
                       base::make_optional("a.crx"), &config);
  AddExtensionToConfig(std::string(32, 'b'), base::make_optional("1.1.0"),
                       base::make_optional(GetTestResourcePath("b.crx")),
                       &config);
  ASSERT_TRUE(SetExtensionsConfig(std::move(config)));

  std::unique_ptr<extensions::ExternalProviderImpl> external_provider =
      CreateExternalProvider(&external_provider_visitor_);

  external_provider->VisitRegisteredExtension();
  external_provider_visitor_.WaitForReady();

  EXPECT_TRUE(external_provider->IsReady());

  std::map<std::string, TestCrxInfo> expected_info = {
      {std::string(32, 'a'),
       TestCrxInfo("1.0.0", GetTestResourcePath("a.crx"))},
  };
  EXPECT_EQ(expected_info, external_provider_visitor_.loaded_crx_files());
}

TEST_F(DemoExtensionsExternalLoaderTest, ExtensionWithPathMissing) {
  InitializeSession(true /*mount_demo_resources*/,
                    true /*wait_for_offline_resources_load*/);

  base::Value config = base::Value(base::Value::Type::DICTIONARY);
  AddExtensionToConfig(std::string(32, 'a'), base::make_optional("1.0.0"),
                       base::make_optional("a.crx"), &config);
  AddExtensionToConfig(std::string(32, 'b'), base::make_optional("1.1.0"),
                       base::nullopt, &config);
  ASSERT_TRUE(SetExtensionsConfig(std::move(config)));

  std::unique_ptr<extensions::ExternalProviderImpl> external_provider =
      CreateExternalProvider(&external_provider_visitor_);

  external_provider->VisitRegisteredExtension();
  external_provider_visitor_.WaitForReady();

  EXPECT_TRUE(external_provider->IsReady());

  std::map<std::string, TestCrxInfo> expected_info = {
      {std::string(32, 'a'),
       TestCrxInfo("1.0.0", GetTestResourcePath("a.crx"))},
  };
  EXPECT_EQ(expected_info, external_provider_visitor_.loaded_crx_files());
}

TEST_F(DemoExtensionsExternalLoaderTest, ExtensionWithVersionMissing) {
  InitializeSession(true /*mount_demo_resources*/,
                    true /*wait_for_offline_resources_load*/);

  base::Value config = base::Value(base::Value::Type::DICTIONARY);
  AddExtensionToConfig(std::string(32, 'a'), base::make_optional("1.0.0"),
                       base::make_optional("a.crx"), &config);
  AddExtensionToConfig(std::string(32, 'b'), base::nullopt,
                       base::make_optional("b.crx"), &config);
  ASSERT_TRUE(SetExtensionsConfig(std::move(config)));

  std::unique_ptr<extensions::ExternalProviderImpl> external_provider =
      CreateExternalProvider(&external_provider_visitor_);
  external_provider->VisitRegisteredExtension();
  external_provider_visitor_.WaitForReady();

  EXPECT_TRUE(external_provider->IsReady());

  std::map<std::string, TestCrxInfo> expected_info = {
      {std::string(32, 'a'),
       TestCrxInfo("1.0.0", GetTestResourcePath("a.crx"))},
  };
  EXPECT_EQ(expected_info, external_provider_visitor_.loaded_crx_files());
}

TEST_F(DemoExtensionsExternalLoaderTest, DemoResourcesNotLoaded) {
  InitializeSession(false /*mount_demo_resources*/,
                    true /*wait_for_offline_resources_load*/);

  std::unique_ptr<extensions::ExternalProviderImpl> external_provider =
      CreateExternalProvider(&external_provider_visitor_);
  external_provider->VisitRegisteredExtension();
  external_provider_visitor_.WaitForReady();

  EXPECT_TRUE(external_provider->IsReady());
  EXPECT_TRUE(external_provider_visitor_.loaded_crx_files().empty());
}

TEST_F(DemoExtensionsExternalLoaderTest,
       StartLoaderBeforeOfflineResourcesLoaded) {
  InitializeSession(true /*mount_demo_resources*/,
                    false /*wait_for_offline_resources_load*/);

  base::Value config = base::Value(base::Value::Type::DICTIONARY);
  AddExtensionToConfig(std::string(32, 'a'), base::make_optional("1.0.0"),
                       base::make_optional("a.crx"), &config);
  ASSERT_TRUE(SetExtensionsConfig(std::move(config)));

  std::unique_ptr<extensions::ExternalProviderImpl> external_provider =
      CreateExternalProvider(&external_provider_visitor_);
  external_provider->VisitRegisteredExtension();

  WaitForOfflineResourcesLoad();

  external_provider_visitor_.WaitForReady();
  EXPECT_TRUE(external_provider->IsReady());

  std::map<std::string, TestCrxInfo> expected_info = {
      {std::string(32, 'a'),
       TestCrxInfo("1.0.0", GetTestResourcePath("a.crx"))},
  };
}

TEST_F(DemoExtensionsExternalLoaderTest,
       StartLoaderBeforeOfflineResourcesLoadFails) {
  InitializeSession(false /*mount_demo_resources*/,
                    false /*wait_for_offline_resources_load*/);

  base::Value config = base::Value(base::Value::Type::DICTIONARY);
  AddExtensionToConfig(std::string(32, 'a'), base::make_optional("1.0.0"),
                       base::make_optional("a.crx"), &config);
  ASSERT_TRUE(SetExtensionsConfig(std::move(config)));

  std::unique_ptr<extensions::ExternalProviderImpl> external_provider =
      CreateExternalProvider(&external_provider_visitor_);
  external_provider->VisitRegisteredExtension();

  WaitForOfflineResourcesLoad();

  external_provider_visitor_.WaitForReady();
  EXPECT_TRUE(external_provider->IsReady());
  EXPECT_TRUE(external_provider_visitor_.loaded_crx_files().empty());
}

class ShouldCreateDemoExtensionsExternalLoaderTest : public testing::Test {
 public:
  ShouldCreateDemoExtensionsExternalLoaderTest() {
    auto fake_user_manager = std::make_unique<FakeChromeUserManager>();
    user_manager_ = fake_user_manager.get();
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(fake_user_manager));
  }

  ~ShouldCreateDemoExtensionsExternalLoaderTest() override = default;

  void SetUp() override { DBusThreadManager::Initialize(); }

  void TearDown() override {
    DBusThreadManager::Shutdown();
    DemoSession::ShutDownIfInitialized();
    DemoSession::SetDemoModeEnrollmentTypeForTesting(
        DemoSession::EnrollmentType::kNone);
  }

 protected:
  std::unique_ptr<TestingProfile> AddTestUser(const AccountId& account_id) {
    auto profile = std::make_unique<TestingProfile>();
    profile->set_profile_name(account_id.GetUserEmail());
    user_manager_->AddUser(account_id);
    user_manager_->LoginUser(account_id);
    return profile;
  }

  void StartDemoSession(DemoSession::EnrollmentType demo_enrollment) {
    ASSERT_NE(DemoSession::EnrollmentType::kNone, demo_enrollment);

    DemoSession::SetDemoModeEnrollmentTypeForTesting(demo_enrollment);
    DemoSession::StartIfInDemoMode();

    base::RunLoop run_loop;
    DemoSession::Get()->EnsureOfflineResourcesLoaded(run_loop.QuitClosure());
    run_loop.Run();
  }

  // Owned by scoped_user_manager_.
  FakeChromeUserManager* user_manager_ = nullptr;

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;

  DISALLOW_COPY_AND_ASSIGN(ShouldCreateDemoExtensionsExternalLoaderTest);
};

TEST_F(ShouldCreateDemoExtensionsExternalLoaderTest, PrimaryDemoProfile) {
  StartDemoSession(DemoSession::EnrollmentType::kOnline);

  std::unique_ptr<TestingProfile> profile = AddTestUser(
      AccountId::FromUserEmailGaiaId("primary@test.com", "primary_user"));

  EXPECT_TRUE(DemoExtensionsExternalLoader::SupportedForProfile(profile.get()));
}

TEST_F(ShouldCreateDemoExtensionsExternalLoaderTest,
       PrimaryOfflineEnrolledDemoProfile) {
  StartDemoSession(DemoSession::EnrollmentType::kOffline);

  std::unique_ptr<TestingProfile> profile = AddTestUser(
      AccountId::FromUserEmailGaiaId("primary@test.com", "primary_user"));

  EXPECT_TRUE(DemoExtensionsExternalLoader::SupportedForProfile(profile.get()));
}

TEST_F(ShouldCreateDemoExtensionsExternalLoaderTest, ProfileWithNoUser) {
  StartDemoSession(DemoSession::EnrollmentType::kOnline);
  TestingProfile profile;

  EXPECT_FALSE(DemoExtensionsExternalLoader::SupportedForProfile(&profile));
}

TEST_F(ShouldCreateDemoExtensionsExternalLoaderTest, MultiProfile) {
  StartDemoSession(DemoSession::EnrollmentType::kOnline);

  std::unique_ptr<TestingProfile> primary_profile = AddTestUser(
      AccountId::FromUserEmailGaiaId("primary@test.com", "primary_user"));

  std::unique_ptr<TestingProfile> secondary_profile = AddTestUser(
      AccountId::FromUserEmailGaiaId("secondary@test.com", "secondary_user"));

  EXPECT_TRUE(
      DemoExtensionsExternalLoader::SupportedForProfile(primary_profile.get()));
  EXPECT_FALSE(DemoExtensionsExternalLoader::SupportedForProfile(
      secondary_profile.get()));
}

TEST_F(ShouldCreateDemoExtensionsExternalLoaderTest, NotDemoMode) {
  DemoSession::SetDemoModeEnrollmentTypeForTesting(
      DemoSession::EnrollmentType::kUnenrolled);

  // This should be no-op, given that the default demo session enrollment state
  // is not-enrolled.
  DemoSession::StartIfInDemoMode();
  ASSERT_FALSE(DemoSession::Get());

  std::unique_ptr<TestingProfile> profile = AddTestUser(
      AccountId::FromUserEmailGaiaId("primary@test.com", "primary_user"));

  EXPECT_FALSE(
      DemoExtensionsExternalLoader::SupportedForProfile(profile.get()));
}

TEST_F(ShouldCreateDemoExtensionsExternalLoaderTest, DemoSessionNotStarted) {
  DemoSession::SetDemoModeEnrollmentTypeForTesting(
      DemoSession::EnrollmentType::kOnline);

  std::unique_ptr<TestingProfile> profile = AddTestUser(
      AccountId::FromUserEmailGaiaId("primary@test.com", "primary_user"));

  EXPECT_FALSE(
      DemoExtensionsExternalLoader::SupportedForProfile(profile.get()));
}

}  // namespace chromeos
