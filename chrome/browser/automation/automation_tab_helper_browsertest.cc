// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/file_path.h"
#include "base/path_service.h"
#include "base/stringprintf.h"
#include "chrome/browser/automation/automation_tab_helper.h"
#include "chrome/browser/automation/mock_tab_event_observer.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/notification_details.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "content/common/notification_service.h"
#include "content/common/notification_source.h"
#include "content/common/notification_type.h"
#include "net/base/net_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

class MockNotificationObserver : public NotificationObserver {
 public:
  MockNotificationObserver() { }
  virtual ~MockNotificationObserver() { }

  MOCK_METHOD3(Observe, void(NotificationType type,
                             const NotificationSource& source,
                             const NotificationDetails& details));

  void Register(NotificationType type, const NotificationSource& source) {
    registrar_.Add(this, type, source);
  }

 private:
  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(MockNotificationObserver);
};

class AutomationTabHelperBrowserTest : public InProcessBrowserTest {
 public:
  AutomationTabHelperBrowserTest() {
    EnableDOMAutomation();
  }
  virtual ~AutomationTabHelperBrowserTest() { }

  void SetUpInProcessBrowserTestFixture() {
    EXPECT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir_));
    test_data_dir_ = test_data_dir_.AppendASCII("automation");
  }

  // Add default expectations for a client redirection initiated by script,
  // and quit the message loop when the redirect has completed. This expects
  // that the tab receives news of the redirect before the script returns.
  void ExpectClientRedirectAndBreak(
      MockTabEventObserver* mock_tab_observer,
      MockNotificationObserver* mock_notification_observer) {
    mock_notification_observer->Register(
        NotificationType::DOM_OPERATION_RESPONSE,
        NotificationService::AllSources());

    testing::InSequence expect_in_sequence;
    EXPECT_CALL(*mock_tab_observer, OnFirstPendingLoad(_));
    EXPECT_CALL(*mock_notification_observer, Observe(
        testing::Eq(NotificationType::DOM_OPERATION_RESPONSE), _, _));
    EXPECT_CALL(*mock_tab_observer, OnNoMorePendingLoads(_))
        .WillOnce(testing::InvokeWithoutArgs(
            MessageLoopForUI::current(), &MessageLoop::Quit));
  }

  // Executes javascript to execute a given test case found in the current
  // page's script. If |wait_for_response| is true, this method will not
  // return until the javascript has been executed.
  // Use with [ASSERT|EXPECT]_NO_FATAL_FAILURE.
  void RunTestCaseInJavaScript(int test_case_number, bool wait_for_response) {
    std::string script = base::StringPrintf("runTestCase(%d);",
                                            test_case_number);
    RenderViewHost* host =
        browser()->GetSelectedTabContents()->render_view_host();
    if (wait_for_response) {
      ASSERT_TRUE(ui_test_utils::ExecuteJavaScript(
          host, L"", ASCIIToWide(script)));
    } else {
      script += "window.domAutomationController.setAutomationId(0);"
                "window.domAutomationController.send(0);";
      host->ExecuteJavascriptInWebFrame(ASCIIToUTF16(""), ASCIIToUTF16(script));
    }
  }

  // Returns the |AutomationTabHelper| for the first browser's first tab.
  AutomationTabHelper* tab_helper() {
    return browser()->GetTabContentsWrapperAt(0)->automation_tab_helper();
  }

 protected:
  FilePath test_data_dir_;
};

IN_PROC_BROWSER_TEST_F(AutomationTabHelperBrowserTest, FormSubmission) {
  ui_test_utils::NavigateToURL(browser(), net::FilePathToFileURL(
      test_data_dir_.AppendASCII("client_redirects.html")));

  MockNotificationObserver mock_notification_observer;
  MockTabEventObserver mock_observer(tab_helper());

  ExpectClientRedirectAndBreak(&mock_observer, &mock_notification_observer);

  ASSERT_NO_FATAL_FAILURE(RunTestCaseInJavaScript(1, false));
  ui_test_utils::RunMessageLoop();
}

IN_PROC_BROWSER_TEST_F(AutomationTabHelperBrowserTest,
                       CancelFormSubmission) {
  ui_test_utils::NavigateToURL(browser(), net::FilePathToFileURL(
      test_data_dir_.AppendASCII("client_redirects.html")));

  MockNotificationObserver mock_notification_observer;
  MockTabEventObserver mock_observer(tab_helper());

  testing::InSequence expect_in_sequence;
  EXPECT_CALL(mock_observer, OnFirstPendingLoad(_)).Times(0);

  ASSERT_NO_FATAL_FAILURE(RunTestCaseInJavaScript(2, true));
}

IN_PROC_BROWSER_TEST_F(AutomationTabHelperBrowserTest,
                       JsRedirectToSite) {
  ui_test_utils::NavigateToURL(browser(), net::FilePathToFileURL(
      test_data_dir_.AppendASCII("client_redirects.html")));

  MockNotificationObserver mock_notification_observer;
  MockTabEventObserver mock_observer(tab_helper());

  ExpectClientRedirectAndBreak(&mock_observer, &mock_notification_observer);

  ASSERT_NO_FATAL_FAILURE(RunTestCaseInJavaScript(3, false));
  ui_test_utils::RunMessageLoop();
}

IN_PROC_BROWSER_TEST_F(AutomationTabHelperBrowserTest,
                       JsRedirectToUnknownSite) {
  ui_test_utils::NavigateToURL(browser(), net::FilePathToFileURL(
      test_data_dir_.AppendASCII("client_redirects.html")));

  MockNotificationObserver mock_notification_observer;
  MockTabEventObserver mock_observer(tab_helper());

  ExpectClientRedirectAndBreak(&mock_observer, &mock_notification_observer);

  ASSERT_NO_FATAL_FAILURE(RunTestCaseInJavaScript(4, false));
  ui_test_utils::RunMessageLoop();
}

IN_PROC_BROWSER_TEST_F(AutomationTabHelperBrowserTest,
                       MetaTagRedirect) {
  MockTabEventObserver mock_observer(tab_helper());

  testing::InSequence expect_in_sequence;
  EXPECT_CALL(mock_observer, OnFirstPendingLoad(_));
  EXPECT_CALL(mock_observer, OnNoMorePendingLoads(_));

  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(),
      net::FilePathToFileURL(test_data_dir_.AppendASCII("meta_redirect.html")),
      2);
}

// Tests that the load stop event occurs after the window onload handler.
IN_PROC_BROWSER_TEST_F(AutomationTabHelperBrowserTest,
                       LoadStopComesAfterOnLoad) {
  MockNotificationObserver mock_notification_observer;
  mock_notification_observer.Register(NotificationType::DOM_OPERATION_RESPONSE,
                                      NotificationService::AllSources());
  MockTabEventObserver mock_tab_observer(tab_helper());

  EXPECT_CALL(mock_tab_observer, OnFirstPendingLoad(_));
  {
    testing::InSequence expect_in_sequence;
    EXPECT_CALL(mock_notification_observer, Observe(
        testing::Eq(NotificationType::DOM_OPERATION_RESPONSE), _, _));
    EXPECT_CALL(mock_tab_observer, OnNoMorePendingLoads(_));
  }

  ui_test_utils::NavigateToURL(browser(), net::FilePathToFileURL(
      test_data_dir_.AppendASCII("sends_message_on_load.html")));
}

// Tests that a crashed tab reports that it has stopped loading.
IN_PROC_BROWSER_TEST_F(AutomationTabHelperBrowserTest,
                       CrashedTabStopsLoading) {
  MockTabEventObserver mock_tab_observer(tab_helper());

  testing::InSequence expect_in_sequence;
  EXPECT_CALL(mock_tab_observer, OnFirstPendingLoad(_));
  EXPECT_CALL(mock_tab_observer, OnNoMorePendingLoads(_));

  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kAboutCrashURL));
}
