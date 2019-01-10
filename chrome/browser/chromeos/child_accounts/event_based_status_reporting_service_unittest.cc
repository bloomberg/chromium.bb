// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/event_based_status_reporting_service.h"

#include <memory>

#include "base/macros.h"
#include "chrome/browser/chromeos/child_accounts/consumer_status_reporting_service.h"
#include "chrome/browser/chromeos/child_accounts/consumer_status_reporting_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_constants.h"
#include "chrome/browser/ui/app_list/arc/arc_app_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/arc/common/app.mojom.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/test/test_browser_thread_bundle.h"
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
    profile_.SetSupervisedUserId(supervised_users::kChildAccountSUID);
    arc_test_.SetUp(profile());
    ConsumerStatusReportingServiceFactory::GetInstance()->SetTestingFactory(
        profile(),
        base::BindRepeating(&CreateTestingConsumerStatusReportingService));
    ConsumerStatusReportingService* consumer_status_reporting_service =
        ConsumerStatusReportingServiceFactory::GetForBrowserContext(profile());
    test_consumer_status_reporting_service_ =
        static_cast<TestingConsumerStatusReportingService*>(
            consumer_status_reporting_service);
  }

  void TearDown() override { arc_test_.TearDown(); }

  arc::mojom::AppHost* app_host() { return arc_test_.arc_app_list_prefs(); }
  Profile* profile() { return &profile_; }
  TestingConsumerStatusReportingService*
  test_consumer_status_reporting_service() {
    return test_consumer_status_reporting_service_;
  }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  ArcAppTest arc_test_;
  TestingProfile profile_;
  TestingConsumerStatusReportingService*
      test_consumer_status_reporting_service_;

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

TEST_F(EventBasedStatusReportingServiceTest, ReportForMultipleEvents) {
  EventBasedStatusReportingService service(profile());

  ASSERT_EQ(
      0, test_consumer_status_reporting_service()->performed_status_reports());
  app_host()->OnPackageAdded(arc::mojom::ArcPackageInfo::New());
  EXPECT_EQ(
      1, test_consumer_status_reporting_service()->performed_status_reports());
  app_host()->OnPackageModified(arc::mojom::ArcPackageInfo::New());
  EXPECT_EQ(
      2, test_consumer_status_reporting_service()->performed_status_reports());
}

}  // namespace chromeos
