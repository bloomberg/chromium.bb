// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/demo_mode/demo_setup_controller.h"
#include "base/test/scoped_task_environment.h"
#include "chrome/browser/chromeos/login/enrollment/enterprise_enrollment_helper_mock.h"
#include "chrome/browser/chromeos/policy/enrollment_status_chromeos.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chromeos/cryptohome/system_salt_getter.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

class MockDemoSetupControllerDelegate : public DemoSetupController::Delegate {
 public:
  MockDemoSetupControllerDelegate() = default;
  ~MockDemoSetupControllerDelegate() override = default;

  MOCK_METHOD0(OnSetupError, void());
  MOCK_METHOD0(OnSetupSuccess, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDemoSetupControllerDelegate);
};

}  // namespace

class DemoSetupControllerTest : public testing::Test {
 protected:
  enum class SetupResult { SUCCESS, ERROR };

  template <SetupResult result>
  static EnterpriseEnrollmentHelper* MockOnlineEnrollmentHelperCreator(
      EnterpriseEnrollmentHelper::EnrollmentStatusConsumer* status_consumer,
      const policy::EnrollmentConfig& enrollment_config,
      const std::string& enrolling_user_domain) {
    EnterpriseEnrollmentHelperMock* mock =
        new EnterpriseEnrollmentHelperMock(status_consumer);

    EXPECT_CALL(*mock, EnrollUsingAttestation())
        .WillRepeatedly(testing::Invoke([mock]() {
          if (result == SetupResult::SUCCESS) {
            mock->status_consumer()->OnDeviceEnrolled("");
          } else {
            // TODO(agawronska): Test different error types.
            mock->status_consumer()->OnEnrollmentError(
                policy::EnrollmentStatus::ForStatus(
                    policy::EnrollmentStatus::REGISTRATION_FAILED));
          }
        }));
    return mock;
  }

  template <SetupResult result>
  static EnterpriseEnrollmentHelper* MockOfflineEnrollmentHelperCreator(
      EnterpriseEnrollmentHelper::EnrollmentStatusConsumer* status_consumer,
      const policy::EnrollmentConfig& enrollment_config,
      const std::string& enrolling_user_domain) {
    EnterpriseEnrollmentHelperMock* mock =
        new EnterpriseEnrollmentHelperMock(status_consumer);

    EXPECT_CALL(*mock, EnrollForOfflineDemo())
        .WillRepeatedly(testing::Invoke([mock]() {
          if (result == SetupResult::SUCCESS) {
            mock->status_consumer()->OnDeviceEnrolled("");
          } else {
            // TODO(agawronska): Test different error types.
            mock->status_consumer()->OnEnrollmentError(
                policy::EnrollmentStatus::ForStatus(
                    policy::EnrollmentStatus::Status::LOCK_ERROR));
          }
        }));
    return mock;
  }

  DemoSetupControllerTest()
      : delegate_(std::make_unique<MockDemoSetupControllerDelegate>()),
        tested_controller_(
            std::make_unique<DemoSetupController>(delegate_.get())) {}

  ~DemoSetupControllerTest() override = default;

  void SetUp() override {
    SystemSaltGetter::Initialize();
    DBusThreadManager::Initialize();
    DeviceSettingsService::Initialize();
  }

  void TearDown() override {
    DBusThreadManager::Shutdown();
    SystemSaltGetter::Shutdown();
    DeviceSettingsService::Shutdown();
  }

  std::unique_ptr<MockDemoSetupControllerDelegate> delegate_;
  std::unique_ptr<DemoSetupController> tested_controller_;

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;

  DISALLOW_COPY_AND_ASSIGN(DemoSetupControllerTest);
};

TEST_F(DemoSetupControllerTest, OfflineSuccess) {
  EnterpriseEnrollmentHelper::SetupEnrollmentHelperMock(
      &MockOfflineEnrollmentHelperCreator<SetupResult::SUCCESS>);

  EXPECT_CALL(*delegate_, OnSetupSuccess()).Times(1);

  tested_controller_->EnrollOffline();
}

TEST_F(DemoSetupControllerTest, OfflineError) {
  EnterpriseEnrollmentHelper::SetupEnrollmentHelperMock(
      &MockOfflineEnrollmentHelperCreator<SetupResult::ERROR>);

  EXPECT_CALL(*delegate_, OnSetupError()).Times(1);

  tested_controller_->EnrollOffline();
}

TEST_F(DemoSetupControllerTest, OnlineSuccess) {
  EnterpriseEnrollmentHelper::SetupEnrollmentHelperMock(
      &MockOnlineEnrollmentHelperCreator<SetupResult::SUCCESS>);

  EXPECT_CALL(*delegate_, OnSetupSuccess()).Times(1);

  tested_controller_->EnrollOnline();
}

TEST_F(DemoSetupControllerTest, OnlineError) {
  EnterpriseEnrollmentHelper::SetupEnrollmentHelperMock(
      &MockOnlineEnrollmentHelperCreator<SetupResult::ERROR>);

  EXPECT_CALL(*delegate_, OnSetupError()).Times(1);

  tested_controller_->EnrollOnline();
}

}  //  namespace chromeos
