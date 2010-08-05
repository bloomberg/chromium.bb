// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/lock.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/renderer_host/resource_dispatcher_host.h"
#include "chrome/browser/safe_browsing/protocol_manager.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

// This starts the browser and keeps status of states related to SafeBrowsing.
class SafeBrowsingServiceTest : public InProcessBrowserTest {
 public:
  SafeBrowsingServiceTest()
    : safe_browsing_service_(NULL),
      is_update_in_progress_(false),
      is_initial_request_(false),
      is_update_scheduled_(false),
      is_url_match_in_db_(false) {
  }

  void UpdateSafeBrowsingStatus() {
    CHECK(safe_browsing_service_);
    AutoLock lock(update_status_mutex_);
    is_update_in_progress_ = safe_browsing_service_->IsUpdateInProgress();
    is_initial_request_ =
        safe_browsing_service_->protocol_manager_->is_initial_request();
    last_update_ = safe_browsing_service_->protocol_manager_->last_update();
    is_update_scheduled_ =
        safe_browsing_service_->protocol_manager_->update_timer_.IsRunning();
  }

  void CheckUrl(SafeBrowsingService::Client* helper, const GURL& url) {
    CHECK(safe_browsing_service_);
    AutoLock lock(update_status_mutex_);
    if (!safe_browsing_service_->CheckUrl(url, helper)) {
      safe_browsing_service_->CancelCheck(helper);
      is_url_match_in_db_ = false;
    }
    is_url_match_in_db_ = true;
  }

  bool is_url_match_in_db() {
    AutoLock l(update_status_mutex_);
    return is_url_match_in_db_;
  }

  bool is_update_in_progress() {
    AutoLock l(update_status_mutex_);
    return is_update_in_progress_;
  }

  bool is_initial_request() {
    AutoLock l(update_status_mutex_);
    return is_initial_request_;
  }

  base::Time last_update() {
    AutoLock l(update_status_mutex_);
    return last_update_;
  }

  bool is_update_scheduled() {
    AutoLock l(update_status_mutex_);
    return is_update_scheduled_;
  }

 protected:
  void InitSafeBrowsingService() {
    safe_browsing_service_ =
        g_browser_process->resource_dispatcher_host()->safe_browsing_service();
  }

  virtual void SetUpCommandLine(CommandLine* command_line) {
    // Makes sure the auto update is not triggered. This test will force the
    // update when needed.
    command_line->AppendSwitch(switches::kSbDisableAutoUpdate);
  }

 private:
  SafeBrowsingService* safe_browsing_service_;

  // Protects all variables below since they are updated on IO thread but
  // read on UI thread.
  Lock update_status_mutex_;
  // States associated with safebrowsing service updates.
  bool is_update_in_progress_;
  bool is_initial_request_;
  base::Time last_update_;
  bool is_update_scheduled_;
  // Indicates if there is a match between a URL's prefix and safebrowsing
  // database (thus potentially it is a phishing url).
  bool is_url_match_in_db_;
  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingServiceTest);
};

// A ref counted helper class that handles callbacks between IO thread and UI
// thread.
class SafeBrowsingServiceTestHelper
    : public base::RefCountedThreadSafe<SafeBrowsingServiceTestHelper>,
      public SafeBrowsingService::Client {
 public:
  explicit SafeBrowsingServiceTestHelper(
      SafeBrowsingServiceTest* safe_browsing_test)
      : safe_browsing_test_(safe_browsing_test) {
  }

  // Callbacks for SafeBrowsingService::Client. Not implemented yet.
  virtual void OnUrlCheckResult(const GURL& url,
                                SafeBrowsingService::UrlCheckResult result) {
    NOTREACHED() << "Not implemented.";
  }
  virtual void OnBlockingPageComplete(bool proceed) {
    NOTREACHED() << "Not implemented.";
  }

  // Functions and callbacks related to CheckUrl. These are used to verify if
  // a URL is a phishing URL.
  void CheckUrl(const GURL& url) {
    ChromeThread::PostTask(ChromeThread::IO, FROM_HERE, NewRunnableMethod(this,
        &SafeBrowsingServiceTestHelper::CheckUrlOnIOThread, url));
  }
  void CheckUrlOnIOThread(const GURL& url) {
    CHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
    safe_browsing_test_->CheckUrl(this, url);
    ChromeThread::PostTask(ChromeThread::UI, FROM_HERE, NewRunnableMethod(this,
        &SafeBrowsingServiceTestHelper::OnCheckUrlOnIOThreadDone));
  }
  void OnCheckUrlOnIOThreadDone() {
    StopUILoop();
  }

  // Functions and callbacks related to safebrowsing server status.
  void CheckStatusOnIOThread() {
    DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
    safe_browsing_test_->UpdateSafeBrowsingStatus();
    ChromeThread::PostTask(ChromeThread::UI, FROM_HERE, NewRunnableMethod(this,
        &SafeBrowsingServiceTestHelper::OnCheckStatusOnIOThreadDone));
  }
  void OnCheckStatusOnIOThreadDone() {
    StopUILoop();
  }
  void CheckStatusAfterDelay(int64 wait_time_sec) {
    ChromeThread::PostDelayedTask(
        ChromeThread::IO,
        FROM_HERE,
        NewRunnableMethod(this,
            &SafeBrowsingServiceTestHelper::CheckStatusOnIOThread),
        wait_time_sec * 1000);
  }

 private:
  // Stops UI loop after desired status is updated.
  void StopUILoop() {
    CHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
    MessageLoopForUI::current()->Quit();
  }

  base::OneShotTimer<SafeBrowsingServiceTestHelper> check_update_timer_;
  SafeBrowsingServiceTest* safe_browsing_test_;
  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingServiceTestHelper);
};

IN_PROC_BROWSER_TEST_F(SafeBrowsingServiceTest, SafeBrowsingSystemTest) {
  InitSafeBrowsingService();
  scoped_refptr<SafeBrowsingServiceTestHelper> safe_browsing_helper =
      new SafeBrowsingServiceTestHelper(this);

  // Waits for 1 sec and makes sure safebrowsing update is not happening.
  safe_browsing_helper->CheckStatusAfterDelay(1);
  // Loop will stop once OnCheckStatusOnIOThreadDone in safe_browsing_helper
  // is called and status from safe_browsing_service_ is checked.
  ui_test_utils::RunMessageLoop();
  EXPECT_FALSE(is_update_in_progress());
  EXPECT_TRUE(is_initial_request());
  EXPECT_FALSE(is_update_scheduled());
  EXPECT_TRUE(last_update().is_null());

  // Verify URL.
  const char test_url[] = "http://ianfette.org";
  safe_browsing_helper->CheckUrl(GURL(test_url));
  // Loop will stop once OnCheckUrlOnIOThreadDone in safe_browsing_helper
  // is called and url check is done.
  ui_test_utils::RunMessageLoop();
  EXPECT_TRUE(is_url_match_in_db());

  // TODO(lzheng): Add tests to launch a testing safebrowsing server
  // and issue requests repeatedly:
  // http://code.google.com/p/google-safe-browsing/wiki/ProtocolTesting
}
