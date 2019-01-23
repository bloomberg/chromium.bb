// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/event_based_status_reporting_service.h"

#include <memory>

#include "base/macros.h"
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
#include "net/base/mock_network_change_notifier.h"
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
  }

  void TearDown() override {
    arc_test_.TearDown();
    DBusThreadManager::Shutdown();
  }

  void SetConnectionType(net::NetworkChangeNotifier::ConnectionType type) {
    notifier_.SetConnectionType(type);
    notifier_.NotifyObserversOfNetworkChangeForTests(
        notifier_.GetConnectionType());
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

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  ArcAppTest arc_test_;
  TestingProfile profile_;
  TestingConsumerStatusReportingService*
      test_consumer_status_reporting_service_;
  session_manager::SessionManager session_manager_;
  net::test::MockNetworkChangeNotifier notifier_;

  DISALLOW_COPY_AND_ASSIGN(EventBasedStatusReportingServiceTest);
};

TEST_F(EventBasedStatusReportingServiceTest, ReportWhenAppInstall) {
  EventBasedStatusReportingService service(profile());

  ASSERT_EQ(
      0, test_consumer_status_reporting_service()->performed_status_reports());
  app_host()->OnPackageAdded(arc::mojom::ArcPackageInfo::New());
  EXPECT_EQ(
      1, test_consumer_status_reporting_service()->performed_status_reports());
}

TEST_F(EventBasedStatusReportingServiceTest, ReportWhenAppUpdate) {
  EventBasedStatusReportingService service(profile());

  ASSERT_EQ(
      0, test_consumer_status_reporting_service()->performed_status_reports());
  app_host()->OnPackageModified(arc::mojom::ArcPackageInfo::New());
  EXPECT_EQ(
      1, test_consumer_status_reporting_service()->performed_status_reports());
}

TEST_F(EventBasedStatusReportingServiceTest, DoNotReportWhenUserJustSignIn) {
  EventBasedStatusReportingService service(profile());

  ASSERT_EQ(
      0, test_consumer_status_reporting_service()->performed_status_reports());
  session_manager()->SetSessionState(session_manager::SessionState::ACTIVE);
  EXPECT_EQ(
      0, test_consumer_status_reporting_service()->performed_status_reports());
}

TEST_F(EventBasedStatusReportingServiceTest, ReportWhenSessionIsLocked) {
  EventBasedStatusReportingService service(profile());

  ASSERT_EQ(
      0, test_consumer_status_reporting_service()->performed_status_reports());
  session_manager()->SetSessionState(session_manager::SessionState::ACTIVE);
  EXPECT_EQ(
      0, test_consumer_status_reporting_service()->performed_status_reports());
  session_manager()->SetSessionState(session_manager::SessionState::LOCKED);
  EXPECT_EQ(
      1, test_consumer_status_reporting_service()->performed_status_reports());
}

TEST_F(EventBasedStatusReportingServiceTest, ReportWhenSessionIsActive) {
  EventBasedStatusReportingService service(profile());

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
}

TEST_F(EventBasedStatusReportingServiceTest, ReportWhenDeviceGoesOnline) {
  EventBasedStatusReportingService service(profile());
  SetConnectionType(
      net::NetworkChangeNotifier::ConnectionType::CONNECTION_NONE);

  ASSERT_EQ(
      0, test_consumer_status_reporting_service()->performed_status_reports());
  SetConnectionType(
      net::NetworkChangeNotifier::ConnectionType::CONNECTION_ETHERNET);
  EXPECT_EQ(
      1, test_consumer_status_reporting_service()->performed_status_reports());
}

TEST_F(EventBasedStatusReportingServiceTest, ReportWhenSuspendIsDone) {
  EventBasedStatusReportingService service(profile());

  ASSERT_EQ(
      0, test_consumer_status_reporting_service()->performed_status_reports());
  power_manager_client()->SendSuspendDone();
  EXPECT_EQ(
      1, test_consumer_status_reporting_service()->performed_status_reports());
}

TEST_F(EventBasedStatusReportingServiceTest, ReportForMultipleEvents) {
  EventBasedStatusReportingService service(profile());
  SetConnectionType(
      net::NetworkChangeNotifier::ConnectionType::CONNECTION_NONE);

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
  SetConnectionType(
      net::NetworkChangeNotifier::ConnectionType::CONNECTION_WIFI);
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
}

}  // namespace chromeos
