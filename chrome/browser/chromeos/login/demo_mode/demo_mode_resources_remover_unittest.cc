// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/demo_mode/demo_mode_resources_remover.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_session.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/settings/stub_install_attributes.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_cryptohome_client.h"
#include "components/prefs/testing_pref_service.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_names.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

// Used as a callback to DemoModeResourcesRemover::AttemptRemoval - it records
// the result of the attempt to |result_out|.
void RecordRemovalResult(
    base::Optional<DemoModeResourcesRemover::RemovalResult>* result_out,
    DemoModeResourcesRemover::RemovalResult result) {
  *result_out = result;
}

}  // namespace

class DemoModeResourcesRemoverTest : public testing::Test {
 public:
  DemoModeResourcesRemoverTest() = default;
  ~DemoModeResourcesRemoverTest() override = default;

  void SetUp() override {
    install_attributes_ = std::make_unique<ScopedStubInstallAttributes>(
        CreateInstallAttributes());

    auto cryptohome_client = std::make_unique<FakeCryptohomeClient>();
    cryptohome_client_ = cryptohome_client.get();

    chromeos::DBusThreadManager::GetSetterForTesting()->SetCryptohomeClient(
        std::move(cryptohome_client));

    ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
    demo_resources_path_ =
        scoped_temp_dir_.GetPath().AppendASCII("demo-mode-resources");
    DemoSession::OverridePreInstalledDemoResourcesPathForTesting(
        &demo_resources_path_);

    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::make_unique<FakeChromeUserManager>());

    DemoSession::SetDemoModeEnrollmentTypeForTesting(
        DemoSession::EnrollmentType::kUnenrolled);

    DemoModeResourcesRemover::RegisterLocalStatePrefs(local_state_.registry());
  }

  void TearDown() override {
    DemoSession::ShutDownIfInitialized();
    DemoSession::OverridePreInstalledDemoResourcesPathForTesting(nullptr);
    DemoSession::SetDemoModeEnrollmentTypeForTesting(
        DemoSession::EnrollmentType::kNone);
    chromeos::DBusThreadManager::Shutdown();
  }

 protected:
  virtual std::unique_ptr<StubInstallAttributes> CreateInstallAttributes() {
    return StubInstallAttributes::CreateConsumerOwned();
  }

  bool CreateDemoModeResources() {
    if (!base::CreateDirectory(demo_resources_path_))
      return false;

    const std::string manifest = R"({
        "name": "demo-mode-resources",
        "version": "0.0.1",
        "min_env_version": "1.0"
    })";
    if (base::WriteFile(demo_resources_path_.AppendASCII("manifest.json"),
                        manifest.data(),
                        manifest.size()) != static_cast<int>(manifest.size())) {
      return false;
    }

    const std::string image = "fake image content";
    if (base::WriteFile(demo_resources_path_.AppendASCII("image.squash"),
                        image.data(),
                        image.size()) != static_cast<int>(image.size())) {
      return false;
    }

    return true;
  }

  bool DemoModeResourcesExist() {
    return base::DirectoryExists(demo_resources_path_);
  }

  enum class TestUserType {
    kRegular,
    kGuest,
    kPublicAccount,
    kKiosk,
    kDerelictDemoKiosk
  };

  void AddAndLogInUser(TestUserType type, DemoModeResourcesRemover* remover) {
    FakeChromeUserManager* user_manager =
        static_cast<FakeChromeUserManager*>(user_manager::UserManager::Get());
    const user_manager::User* user = nullptr;
    switch (type) {
      case TestUserType::kRegular:
        user =
            user_manager->AddUser(AccountId::FromUserEmail("fake_user@test"));
        break;
      case TestUserType::kGuest:
        user = user_manager->AddGuestUser();
        break;
      case TestUserType::kPublicAccount:
        user = user_manager->AddPublicAccountUser(
            AccountId::FromUserEmail("fake_user@test"));
        break;
      case TestUserType::kKiosk:
        user = user_manager->AddKioskAppUser(
            AccountId::FromUserEmail("fake_user@test"));
        break;
      case TestUserType::kDerelictDemoKiosk:
        user = user_manager->AddKioskAppUser(user_manager::DemoAccountId());
        break;
    }

    ASSERT_TRUE(user);

    user_manager->LoginUser(user->GetAccountId());
    user_manager->SwitchActiveUser(user->GetAccountId());
    remover->ActiveUserChanged(user);
  }

  FakeCryptohomeClient* cryptohome_client_ = nullptr;

  TestingPrefServiceSimple local_state_;
  content::TestBrowserThreadBundle thread_bundle_;

 private:
  std::unique_ptr<ScopedStubInstallAttributes> install_attributes_;

  base::ScopedTempDir scoped_temp_dir_;
  base::FilePath demo_resources_path_;

  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;

  DISALLOW_COPY_AND_ASSIGN(DemoModeResourcesRemoverTest);
};

class ManagedDemoModeResourcesRemoverTest
    : public DemoModeResourcesRemoverTest {
 public:
  ManagedDemoModeResourcesRemoverTest() = default;
  ~ManagedDemoModeResourcesRemoverTest() override = default;

  std::unique_ptr<StubInstallAttributes> CreateInstallAttributes() override {
    return StubInstallAttributes::CreateCloudManaged("test-domain",
                                                     "FAKE_DEVICE_ID");
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ManagedDemoModeResourcesRemoverTest);
};

class DemoModeResourcesRemoverInLegacyDemoRetailModeTest
    : public DemoModeResourcesRemoverTest {
 public:
  DemoModeResourcesRemoverInLegacyDemoRetailModeTest() = default;
  ~DemoModeResourcesRemoverInLegacyDemoRetailModeTest() override = default;

  std::unique_ptr<StubInstallAttributes> CreateInstallAttributes() override {
    return StubInstallAttributes::CreateCloudManaged("us-retailmode.com",
                                                     "FAKE_DEVICE_ID");
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DemoModeResourcesRemoverInLegacyDemoRetailModeTest);
};

TEST(LegacyDemoRetailModeDomainMatching, Matching) {
  EXPECT_TRUE(DemoModeResourcesRemover::IsLegacyDemoRetailModeDomain(
      "us-retailmode.com"));
  EXPECT_TRUE(DemoModeResourcesRemover::IsLegacyDemoRetailModeDomain(
      "us2-retailmode.com"));
  EXPECT_TRUE(DemoModeResourcesRemover::IsLegacyDemoRetailModeDomain(
      "hr-retailmode.com"));
  EXPECT_TRUE(DemoModeResourcesRemover::IsLegacyDemoRetailModeDomain(
      "uk-retailmode.com"));
  EXPECT_FALSE(DemoModeResourcesRemover::IsLegacyDemoRetailModeDomain(
      "u1-retailmode.com"));
  EXPECT_FALSE(DemoModeResourcesRemover::IsLegacyDemoRetailModeDomain(
      "uss-retailmode.com"));
  EXPECT_FALSE(DemoModeResourcesRemover::IsLegacyDemoRetailModeDomain(
      "us4-retailmode.com"));
  EXPECT_FALSE(
      DemoModeResourcesRemover::IsLegacyDemoRetailModeDomain("us-retailmode"));
  EXPECT_FALSE(DemoModeResourcesRemover::IsLegacyDemoRetailModeDomain(
      "us-retailmode.com.foo"));
  EXPECT_FALSE(DemoModeResourcesRemover::IsLegacyDemoRetailModeDomain(""));
  EXPECT_FALSE(DemoModeResourcesRemover::IsLegacyDemoRetailModeDomain(
      "fake-domain.com"));
  EXPECT_FALSE(DemoModeResourcesRemover::IsLegacyDemoRetailModeDomain(
      "us-us-retailmode.com"));
  EXPECT_FALSE(
      DemoModeResourcesRemover::IsLegacyDemoRetailModeDomain("us.com"));
}

TEST_F(DemoModeResourcesRemoverTest, LowDiskSpace) {
  ASSERT_TRUE(CreateDemoModeResources());

  std::unique_ptr<DemoModeResourcesRemover> remover =
      DemoModeResourcesRemover::CreateIfNeeded(&local_state_);
  ASSERT_TRUE(remover.get());
  EXPECT_EQ(DemoModeResourcesRemover::Get(), remover.get());

  cryptohome_client_->NotifyLowDiskSpace(1024 * 1024 * 1024);
  thread_bundle_.RunUntilIdle();
  EXPECT_FALSE(DemoModeResourcesExist());
}

TEST_F(DemoModeResourcesRemoverTest, LowDiskSpaceInDemoSession) {
  ASSERT_TRUE(CreateDemoModeResources());
  DemoSession::SetDemoModeEnrollmentTypeForTesting(
      DemoSession::EnrollmentType::kOnline);

  std::unique_ptr<DemoModeResourcesRemover> remover =
      DemoModeResourcesRemover::CreateIfNeeded(&local_state_);
  EXPECT_FALSE(remover.get());
  EXPECT_FALSE(DemoModeResourcesRemover::Get());

  cryptohome_client_->NotifyLowDiskSpace(1024 * 1024 * 1024);
  thread_bundle_.RunUntilIdle();
  EXPECT_TRUE(DemoModeResourcesExist());
}

TEST_F(DemoModeResourcesRemoverTest, NotCreatedAfterResourcesRemoved) {
  ASSERT_TRUE(CreateDemoModeResources());

  std::unique_ptr<DemoModeResourcesRemover> remover =
      DemoModeResourcesRemover::CreateIfNeeded(&local_state_);
  ASSERT_TRUE(remover.get());
  EXPECT_EQ(DemoModeResourcesRemover::Get(), remover.get());

  cryptohome_client_->NotifyLowDiskSpace(1024 * 1024 * 1024);
  thread_bundle_.RunUntilIdle();
  EXPECT_FALSE(DemoModeResourcesExist());

  // Reset the resources remover - subsequent attempts to create the remover
  // instance should return nullptr.
  remover.reset();
  EXPECT_FALSE(DemoModeResourcesRemover::CreateIfNeeded(&local_state_));
}

TEST_F(DemoModeResourcesRemoverTest, AttemptRemoval) {
  ASSERT_TRUE(CreateDemoModeResources());
  std::unique_ptr<DemoModeResourcesRemover> remover =
      DemoModeResourcesRemover::CreateIfNeeded(&local_state_);
  ASSERT_TRUE(remover.get());
  EXPECT_EQ(DemoModeResourcesRemover::Get(), remover.get());

  base::Optional<DemoModeResourcesRemover::RemovalResult> result;
  remover->AttemptRemoval(
      DemoModeResourcesRemover::RemovalReason::kEnterpriseEnrolled,
      base::BindOnce(&RecordRemovalResult, &result));

  thread_bundle_.RunUntilIdle();

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(DemoModeResourcesRemover::RemovalResult::kSuccess, result.value());
  EXPECT_FALSE(DemoModeResourcesExist());
}

TEST_F(DemoModeResourcesRemoverTest, AttemptRemovalResourcesNonExistent) {
  std::unique_ptr<DemoModeResourcesRemover> remover =
      DemoModeResourcesRemover::CreateIfNeeded(&local_state_);
  ASSERT_TRUE(remover.get());
  EXPECT_EQ(DemoModeResourcesRemover::Get(), remover.get());

  base::Optional<DemoModeResourcesRemover::RemovalResult> result;
  remover->AttemptRemoval(
      DemoModeResourcesRemover::RemovalReason::kLowDiskSpace,
      base::BindOnce(&RecordRemovalResult, &result));

  thread_bundle_.RunUntilIdle();

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(DemoModeResourcesRemover::RemovalResult::kNotFound, result.value());
}

TEST_F(DemoModeResourcesRemoverTest, AttemptRemovalInDemoSession) {
  ASSERT_TRUE(CreateDemoModeResources());
  std::unique_ptr<DemoModeResourcesRemover> remover =
      DemoModeResourcesRemover::CreateIfNeeded(&local_state_);
  DemoSession::SetDemoModeEnrollmentTypeForTesting(
      DemoSession::EnrollmentType::kOnline);

  base::Optional<DemoModeResourcesRemover::RemovalResult> result;
  remover->AttemptRemoval(
      DemoModeResourcesRemover::RemovalReason::kLowDiskSpace,
      base::BindOnce(&RecordRemovalResult, &result));

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(DemoModeResourcesRemover::RemovalResult::kNotAllowed,
            result.value());
  EXPECT_TRUE(DemoModeResourcesExist());
}

TEST_F(DemoModeResourcesRemoverTest, ConcurrentRemovalAttempts) {
  ASSERT_TRUE(CreateDemoModeResources());
  std::unique_ptr<DemoModeResourcesRemover> remover =
      DemoModeResourcesRemover::CreateIfNeeded(&local_state_);
  ASSERT_TRUE(remover.get());
  EXPECT_EQ(DemoModeResourcesRemover::Get(), remover.get());

  base::Optional<DemoModeResourcesRemover::RemovalResult> result_1;
  remover->AttemptRemoval(
      DemoModeResourcesRemover::RemovalReason::kLowDiskSpace,
      base::BindOnce(&RecordRemovalResult, &result_1));

  base::Optional<DemoModeResourcesRemover::RemovalResult> result_2;
  remover->AttemptRemoval(
      DemoModeResourcesRemover::RemovalReason::kLowDiskSpace,
      base::BindOnce(&RecordRemovalResult, &result_2));

  thread_bundle_.RunUntilIdle();

  EXPECT_FALSE(DemoModeResourcesExist());
  ASSERT_TRUE(result_1.has_value());
  EXPECT_EQ(DemoModeResourcesRemover::RemovalResult::kSuccess,
            result_1.value());

  ASSERT_TRUE(result_2.has_value());
  EXPECT_EQ(DemoModeResourcesRemover::RemovalResult::kSuccess,
            result_2.value());
}

TEST_F(DemoModeResourcesRemoverTest, RepeatedRemovalAttempt) {
  ASSERT_TRUE(CreateDemoModeResources());
  std::unique_ptr<DemoModeResourcesRemover> remover =
      DemoModeResourcesRemover::CreateIfNeeded(&local_state_);
  ASSERT_TRUE(remover.get());
  remover->AttemptRemoval(
      DemoModeResourcesRemover::RemovalReason::kLowDiskSpace,
      DemoModeResourcesRemover::RemovalCallback());
  thread_bundle_.RunUntilIdle();

  EXPECT_FALSE(DemoModeResourcesExist());

  base::Optional<DemoModeResourcesRemover::RemovalResult> result;
  remover->AttemptRemoval(
      DemoModeResourcesRemover::RemovalReason::kLowDiskSpace,
      base::BindOnce(&RecordRemovalResult, &result));
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(DemoModeResourcesRemover::RemovalResult::kAlreadyRemoved,
            result.value());
}

TEST_F(DemoModeResourcesRemoverTest, NoRemovalOnLogin) {
  ASSERT_TRUE(CreateDemoModeResources());
  std::unique_ptr<DemoModeResourcesRemover> remover =
      DemoModeResourcesRemover::CreateIfNeeded(&local_state_);
  ASSERT_TRUE(remover.get());

  AddAndLogInUser(TestUserType::kRegular, remover.get());

  thread_bundle_.RunUntilIdle();

  EXPECT_TRUE(DemoModeResourcesExist());
}

// Tests the kiosk app incarnation of demo mode.
TEST_F(DemoModeResourcesRemoverTest, NoRemovalInKioskDemoMode) {
  ASSERT_TRUE(CreateDemoModeResources());
  std::unique_ptr<DemoModeResourcesRemover> remover =
      DemoModeResourcesRemover::CreateIfNeeded(&local_state_);
  ASSERT_TRUE(remover.get());

  AddAndLogInUser(TestUserType::kDerelictDemoKiosk, remover.get());
  thread_bundle_.RunUntilIdle();

  EXPECT_TRUE(DemoModeResourcesExist());
}

TEST_F(ManagedDemoModeResourcesRemoverTest, RemoveOnRegularLogin) {
  ASSERT_TRUE(CreateDemoModeResources());
  std::unique_ptr<DemoModeResourcesRemover> remover =
      DemoModeResourcesRemover::CreateIfNeeded(&local_state_);
  ASSERT_TRUE(remover.get());

  AddAndLogInUser(TestUserType::kRegular, remover.get());

  thread_bundle_.RunUntilIdle();

  EXPECT_FALSE(DemoModeResourcesExist());
}

TEST_F(ManagedDemoModeResourcesRemoverTest, NoRemovalGuestLogin) {
  ASSERT_TRUE(CreateDemoModeResources());
  std::unique_ptr<DemoModeResourcesRemover> remover =
      DemoModeResourcesRemover::CreateIfNeeded(&local_state_);
  ASSERT_TRUE(remover.get());

  AddAndLogInUser(TestUserType::kGuest, remover.get());

  thread_bundle_.RunUntilIdle();

  EXPECT_TRUE(DemoModeResourcesExist());
}

TEST_F(ManagedDemoModeResourcesRemoverTest, RemoveOnLowDiskInGuest) {
  ASSERT_TRUE(CreateDemoModeResources());
  std::unique_ptr<DemoModeResourcesRemover> remover =
      DemoModeResourcesRemover::CreateIfNeeded(&local_state_);
  ASSERT_TRUE(remover.get());

  AddAndLogInUser(TestUserType::kGuest, remover.get());
  cryptohome_client_->NotifyLowDiskSpace(1024 * 1024 * 1024);
  thread_bundle_.RunUntilIdle();

  EXPECT_FALSE(DemoModeResourcesExist());
}

TEST_F(ManagedDemoModeResourcesRemoverTest, RemoveOnPublicSessionLogin) {
  ASSERT_TRUE(CreateDemoModeResources());
  std::unique_ptr<DemoModeResourcesRemover> remover =
      DemoModeResourcesRemover::CreateIfNeeded(&local_state_);
  ASSERT_TRUE(remover.get());

  AddAndLogInUser(TestUserType::kPublicAccount, remover.get());
  thread_bundle_.RunUntilIdle();

  EXPECT_FALSE(DemoModeResourcesExist());
}

TEST_F(ManagedDemoModeResourcesRemoverTest, RemoveInKioskSession) {
  ASSERT_TRUE(CreateDemoModeResources());
  std::unique_ptr<DemoModeResourcesRemover> remover =
      DemoModeResourcesRemover::CreateIfNeeded(&local_state_);
  ASSERT_TRUE(remover.get());

  AddAndLogInUser(TestUserType::kKiosk, remover.get());
  thread_bundle_.RunUntilIdle();

  EXPECT_FALSE(DemoModeResourcesExist());
}

TEST_F(DemoModeResourcesRemoverInLegacyDemoRetailModeTest, NoRemovalOnLogin) {
  ASSERT_TRUE(CreateDemoModeResources());
  std::unique_ptr<DemoModeResourcesRemover> remover =
      DemoModeResourcesRemover::CreateIfNeeded(&local_state_);
  ASSERT_TRUE(remover.get());

  AddAndLogInUser(TestUserType::kPublicAccount, remover.get());
  thread_bundle_.RunUntilIdle();

  EXPECT_TRUE(DemoModeResourcesExist());
}

TEST_F(DemoModeResourcesRemoverInLegacyDemoRetailModeTest,
       RemoveOnLowDiskSpace) {
  ASSERT_TRUE(CreateDemoModeResources());
  std::unique_ptr<DemoModeResourcesRemover> remover =
      DemoModeResourcesRemover::CreateIfNeeded(&local_state_);
  ASSERT_TRUE(remover.get());

  AddAndLogInUser(TestUserType::kPublicAccount, remover.get());
  cryptohome_client_->NotifyLowDiskSpace(1024 * 1024 * 1024);
  thread_bundle_.RunUntilIdle();

  EXPECT_FALSE(DemoModeResourcesExist());
}

}  // namespace chromeos
