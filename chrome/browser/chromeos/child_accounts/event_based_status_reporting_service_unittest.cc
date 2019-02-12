// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/event_based_status_reporting_service.h"

#include <memory>

#include "base/bind.h"
#include "base/macros.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/chromeos/child_accounts/consumer_status_reporting_service.h"
#include "chrome/browser/chromeos/child_accounts/consumer_status_reporting_service_factory.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/supervised_user/supervised_user_constants.h"
#include "chrome/browser/ui/app_list/arc/arc_app_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_power_manager_client.h"
#include "components/account_id/account_id.h"
#include "components/arc/common/app.mojom.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

class TestingConsumerStatusReportingService
    : public ConsumerStatusReportingService {
 public:
  explicit TestingConsumerStatusReportingService(
      content::BrowserContext* context)
      : ConsumerStatusReportingService(context) {}
  ~TestingConsumerStatusReportingService() override = default;

  void RequestImmediateStatusReport() override { performed_status_reports_++; }
  int performed_status_reports() const { return performed_status_reports_; }

 private:
  int performed_status_reports_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TestingConsumerStatusReportingService);
};

std::unique_ptr<KeyedService> CreateTestingConsumerStatusReportingService(
    content::BrowserContext* browser_context) {
  return std::unique_ptr<KeyedService>(
      new TestingConsumerStatusReportingService(browser_context));
}

}  // namespace

class EventBasedStatusReportingServiceTest : public testing::Test {
 protected:
  EventBasedStatusReportingServiceTest() = default;
  ~EventBasedStatusReportingServiceTest() override = default;

  void SetUp() override {
    DBusThreadManager::GetSetterForTesting()->SetPowerManagerClient(
        std::make_unique<FakePowerManagerClient>());

    profile_.SetSupervisedUserId(supervised_users::kChildAccountSUID);
    arc_test_.SetUp(profile());

    session_manager_.CreateSession(
        account_id(),
        chromeos::ProfileHelper::GetUserIdHashByUserIdForTesting(
            account_id().GetUserEmail()),
        true);
    session_manager_.SetSessionState(
        session_manager::SessionState::LOGIN_PRIMARY);

    ConsumerStatusReportingServiceFactory::GetInstance()->SetTestingFactory(
        profile(),
        base::BindRepeating(&CreateTestingConsumerStatusReportingService));
    ConsumerStatusReportingService* consumer_status_reporting_service =
        ConsumerStatusReportingServiceFactory::GetForBrowserContext(profile());
    test_consumer_status_reporting_service_ =
        static_cast<TestingConsumerStatusReportingService*>(
            consumer_status_reporting_service);
    service_ = std::make_unique<EventBasedStatusReportingService>(&profile_);
  }

  void TearDown() override {
    service_->Shutdown();
    arc_test_.TearDown();
    DBusThreadManager::Shutdown();
  }

  void SetConnectionType(network::mojom::ConnectionType type) {
    network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
        type);
    thread_bundle_.RunUntilIdle();
  }

  arc::mojom::AppHost* app_host() { return arc_test_.arc_app_list_prefs(); }
  Profile* profile() { return &profile_; }
  FakePowerManagerClient* power_manager_client() {
    return static_cast<FakePowerManagerClient*>(
        DBusThreadManager::Get()->GetPowerManagerClient());
  }

  TestingConsumerStatusReportingService*
  test_consumer_status_reporting_service() {
    return test_consumer_status_reporting_service_;
  }

  session_manager::SessionManager* session_manager() {
    return &session_manager_;
  }

  AccountId account_id() {
    return chromeos::ProfileHelper::Get()
        ->GetUserByProfile(profile())
        ->GetAccountId();
  }

  base::HistogramTester histogram_tester_;

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  ArcAppTest arc_test_;
  TestingProfile profile_;
  TestingConsumerStatusReportingService*
      test_consumer_status_reporting_service_;
  session_manager::SessionManager session_manager_;
  std::unique_ptr<EventBasedStatusReportingService> service_;

  DISALLOW_COPY_AND_ASSIGN(EventBasedStatusReportingServiceTest);
};

TEST_F(EventBasedStatusReportingServiceTest, ReportWhenAppInstall) {
  ASSERT_EQ(
      0, test_consumer_status_reporting_service()->performed_status_reports());
  app_host()->OnPackageAdded(arc::mojom::ArcPackageInfo::New());
  EXPECT_EQ(
      1, test_consumer_status_reporting_service()->performed_status_reports());

  histogram_tester_.ExpectBucketCount(
      EventBasedStatusReportingService::kUMAStatusReportEvent,
      EventBasedStatusReportingService::StatusReportEvent::kAppInstalled, 1);
  histogram_tester_.ExpectTotalCount(
      EventBasedStatusReportingService::kUMAStatusReportEvent, 1);
}

TEST_F(EventBasedStatusReportingServiceTest, ReportWhenAppUpdate) {
  ASSERT_EQ(
      0, test_consumer_status_reporting_service()->performed_status_reports());
  app_host()->OnPackageModified(arc::mojom::ArcPackageInfo::New());
  EXPECT_EQ(
      1, test_consumer_status_reporting_service()->performed_status_reports());

  histogram_tester_.ExpectBucketCount(
      EventBasedStatusReportingService::kUMAStatusReportEvent,
      EventBasedStatusReportingService::StatusReportEvent::kAppUpdated, 1);
  histogram_tester_.ExpectTotalCount(
      EventBasedStatusReportingService::kUMAStatusReportEvent, 1);
}

TEST_F(EventBasedStatusReportingServiceTest, DoNotReportWhenUserJustSignIn) {
  ASSERT_EQ(
      0, test_consumer_status_reporting_service()->performed_status_reports());
  session_manager()->SetSessionState(session_manager::SessionState::ACTIVE);
  EXPECT_EQ(
      0, test_consumer_status_reporting_service()->performed_status_reports());

  histogram_tester_.ExpectTotalCount(
      EventBasedStatusReportingService::kUMAStatusReportEvent, 0);
}

TEST_F(EventBasedStatusReportingServiceTest, ReportWhenSessionIsLocked) {
  ASSERT_EQ(
      0, test_consumer_status_reporting_service()->performed_status_reports());
  session_manager()->SetSessionState(session_manager::SessionState::ACTIVE);
  EXPECT_EQ(
      0, test_consumer_status_reporting_service()->performed_status_reports());
  session_manager()->SetSessionState(session_manager::SessionState::LOCKED);
  EXPECT_EQ(
      1, test_consumer_status_reporting_service()->performed_status_reports());

  histogram_tester_.ExpectBucketCount(
      EventBasedStatusReportingService::kUMAStatusReportEvent,
      EventBasedStatusReportingService::StatusReportEvent::kSessionLocked, 1);
  histogram_tester_.ExpectTotalCount(
      EventBasedStatusReportingService::kUMAStatusReportEvent, 1);
}

TEST_F(EventBasedStatusReportingServiceTest, ReportWhenSessionIsActive) {
  ASSERT_EQ(
      0, test_consumer_status_reporting_service()->performed_status_reports());
  session_manager()->SetSessionState(session_manager::SessionState::ACTIVE);
  EXPECT_EQ(
      0, test_consumer_status_reporting_service()->performed_status_reports());
  session_manager()->SetSessionState(session_manager::SessionState::LOCKED);
  EXPECT_EQ(
      1, test_consumer_status_reporting_service()->performed_status_reports());
  session_manager()->SetSessionState(session_manager::SessionState::ACTIVE);
  EXPECT_EQ(
      2, test_consumer_status_reporting_service()->performed_status_reports());

  histogram_tester_.ExpectBucketCount(
      EventBasedStatusReportingService::kUMAStatusReportEvent,
      EventBasedStatusReportingService::StatusReportEvent::kSessionActive, 1);
  histogram_tester_.ExpectBucketCount(
      EventBasedStatusReportingService::kUMAStatusReportEvent,
      EventBasedStatusReportingService::StatusReportEvent::kSessionLocked, 1);
  histogram_tester_.ExpectTotalCount(
      EventBasedStatusReportingService::kUMAStatusReportEvent, 2);
}

TEST_F(EventBasedStatusReportingServiceTest, ReportWhenDeviceGoesOnline) {
  SetConnectionType(network::mojom::ConnectionType::CONNECTION_NONE);

  ASSERT_EQ(
      0, test_consumer_status_reporting_service()->performed_status_reports());
  SetConnectionType(network::mojom::ConnectionType::CONNECTION_ETHERNET);
  EXPECT_EQ(
      1, test_consumer_status_reporting_service()->performed_status_reports());

  histogram_tester_.ExpectBucketCount(
      EventBasedStatusReportingService::kUMAStatusReportEvent,
      EventBasedStatusReportingService::StatusReportEvent::kDeviceOnline, 1);
  histogram_tester_.ExpectTotalCount(
      EventBasedStatusReportingService::kUMAStatusReportEvent, 1);
}

TEST_F(EventBasedStatusReportingServiceTest, ReportWhenSuspendIsDone) {
  ASSERT_EQ(
      0, test_consumer_status_reporting_service()->performed_status_reports());
  power_manager_client()->SendSuspendDone();
  EXPECT_EQ(
      1, test_consumer_status_reporting_service()->performed_status_reports());

  histogram_tester_.ExpectBucketCount(
      EventBasedStatusReportingService::kUMAStatusReportEvent,
      EventBasedStatusReportingService::StatusReportEvent::kSuspendDone, 1);
  histogram_tester_.ExpectTotalCount(
      EventBasedStatusReportingService::kUMAStatusReportEvent, 1);
}

TEST_F(EventBasedStatusReportingServiceTest, ReportForMultipleEvents) {
  SetConnectionType(network::mojom::ConnectionType::CONNECTION_NONE);

  ASSERT_EQ(
      0, test_consumer_status_reporting_service()->performed_status_reports());
  session_manager()->SetSessionState(session_manager::SessionState::ACTIVE);
  EXPECT_EQ(
      0, test_consumer_status_reporting_service()->performed_status_reports());
  session_manager()->SetSessionState(session_manager::SessionState::LOCKED);
  EXPECT_EQ(
      1, test_consumer_status_reporting_service()->performed_status_reports());
  session_manager()->SetSessionState(session_manager::SessionState::ACTIVE);
  EXPECT_EQ(
      2, test_consumer_status_reporting_service()->performed_status_reports());
  SetConnectionType(network::mojom::ConnectionType::CONNECTION_WIFI);
  EXPECT_EQ(
      3, test_consumer_status_reporting_service()->performed_status_reports());
  app_host()->OnPackageAdded(arc::mojom::ArcPackageInfo::New());
  EXPECT_EQ(
      4, test_consumer_status_reporting_service()->performed_status_reports());
  app_host()->OnPackageModified(arc::mojom::ArcPackageInfo::New());
  EXPECT_EQ(
      5, test_consumer_status_reporting_service()->performed_status_reports());
  power_manager_client()->SendSuspendDone();
  EXPECT_EQ(
      6, test_consumer_status_reporting_service()->performed_status_reports());

  histogram_tester_.ExpectBucketCount(
      EventBasedStatusReportingService::kUMAStatusReportEvent,
      EventBasedStatusReportingService::StatusReportEvent::kSessionLocked, 1);
  histogram_tester_.ExpectBucketCount(
      EventBasedStatusReportingService::kUMAStatusReportEvent,
      EventBasedStatusReportingService::StatusReportEvent::kSessionActive, 1);
  histogram_tester_.ExpectBucketCount(
      EventBasedStatusReportingService::kUMAStatusReportEvent,
      EventBasedStatusReportingService::StatusReportEvent::kDeviceOnline, 1);
  histogram_tester_.ExpectBucketCount(
      EventBasedStatusReportingService::kUMAStatusReportEvent,
      EventBasedStatusReportingService::StatusReportEvent::kAppInstalled, 1);
  histogram_tester_.ExpectBucketCount(
      EventBasedStatusReportingService::kUMAStatusReportEvent,
      EventBasedStatusReportingService::StatusReportEvent::kAppUpdated, 1);
  histogram_tester_.ExpectBucketCount(
      EventBasedStatusReportingService::kUMAStatusReportEvent,
      EventBasedStatusReportingService::StatusReportEvent::kSuspendDone, 1);
  histogram_tester_.ExpectTotalCount(
      EventBasedStatusReportingService::kUMAStatusReportEvent, 6);
}

}  // namespace chromeos
