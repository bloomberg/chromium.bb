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
#include "base/test/scoped_task_environment.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_session.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_cryptohome_client.h"
#include "components/prefs/testing_pref_service.h"
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
    auto cryptohome_client = std::make_unique<FakeCryptohomeClient>();
    cryptohome_client_ = cryptohome_client.get();

    chromeos::DBusThreadManager::GetSetterForTesting()->SetCryptohomeClient(
        std::move(cryptohome_client));

    ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
    demo_resources_path_ =
        scoped_temp_dir_.GetPath().AppendASCII("demo-mode-resources");
    DemoSession::OverridePreInstalledDemoResourcesPathForTesting(
        &demo_resources_path_);

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
  bool CreateDemoModeResources() {
    if (!base::CreateDirectory(demo_resources_path_))
      return false;

    const std::string manifest = R"({
        "name": "demo-mode-resources",
        "version": "0.0.1",
        "min_env_version": "1.0"
    })";
    if (base::WriteFile(demo_resources_path_.AppendASCII("manifest.json"),
                        manifest.data(), manifest.size()) < 0) {
      return false;
    }

    const std::string image = "fake image content";
    if (base::WriteFile(demo_resources_path_.AppendASCII("image.squash"),
                        image.data(), manifest.size()) < 0) {
      return false;
    }

    return true;
  }

  bool DemoModeResourcesExist() {
    return base::DirectoryExists(demo_resources_path_);
  }

  FakeCryptohomeClient* cryptohome_client_ = nullptr;

  TestingPrefServiceSimple local_state_;
  base::test::ScopedTaskEnvironment task_environment_;

 private:
  base::ScopedTempDir scoped_temp_dir_;
  base::FilePath demo_resources_path_;

  DISALLOW_COPY_AND_ASSIGN(DemoModeResourcesRemoverTest);
};

TEST_F(DemoModeResourcesRemoverTest, LowDiskSpace) {
  ASSERT_TRUE(CreateDemoModeResources());

  std::unique_ptr<DemoModeResourcesRemover> remover =
      DemoModeResourcesRemover::CreateIfNeeded(&local_state_);
  ASSERT_TRUE(remover.get());
  EXPECT_EQ(DemoModeResourcesRemover::Get(), remover.get());

  cryptohome_client_->NotifyLowDiskSpace(1024 * 1024 * 1024);
  task_environment_.RunUntilIdle();
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
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(DemoModeResourcesExist());
}

TEST_F(DemoModeResourcesRemoverTest, NotCreatedAfterResourcesRemoved) {
  ASSERT_TRUE(CreateDemoModeResources());

  std::unique_ptr<DemoModeResourcesRemover> remover =
      DemoModeResourcesRemover::CreateIfNeeded(&local_state_);
  ASSERT_TRUE(remover.get());
  EXPECT_EQ(DemoModeResourcesRemover::Get(), remover.get());

  cryptohome_client_->NotifyLowDiskSpace(1024 * 1024 * 1024);
  task_environment_.RunUntilIdle();
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

  task_environment_.RunUntilIdle();

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

  task_environment_.RunUntilIdle();

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

  task_environment_.RunUntilIdle();

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
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(DemoModeResourcesExist());

  base::Optional<DemoModeResourcesRemover::RemovalResult> result;
  remover->AttemptRemoval(
      DemoModeResourcesRemover::RemovalReason::kLowDiskSpace,
      base::BindOnce(&RecordRemovalResult, &result));
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(DemoModeResourcesRemover::RemovalResult::kAlreadyRemoved,
            result.value());
}

}  // namespace chromeos
