// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/demo_mode/demo_setup_controller.h"

#include <memory>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_setup_test_utils.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chromeos/cryptohome/system_salt_getter.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_store.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using chromeos::test::DemoModeSetupResult;
using chromeos::test::MockDemoModeOfflineEnrollmentHelperCreator;
using chromeos::test::MockDemoModeOnlineEnrollmentHelperCreator;
using chromeos::test::SetupDummyOfflinePolicyDir;
using testing::_;

namespace chromeos {

namespace {

class DemoSetupControllerTestHelper {
 public:
  DemoSetupControllerTestHelper()
      : run_loop_(std::make_unique<base::RunLoop>()) {}
  virtual ~DemoSetupControllerTestHelper() = default;

  void OnSetupError(DemoSetupController::DemoSetupError error) {
    EXPECT_FALSE(succeeded_.has_value());
    succeeded_ = false;
    error_ = error;
    run_loop_->Quit();
  }

  void OnSetupSuccess() {
    EXPECT_FALSE(succeeded_.has_value());
    succeeded_ = true;
    run_loop_->Quit();
  }

  // Wait until the setup result arrives (either OnSetupError or OnSetupSuccess
  // is called), returns true when the result matches with |expected|.
  bool WaitResult(bool expected) {
    // Run() stops immediately if Quit is already called.
    run_loop_->Run();
    return succeeded_.has_value() && succeeded_.value() == expected;
  }

  // Returns true if it receives a fatal error.
  bool IsErrorFatal() const {
    return error_ == DemoSetupController::DemoSetupError::kFatal;
  }

  void Reset() {
    succeeded_.reset();
    run_loop_ = std::make_unique<base::RunLoop>();
  }

 private:
  base::Optional<bool> succeeded_;
  DemoSetupController::DemoSetupError error_ =
      DemoSetupController::DemoSetupError::kRecoverable;
  std::unique_ptr<base::RunLoop> run_loop_;

  DISALLOW_COPY_AND_ASSIGN(DemoSetupControllerTestHelper);
};

}  // namespace

class DemoSetupControllerTest : public testing::Test {
 protected:
  DemoSetupControllerTest() = default;
  ~DemoSetupControllerTest() override = default;

  void SetUp() override {
    SystemSaltGetter::Initialize();
    DBusThreadManager::Initialize();
    DeviceSettingsService::Initialize();
    helper_ = std::make_unique<DemoSetupControllerTestHelper>();
    tested_controller_ = std::make_unique<DemoSetupController>();
  }

  void TearDown() override {
    DBusThreadManager::Shutdown();
    SystemSaltGetter::Shutdown();
    DeviceSettingsService::Shutdown();
  }

  std::unique_ptr<DemoSetupControllerTestHelper> helper_;
  std::unique_ptr<DemoSetupController> tested_controller_;

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;

  DISALLOW_COPY_AND_ASSIGN(DemoSetupControllerTest);
};

TEST_F(DemoSetupControllerTest, OfflineSuccess) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(SetupDummyOfflinePolicyDir("test", &temp_dir));

  EnterpriseEnrollmentHelper::SetupEnrollmentHelperMock(
      &MockDemoModeOfflineEnrollmentHelperCreator<
          DemoModeSetupResult::SUCCESS>);
  policy::MockCloudPolicyStore mock_store;
  EXPECT_CALL(mock_store, Store(_))
      .WillOnce(testing::InvokeWithoutArgs(
          &mock_store, &policy::MockCloudPolicyStore::NotifyStoreLoaded));
  tested_controller_->SetDeviceLocalAccountPolicyStoreForTest(&mock_store);

  tested_controller_->set_enrollment_type(
      DemoSetupController::EnrollmentType::kOffline);
  tested_controller_->SetOfflineDataDirForTest(temp_dir.GetPath());
  tested_controller_->Enroll(
      base::BindOnce(&DemoSetupControllerTestHelper::OnSetupSuccess,
                     base::Unretained(helper_.get())),
      base::BindOnce(&DemoSetupControllerTestHelper::OnSetupError,
                     base::Unretained(helper_.get())));

  EXPECT_TRUE(helper_->WaitResult(true));
}

TEST_F(DemoSetupControllerTest, OfflineDeviceLocalAccountPolicyLoadFailure) {
  EnterpriseEnrollmentHelper::SetupEnrollmentHelperMock(
      &MockDemoModeOfflineEnrollmentHelperCreator<
          DemoModeSetupResult::SUCCESS>);

  policy::MockCloudPolicyStore mock_store;
  EXPECT_CALL(mock_store, Store(_)).Times(0);
  tested_controller_->SetDeviceLocalAccountPolicyStoreForTest(&mock_store);

  tested_controller_->set_enrollment_type(
      DemoSetupController::EnrollmentType::kOffline);
  tested_controller_->SetOfflineDataDirForTest(
      base::FilePath(FILE_PATH_LITERAL("/no/such/path")));
  tested_controller_->Enroll(
      base::BindOnce(&DemoSetupControllerTestHelper::OnSetupSuccess,
                     base::Unretained(helper_.get())),
      base::BindOnce(&DemoSetupControllerTestHelper::OnSetupError,
                     base::Unretained(helper_.get())));

  EXPECT_TRUE(helper_->WaitResult(false));
  EXPECT_FALSE(helper_->IsErrorFatal());
}

TEST_F(DemoSetupControllerTest, OfflineDeviceLocalAccountPolicyStoreFailed) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(SetupDummyOfflinePolicyDir("test", &temp_dir));

  EnterpriseEnrollmentHelper::SetupEnrollmentHelperMock(
      &MockDemoModeOfflineEnrollmentHelperCreator<
          DemoModeSetupResult::SUCCESS>);
  policy::MockCloudPolicyStore mock_store;
  EXPECT_CALL(mock_store, Store(_))
      .WillOnce(testing::InvokeWithoutArgs(
          &mock_store, &policy::MockCloudPolicyStore::NotifyStoreError));
  tested_controller_->SetDeviceLocalAccountPolicyStoreForTest(&mock_store);

  tested_controller_->set_enrollment_type(
      DemoSetupController::EnrollmentType::kOffline);
  tested_controller_->SetOfflineDataDirForTest(temp_dir.GetPath());
  tested_controller_->Enroll(
      base::BindOnce(&DemoSetupControllerTestHelper::OnSetupSuccess,
                     base::Unretained(helper_.get())),
      base::BindOnce(&DemoSetupControllerTestHelper::OnSetupError,
                     base::Unretained(helper_.get())));

  EXPECT_TRUE(helper_->WaitResult(false));
  EXPECT_TRUE(helper_->IsErrorFatal());
}

TEST_F(DemoSetupControllerTest, OfflineInvalidDeviceLocalAccountPolicyBlob) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(SetupDummyOfflinePolicyDir("", &temp_dir));

  EnterpriseEnrollmentHelper::SetupEnrollmentHelperMock(
      &MockDemoModeOfflineEnrollmentHelperCreator<
          DemoModeSetupResult::SUCCESS>);

  tested_controller_->set_enrollment_type(
      DemoSetupController::EnrollmentType::kOffline);
  tested_controller_->SetOfflineDataDirForTest(temp_dir.GetPath());
  tested_controller_->Enroll(
      base::BindOnce(&DemoSetupControllerTestHelper::OnSetupSuccess,
                     base::Unretained(helper_.get())),
      base::BindOnce(&DemoSetupControllerTestHelper::OnSetupError,
                     base::Unretained(helper_.get())));

  EXPECT_TRUE(helper_->WaitResult(false));
  EXPECT_TRUE(helper_->IsErrorFatal());
}

TEST_F(DemoSetupControllerTest, OfflineError) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(SetupDummyOfflinePolicyDir("test", &temp_dir));

  EnterpriseEnrollmentHelper::SetupEnrollmentHelperMock(
      &MockDemoModeOfflineEnrollmentHelperCreator<DemoModeSetupResult::ERROR>);

  policy::MockCloudPolicyStore mock_store;
  EXPECT_CALL(mock_store, Store(_)).Times(0);
  tested_controller_->SetDeviceLocalAccountPolicyStoreForTest(&mock_store);

  tested_controller_->set_enrollment_type(
      DemoSetupController::EnrollmentType::kOffline);
  tested_controller_->SetOfflineDataDirForTest(temp_dir.GetPath());
  tested_controller_->Enroll(
      base::BindOnce(&DemoSetupControllerTestHelper::OnSetupSuccess,
                     base::Unretained(helper_.get())),
      base::BindOnce(&DemoSetupControllerTestHelper::OnSetupError,
                     base::Unretained(helper_.get())));

  EXPECT_TRUE(helper_->WaitResult(false));
  EXPECT_FALSE(helper_->IsErrorFatal());
}

TEST_F(DemoSetupControllerTest, OnlineSuccess) {
  EnterpriseEnrollmentHelper::SetupEnrollmentHelperMock(
      &MockDemoModeOnlineEnrollmentHelperCreator<DemoModeSetupResult::SUCCESS>);

  tested_controller_->set_enrollment_type(
      DemoSetupController::EnrollmentType::kOnline);
  tested_controller_->Enroll(
      base::BindOnce(&DemoSetupControllerTestHelper::OnSetupSuccess,
                     base::Unretained(helper_.get())),
      base::BindOnce(&DemoSetupControllerTestHelper::OnSetupError,
                     base::Unretained(helper_.get())));

  EXPECT_TRUE(helper_->WaitResult(true));
}

TEST_F(DemoSetupControllerTest, OnlineError) {
  EnterpriseEnrollmentHelper::SetupEnrollmentHelperMock(
      &MockDemoModeOnlineEnrollmentHelperCreator<DemoModeSetupResult::ERROR>);

  tested_controller_->set_enrollment_type(
      DemoSetupController::EnrollmentType::kOnline);
  tested_controller_->Enroll(
      base::BindOnce(&DemoSetupControllerTestHelper::OnSetupSuccess,
                     base::Unretained(helper_.get())),
      base::BindOnce(&DemoSetupControllerTestHelper::OnSetupError,
                     base::Unretained(helper_.get())));

  EXPECT_TRUE(helper_->WaitResult(false));
  EXPECT_FALSE(helper_->IsErrorFatal());
}

TEST_F(DemoSetupControllerTest, EnrollTwice) {
  EnterpriseEnrollmentHelper::SetupEnrollmentHelperMock(
      &MockDemoModeOnlineEnrollmentHelperCreator<DemoModeSetupResult::ERROR>);

  tested_controller_->set_enrollment_type(
      DemoSetupController::EnrollmentType::kOnline);
  tested_controller_->Enroll(
      base::BindOnce(&DemoSetupControllerTestHelper::OnSetupSuccess,
                     base::Unretained(helper_.get())),
      base::BindOnce(&DemoSetupControllerTestHelper::OnSetupError,
                     base::Unretained(helper_.get())));

  EXPECT_TRUE(helper_->WaitResult(false));
  EXPECT_FALSE(helper_->IsErrorFatal());

  helper_->Reset();

  EnterpriseEnrollmentHelper::SetupEnrollmentHelperMock(
      &MockDemoModeOnlineEnrollmentHelperCreator<DemoModeSetupResult::SUCCESS>);

  tested_controller_->set_enrollment_type(
      DemoSetupController::EnrollmentType::kOnline);
  tested_controller_->Enroll(
      base::BindOnce(&DemoSetupControllerTestHelper::OnSetupSuccess,
                     base::Unretained(helper_.get())),
      base::BindOnce(&DemoSetupControllerTestHelper::OnSetupError,
                     base::Unretained(helper_.get())));

  EXPECT_TRUE(helper_->WaitResult(true));
}

}  //  namespace chromeos
