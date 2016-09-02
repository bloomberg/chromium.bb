// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/permission_reporter.h"

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/test/simple_test_clock.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/permissions/permission_request_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/mock_permission_report_sender.h"
#include "chrome/browser/safe_browsing/ping_manager.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/website_settings/mock_permission_prompt_factory.h"
#include "chrome/common/safe_browsing/permission_report.pb.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/browser_thread.h"

#if !defined(OS_CHROMEOS)
#include "chrome/browser/signin/signin_manager_factory.h"
#include "components/signin/core/browser/signin_manager.h"
#endif

namespace safe_browsing {

class PermissionReporterBrowserTest : public SyncTest {
 public:
  PermissionReporterBrowserTest() : SyncTest(SINGLE_CLIENT) {}
  ~PermissionReporterBrowserTest() override {}

 protected:
  void SetUpOnMainThread() override {
    SyncTest::SetUpOnMainThread();

    base::RunLoop run_loop;
    content::BrowserThread::PostTaskAndReply(
        content::BrowserThread::IO, FROM_HERE,
        base::Bind(
            &PermissionReporterBrowserTest::AttachMockReportSenderOnIOThread,
            base::Unretained(this),
            make_scoped_refptr(g_browser_process->safe_browsing_service())),
        run_loop.QuitClosure());
    run_loop.Run();
  }

  void AttachMockReportSenderOnIOThread(
      scoped_refptr<SafeBrowsingService> safe_browsing_service) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

    mock_report_sender_ = new MockPermissionReportSender;

    safe_browsing_service->ping_manager()->permission_reporter_.reset(
        new PermissionReporter(base::WrapUnique(mock_report_sender_),
                               base::WrapUnique(new base::SimpleTestClock)));
  }

  PermissionRequestManager* GetPermissionRequestManager(Browser* browser) {
    return PermissionRequestManager::FromWebContents(
        browser->tab_strip_model()->GetActiveWebContents());
  }

  void AcceptBubble(Browser* b) { GetPermissionRequestManager(b)->Accept(); }

  MockPermissionReportSender* mock_report_sender() {
    return mock_report_sender_;
  }

 private:
  // Owned by permission reporter.
  MockPermissionReportSender* mock_report_sender_;

  DISALLOW_COPY_AND_ASSIGN(PermissionReporterBrowserTest);
};

// Test that permission action report will be sent if the user is opted into it.
IN_PROC_BROWSER_TEST_F(PermissionReporterBrowserTest,
                       PermissionActionReporting) {
  // Set up the Sync client.
  ASSERT_TRUE(SetupSync());
  Profile* profile = GetProfile(0);
  Browser* browser = CreateBrowser(profile);

  // Set up mock permission manager and prompt factory.
  PermissionRequestManager* manager = GetPermissionRequestManager(browser);
  std::unique_ptr<MockPermissionPromptFactory> mock_permission_prompt_factory =
      base::MakeUnique<MockPermissionPromptFactory>(manager);
  manager->DisplayPendingRequests();

  ASSERT_TRUE(embedded_test_server()->Start());
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser, embedded_test_server()->GetURL("/permissions/request.html"),
      1);

  mock_permission_prompt_factory->WaitForPermissionBubble();
  EXPECT_TRUE(mock_permission_prompt_factory->is_visible());

  AcceptBubble(browser);
  EXPECT_FALSE(mock_permission_prompt_factory->is_visible());

  // We need to wait for the report to be sent on the IO thread.
  mock_report_sender()->WaitForReportSent();
  EXPECT_EQ(1, mock_report_sender()->GetAndResetNumberOfReportsSent());

  PermissionReport permission_report;
  ASSERT_TRUE(
      permission_report.ParseFromString(mock_report_sender()->latest_report()));
  EXPECT_EQ(PermissionReport::GEOLOCATION, permission_report.permission());
  EXPECT_EQ(PermissionReport::GRANTED, permission_report.action());
  EXPECT_EQ(embedded_test_server()->base_url().spec(),
            permission_report.origin());
  EXPECT_EQ(PermissionReport::DESKTOP_PLATFORM,
            permission_report.platform_type());
  EXPECT_EQ(PermissionReport::NO_GESTURE, permission_report.gesture());
  EXPECT_EQ(PermissionReport::PERSIST_DECISION_UNSPECIFIED,
            permission_report.persisted());
  EXPECT_EQ(0, permission_report.num_prior_dismissals());
  EXPECT_EQ(0, permission_report.num_prior_ignores());
}

IN_PROC_BROWSER_TEST_F(PermissionReporterBrowserTest,
                       PermissionActionReportingPriorIgnoreCount) {
  // Set up the Sync client.
  ASSERT_TRUE(SetupSync());
  Profile* profile = GetProfile(0);
  Browser* browser = CreateBrowser(profile);

  // Set up mock permission manager and prompt factory.
  PermissionRequestManager* manager = GetPermissionRequestManager(browser);
  std::unique_ptr<MockPermissionPromptFactory> mock_permission_prompt_factory =
      base::MakeUnique<MockPermissionPromptFactory>(manager);
  manager->DisplayPendingRequests();

  ASSERT_TRUE(embedded_test_server()->Start());
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser, embedded_test_server()->GetURL("/permissions/request.html"),
      1);

  mock_permission_prompt_factory->WaitForPermissionBubble();
  EXPECT_TRUE(mock_permission_prompt_factory->is_visible());

  // Ignore this prompt and navigate away.
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser, embedded_test_server()->GetURL("/permissions/request.html"),
      1);

  mock_permission_prompt_factory->WaitForPermissionBubble();
  EXPECT_TRUE(mock_permission_prompt_factory->is_visible());

  // We don't need to call mock_report_sender()->WaitForReportSent() here
  // because the report has already been sent during the second call to
  // NavigateToURLBlockUntilNavigationsComplete().
  EXPECT_EQ(1, mock_report_sender()->GetAndResetNumberOfReportsSent());

  PermissionReport permission_report;
  ASSERT_TRUE(
      permission_report.ParseFromString(mock_report_sender()->latest_report()));
  EXPECT_EQ(PermissionReport::GEOLOCATION, permission_report.permission());
  EXPECT_EQ(PermissionReport::IGNORED, permission_report.action());
  EXPECT_EQ(embedded_test_server()->base_url().spec(),
            permission_report.origin());
  EXPECT_EQ(PermissionReport::DESKTOP_PLATFORM,
            permission_report.platform_type());
  EXPECT_EQ(PermissionReport::NO_GESTURE, permission_report.gesture());
  EXPECT_EQ(PermissionReport::PERSIST_DECISION_UNSPECIFIED,
            permission_report.persisted());
  EXPECT_EQ(0, permission_report.num_prior_dismissals());
  EXPECT_EQ(0, permission_report.num_prior_ignores());

  // Now accept the prompt.
  AcceptBubble(browser);

  EXPECT_FALSE(mock_permission_prompt_factory->is_visible());
  mock_report_sender()->WaitForReportSent();
  EXPECT_EQ(1, mock_report_sender()->GetAndResetNumberOfReportsSent());

  ASSERT_TRUE(
      permission_report.ParseFromString(mock_report_sender()->latest_report()));
  EXPECT_EQ(PermissionReport::GEOLOCATION, permission_report.permission());
  EXPECT_EQ(PermissionReport::GRANTED, permission_report.action());
  EXPECT_EQ(embedded_test_server()->base_url().spec(),
            permission_report.origin());
  EXPECT_EQ(PermissionReport::DESKTOP_PLATFORM,
            permission_report.platform_type());
  EXPECT_EQ(PermissionReport::NO_GESTURE, permission_report.gesture());
  EXPECT_EQ(PermissionReport::PERSIST_DECISION_UNSPECIFIED,
            permission_report.persisted());
  EXPECT_EQ(0, permission_report.num_prior_dismissals());
  // Ensure that we correctly record one prior ignore.
  EXPECT_EQ(1, permission_report.num_prior_ignores());
}

}  // namespace safe_browsing
