// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/enrollment/enrollment_screen.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/test/scoped_mock_time_message_loop_task_runner.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chromeos/login/enrollment/enterprise_enrollment_helper.h"
#include "chrome/browser/chromeos/login/enrollment/enterprise_enrollment_helper_mock.h"
#include "chrome/browser/chromeos/login/enrollment/mock_enrollment_screen.h"
#include "chrome/browser/chromeos/login/screens/mock_base_screen_delegate.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/policy/enrollment_config.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "components/pairing/fake_controller_pairing_controller.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::AnyNumber;
using testing::Invoke;

namespace chromeos {

class EnrollmentScreenUnitTest : public testing::Test {
 public:
  EnrollmentScreenUnitTest() : fake_controller_("") {}

  // Creates the EnrollmentScreen and sets required parameters.
  virtual void SetUpEnrollmentScreen() {
    enrollment_screen_.reset(
        new EnrollmentScreen(&mock_delegate_, &mock_view_));
    enrollment_screen_->SetParameters(enrollment_config_, &fake_controller_);
  }

  // Fast forwards time by the specified amount.
  void FastForwardTime(base::TimeDelta time) {
    runner_.task_runner()->FastForwardBy(time);
  }

  MockBaseScreenDelegate* GetBaseScreenDelegate() { return &mock_delegate_; }
  MockEnrollmentScreenView* GetMockScreenView() { return &mock_view_; }

  // testing::Test:
  void SetUp() override {
    // Initialize the thread manager.
    DBusThreadManager::Initialize();
  }

  void TearDown() override {
    TestingBrowserProcess::GetGlobal()->SetShuttingDown(true);
    DBusThreadManager::Shutdown();
  }

 protected:
  std::unique_ptr<EnrollmentScreen> enrollment_screen_;

  policy::EnrollmentConfig enrollment_config_;

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  // Replace main thread's task runner with a mock for duration of test.
  base::ScopedMockTimeMessageLoopTaskRunner runner_;

  // Objects required by the EnrollmentScreen that can be re-used.
  pairing_chromeos::FakeControllerPairingController fake_controller_;
  MockBaseScreenDelegate mock_delegate_;
  MockEnrollmentScreenView mock_view_;

  DISALLOW_COPY_AND_ASSIGN(EnrollmentScreenUnitTest);
};

class ZeroTouchEnrollmentScreenUnitTest : public EnrollmentScreenUnitTest {
 public:
  ZeroTouchEnrollmentScreenUnitTest() = default;

  // Closure passed to EnterpriseEnrollmentHelper::SetupEnrollmentHelperMock
  // which creates the EnterpriseEnrollmentHelperMock object that will
  // eventually be tied to the EnrollmentScreen. It also sets up the
  // appropriate expectations for testing with the Google Mock framework.
  // The template parameter should_enroll indicates whether or not
  // the EnterpriseEnrollmentHelper should be mocked to successfully enroll.
  template <bool should_enroll>
  static EnterpriseEnrollmentHelper* MockEnrollmentHelperCreator(
      EnterpriseEnrollmentHelper::EnrollmentStatusConsumer* status_consumer,
      const policy::EnrollmentConfig& enrollment_config,
      const std::string& enrolling_user_domain) {
    EnterpriseEnrollmentHelperMock* mock =
        new EnterpriseEnrollmentHelperMock(status_consumer);
    if (should_enroll) {
      // Define behavior of EnrollUsingAttestation to successfully enroll.
      EXPECT_CALL(*mock, EnrollUsingAttestation())
          .Times(AnyNumber())
          .WillRepeatedly(Invoke([mock]() {
            static_cast<EnrollmentScreen*>(mock->status_consumer())
                ->ShowEnrollmentStatusOnSuccess();
          }));
      // Define behavior of ClearAuth to only run the callback it is given.
      EXPECT_CALL(*mock, ClearAuth(_))
          .Times(AnyNumber())
          .WillRepeatedly(
              Invoke([](const base::Closure& callback) { callback.Run(); }));
    } else {
      // Define behavior of EnrollUsingAttestation to fail to enroll.
      EXPECT_CALL(*mock, EnrollUsingAttestation())
          .Times(AnyNumber())
          .WillRepeatedly(Invoke([mock]() {
            mock->status_consumer()->OnEnrollmentError(
                policy::EnrollmentStatus::ForStatus(
                    policy::EnrollmentStatus::REGISTRATION_FAILED));
          }));
    }
    return mock;
  }

  void SetUpEnrollmentScreen() override {
    enrollment_config_.mode = policy::EnrollmentConfig::MODE_ATTESTATION_FORCED;
    enrollment_config_.auth_mechanism =
        policy::EnrollmentConfig::AUTH_MECHANISM_ATTESTATION;
    EnrollmentScreenUnitTest::SetUpEnrollmentScreen();
  }

  // testing::Test:
  void SetUp() override {
    EnrollmentScreenUnitTest::SetUp();

    // Configure the browser to use Hands-Off Enrollment.
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kEnterpriseEnableZeroTouchEnrollment, "hands-off");
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ZeroTouchEnrollmentScreenUnitTest);
};

TEST_F(ZeroTouchEnrollmentScreenUnitTest, Retries) {
  // Define behavior of EnterpriseEnrollmentHelperMock to always fail
  // enrollment.
  EnterpriseEnrollmentHelper::SetupEnrollmentHelperMock(
      &ZeroTouchEnrollmentScreenUnitTest::MockEnrollmentHelperCreator<false>);

  SetUpEnrollmentScreen();

  // Remove jitter to enable deterministic testing.
  enrollment_screen_->retry_policy_.jitter_factor = 0;

  // Start zero-touch enrollment.
  enrollment_screen_->Show();

  // Fast forward time by 1 minute.
  FastForwardTime(base::TimeDelta::FromMinutes(1));

  // Check that we have retried 4 times.
  EXPECT_EQ(enrollment_screen_->num_retries_, 4);
}

TEST_F(ZeroTouchEnrollmentScreenUnitTest, DoesNotRetryOnTopOfUser) {
  // Define behavior of EnterpriseEnrollmentHelperMock to always fail
  // enrollment.
  EnterpriseEnrollmentHelper::SetupEnrollmentHelperMock(
      &ZeroTouchEnrollmentScreenUnitTest::MockEnrollmentHelperCreator<false>);

  SetUpEnrollmentScreen();

  // Remove jitter to enable deterministic testing.
  enrollment_screen_->retry_policy_.jitter_factor = 0;

  // Start zero-touch enrollment.
  enrollment_screen_->Show();

  // Schedule user retry button click after 30 sec.
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&EnrollmentScreen::OnRetry,
                     enrollment_screen_->weak_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromSeconds(30));

  // Fast forward time by 1 minute.
  FastForwardTime(base::TimeDelta::FromMinutes(1));

  // Check that the number of retries is still 4.
  EXPECT_EQ(enrollment_screen_->num_retries_, 4);
}

TEST_F(ZeroTouchEnrollmentScreenUnitTest, DoesNotRetryAfterSuccess) {
  // Define behavior of EnterpriseEnrollmentHelperMock to successfully enroll.
  EnterpriseEnrollmentHelper::SetupEnrollmentHelperMock(
      &ZeroTouchEnrollmentScreenUnitTest::MockEnrollmentHelperCreator<true>);

  SetUpEnrollmentScreen();

  // Start zero-touch enrollment.
  enrollment_screen_->Show();

  // Fast forward time by 1 minute.
  FastForwardTime(base::TimeDelta::FromMinutes(1));

  // Check that we do not retry.
  EXPECT_EQ(enrollment_screen_->num_retries_, 0);
}

TEST_F(ZeroTouchEnrollmentScreenUnitTest, FinishesEnrollmentFlow) {
  // Define behavior of EnterpriseEnrollmentHelperMock to successfully enroll.
  EnterpriseEnrollmentHelper::SetupEnrollmentHelperMock(
      &ZeroTouchEnrollmentScreenUnitTest::MockEnrollmentHelperCreator<true>);

  SetUpEnrollmentScreen();

  // Set up expectation for BaseScreenDelegate::OnExit to be called
  // with BaseScreenDelegate::ENTERPRISE_ENROLLMENT_COMPLETED
  // This is how we check that the code finishes and cleanly exits
  // the enterprise enrollment flow.
  EXPECT_CALL(*GetBaseScreenDelegate(),
              OnExit(_, ScreenExitCode::ENTERPRISE_ENROLLMENT_COMPLETED, _))
      .Times(1);

  // Start zero-touch enrollment.
  enrollment_screen_->Show();
}

class MultiLicenseEnrollmentScreenUnitTest : public EnrollmentScreenUnitTest {
 public:
  MultiLicenseEnrollmentScreenUnitTest() = default;

  void SetUpEnrollmentScreen() override {
    enrollment_config_.mode = policy::EnrollmentConfig::MODE_MANUAL;
    enrollment_config_.auth_mechanism =
        policy::EnrollmentConfig::AUTH_MECHANISM_INTERACTIVE;
    EnrollmentScreenUnitTest::SetUpEnrollmentScreen();
  }

  static EnterpriseEnrollmentHelper* MockEnrollmentHelperCreator(
      EnterpriseEnrollmentHelper::EnrollmentStatusConsumer* status_consumer,
      const policy::EnrollmentConfig& enrollment_config,
      const std::string& enrolling_user_domain) {
    EnterpriseEnrollmentHelperMock* mock =
        new EnterpriseEnrollmentHelperMock(status_consumer);
    EXPECT_CALL(*mock, EnrollUsingAuthCode(_, _))
        .Times(AnyNumber())
        .WillRepeatedly(Invoke([mock](const std::string&, bool) {
          EnrollmentLicenseMap licenses;
          static_cast<EnrollmentScreen*>(mock->status_consumer())
              ->OnMultipleLicensesAvailable(licenses);
        }));
    EXPECT_CALL(*mock, UseLicenseType(::policy::LicenseType::ANNUAL)).Times(1);

    return mock;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MultiLicenseEnrollmentScreenUnitTest);
};

// Sign in and check that selected license type is propagated correctly.
TEST_F(MultiLicenseEnrollmentScreenUnitTest, TestLicenseSelection) {
  EnterpriseEnrollmentHelper::SetupEnrollmentHelperMock(
      &MultiLicenseEnrollmentScreenUnitTest::MockEnrollmentHelperCreator);

  EXPECT_CALL(*GetMockScreenView(), SetParameters(_, _)).Times(1);

  SetUpEnrollmentScreen();

  EXPECT_CALL(*GetMockScreenView(), Show()).Times(1);
  EXPECT_CALL(*GetMockScreenView(), ShowSigninScreen()).Times(1);

  // Start enrollment.
  enrollment_screen_->Show();

  // Once at login, once after picking license type.

  EXPECT_CALL(*GetMockScreenView(), ShowEnrollmentSpinnerScreen()).Times(2);
  EXPECT_CALL(*GetMockScreenView(), ShowLicenseTypeSelectionScreen(_)).Times(1);

  enrollment_screen_->OnLoginDone("user@domain.com", "oauth");
  enrollment_screen_->OnLicenseTypeSelected("annual");
}

}  // namespace chromeos
