// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/notification_image_reporter.h"

#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/safe_browsing/mock_permission_report_sender.h"
#include "chrome/browser/safe_browsing/ping_manager.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_service.h"
#include "chrome/common/safe_browsing/csd.pb.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/safe_browsing_db/safe_browsing_prefs.h"
#include "components/safe_browsing_db/test_database_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace safe_browsing {

namespace {

class TestingNotificationImageReporter : public NotificationImageReporter {
 public:
  explicit TestingNotificationImageReporter(
      std::unique_ptr<net::ReportSender> report_sender)
      : NotificationImageReporter(std::move(report_sender)) {}

  void WaitForReportSkipped() {
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void SetReportingChance(bool reporting_chance) {
    reporting_chance_ = reporting_chance;
  }

 protected:
  double GetReportChance() const override { return reporting_chance_; }
  void SkippedReporting() override {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&TestingNotificationImageReporter::SkippedReportingOnUI,
                   base::Unretained(this)));
  }

 private:
  void SkippedReportingOnUI() {
    if (quit_closure_) {
      quit_closure_.Run();
      quit_closure_.Reset();
    }
  }
  base::Closure quit_closure_;
  double reporting_chance_ = 1.0;
};

class FakeSafeBrowsingDatabaseManager : public TestSafeBrowsingDatabaseManager {
 public:
  bool MatchCsdWhitelistUrl(const GURL& url) override { return false; }

 private:
  ~FakeSafeBrowsingDatabaseManager() override {}
};

SkBitmap CreateBitmap(int width, int height) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(width, height);
  bitmap.eraseColor(SK_ColorGREEN);
  return bitmap;
}

}  // namespace

class NotificationImageReporterTest : public ::testing::Test {
 public:
  NotificationImageReporterTest();

  void SetUp() override;
  void TearDown() override;

 private:
  content::TestBrowserThreadBundle thread_bundle_;  // Should be first member.

 protected:
  void SetExtendedReportingLevel(ExtendedReportingLevel level);
  void ReportNotificationImage();

  scoped_refptr<SafeBrowsingService> safe_browsing_service_;

  std::unique_ptr<TestingProfile> profile_;  // Written on UI, read on IO.

  // Owned by |notification_image_reporter_|.
  MockPermissionReportSender* mock_report_sender_;

  TestingNotificationImageReporter* notification_image_reporter_;

  std::unique_ptr<base::test::ScopedFeatureList> feature_list_;

  GURL origin_;     // Written on UI, read on IO.
  SkBitmap image_;  // Written on UI, read on IO.

 private:
  void SetUpOnIO();

  void ReportNotificationImageOnIO();
};

NotificationImageReporterTest::NotificationImageReporterTest()
    // Use REAL_IO_THREAD so DCHECK_CURRENTLY_ON distinguishes IO from UI.
    : thread_bundle_(content::TestBrowserThreadBundle::REAL_IO_THREAD),
      origin_("https://example.com") {
  image_ = CreateBitmap(1 /* w */, 1 /* h */);
}

void NotificationImageReporterTest::SetUp() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Initialize SafeBrowsingService with FakeSafeBrowsingDatabaseManager.
  TestSafeBrowsingServiceFactory sb_service_factory;
  sb_service_factory.SetTestDatabaseManager(
      new FakeSafeBrowsingDatabaseManager());
  SafeBrowsingService::RegisterFactory(&sb_service_factory);
  safe_browsing_service_ = sb_service_factory.CreateSafeBrowsingService();
  SafeBrowsingService::RegisterFactory(nullptr);
  TestingBrowserProcess::GetGlobal()->SetSafeBrowsingService(
      safe_browsing_service_.get());
  g_browser_process->safe_browsing_service()->Initialize();
  base::RunLoop().RunUntilIdle();  // TODO(johnme): Might still be tasks on IO.

  profile_ = base::MakeUnique<TestingProfile>();

  base::RunLoop run_loop;
  BrowserThread::PostTaskAndReply(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&NotificationImageReporterTest::SetUpOnIO,
                 base::Unretained(this)),
      run_loop.QuitClosure());
  run_loop.Run();
}

void NotificationImageReporterTest::TearDown() {
  TestingBrowserProcess::GetGlobal()->safe_browsing_service()->ShutDown();
  base::RunLoop().RunUntilIdle();  // TODO(johnme): Might still be tasks on IO.
  TestingBrowserProcess::GetGlobal()->SetSafeBrowsingService(nullptr);
}

void NotificationImageReporterTest::SetUpOnIO() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  mock_report_sender_ = new MockPermissionReportSender;
  notification_image_reporter_ = new TestingNotificationImageReporter(
      base::WrapUnique(mock_report_sender_));
  safe_browsing_service_->ping_manager()->notification_image_reporter_ =
      base::WrapUnique(notification_image_reporter_);
}

void NotificationImageReporterTest::SetExtendedReportingLevel(
    ExtendedReportingLevel level) {
  feature_list_ = base::MakeUnique<base::test::ScopedFeatureList>();
  if (level == SBER_LEVEL_SCOUT)
    feature_list_->InitWithFeatures({safe_browsing::kOnlyShowScoutOptIn}, {});

  InitializeSafeBrowsingPrefs(profile_->GetPrefs());
  SetExtendedReportingPref(profile_->GetPrefs(), level != SBER_LEVEL_OFF);
}

void NotificationImageReporterTest::ReportNotificationImage() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&NotificationImageReporterTest::ReportNotificationImageOnIO,
                 base::Unretained(this)));
}

void NotificationImageReporterTest::ReportNotificationImageOnIO() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!safe_browsing_service_->enabled())
    return;
  safe_browsing_service_->ping_manager()->ReportNotificationImage(
      profile_.get(), safe_browsing_service_->database_manager(), origin_,
      image_);
}

TEST_F(NotificationImageReporterTest, ReportSuccess) {
  SetExtendedReportingLevel(SBER_LEVEL_SCOUT);

  ReportNotificationImage();
  mock_report_sender_->WaitForReportSent();

  ASSERT_EQ(1, mock_report_sender_->GetAndResetNumberOfReportsSent());
  EXPECT_EQ(GURL(NotificationImageReporter::kReportingUploadUrl),
            mock_report_sender_->latest_report_uri());
  EXPECT_EQ("application/octet-stream",
            mock_report_sender_->latest_content_type());

  NotificationImageReportRequest report;
  ASSERT_TRUE(report.ParseFromString(mock_report_sender_->latest_report()));
  EXPECT_EQ(origin_.spec(), report.notification_origin());
  ASSERT_TRUE(report.has_image());
  EXPECT_GT(report.image().data().size(), 0U);
  ASSERT_TRUE(report.image().has_mime_type());
  EXPECT_EQ(report.image().mime_type(), "image/png");
  ASSERT_TRUE(report.image().has_dimensions());
  EXPECT_EQ(1, report.image().dimensions().width());
  EXPECT_EQ(1, report.image().dimensions().height());
  EXPECT_FALSE(report.image().has_original_dimensions());
}

TEST_F(NotificationImageReporterTest, ImageDownscaling) {
  SetExtendedReportingLevel(SBER_LEVEL_SCOUT);

  image_ = CreateBitmap(640 /* w */, 360 /* h */);

  ReportNotificationImage();
  mock_report_sender_->WaitForReportSent();

  NotificationImageReportRequest report;
  ASSERT_TRUE(report.ParseFromString(mock_report_sender_->latest_report()));
  ASSERT_TRUE(report.has_image());
  EXPECT_GT(report.image().data().size(), 0U);
  ASSERT_TRUE(report.image().has_dimensions());
  EXPECT_EQ(512, report.image().dimensions().width());
  EXPECT_EQ(288, report.image().dimensions().height());
  ASSERT_TRUE(report.image().has_original_dimensions());
  EXPECT_EQ(640, report.image().original_dimensions().width());
  EXPECT_EQ(360, report.image().original_dimensions().height());
}

TEST_F(NotificationImageReporterTest, NoReportWithoutSBER) {
  SetExtendedReportingLevel(SBER_LEVEL_OFF);

  ReportNotificationImage();
  notification_image_reporter_->WaitForReportSkipped();

  EXPECT_EQ(0, mock_report_sender_->GetAndResetNumberOfReportsSent());
}

TEST_F(NotificationImageReporterTest, NoReportWithoutScout) {
  SetExtendedReportingLevel(SBER_LEVEL_LEGACY);

  ReportNotificationImage();
  notification_image_reporter_->WaitForReportSkipped();

  EXPECT_EQ(0, mock_report_sender_->GetAndResetNumberOfReportsSent());
}

TEST_F(NotificationImageReporterTest, NoReportWithoutReportingEnabled) {
  SetExtendedReportingLevel(SBER_LEVEL_SCOUT);
  notification_image_reporter_->SetReportingChance(0.0);

  ReportNotificationImage();
  notification_image_reporter_->WaitForReportSkipped();

  EXPECT_EQ(0, mock_report_sender_->GetAndResetNumberOfReportsSent());
}

TEST_F(NotificationImageReporterTest, MaxReportsPerDay) {
  SetExtendedReportingLevel(SBER_LEVEL_SCOUT);

  const int kMaxReportsPerDay = 5;

  for (int i = 0; i < kMaxReportsPerDay; i++) {
    ReportNotificationImage();
    mock_report_sender_->WaitForReportSent();
  }
  ReportNotificationImage();
  notification_image_reporter_->WaitForReportSkipped();

  EXPECT_EQ(kMaxReportsPerDay,
            mock_report_sender_->GetAndResetNumberOfReportsSent());
}

}  // namespace safe_browsing
