// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This test uses the safebrowsing test server published at
// http://code.google.com/p/google-safe-browsing/ to test the safebrowsing
// protocol implemetation. Details of the safebrowsing testing flow is
// documented at
// http://code.google.com/p/google-safe-browsing/wiki/ProtocolTesting
//
// This test launches safebrowsing test server and issues several update
// requests against that server. Each update would get different data and after
// each update, the test will get a list of URLs from the test server to verify
// its repository. The test will succeed only if all updates are performed and
// URLs match what the server expected.

#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/environment.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/database_manager.h"
#include "chrome/browser/safe_browsing/local_safebrowsing_test_server.h"
#include "chrome/browser/safe_browsing/protocol_manager.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/test_browser_thread.h"
#include "net/base/load_flags.h"
#include "net/base/net_log.h"
#include "net/dns/host_resolver.h"
#include "net/test/python_utils.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_status.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

namespace {

const base::FilePath::CharType kDataFile[] =
    FILE_PATH_LITERAL("testing_input_nomac.dat");
const char kUrlVerifyPath[] = "safebrowsing/verify_urls";
const char kDBVerifyPath[] = "safebrowsing/verify_database";
const char kTestCompletePath[] = "test_complete";

struct PhishingUrl {
  std::string url;
  std::string list_name;
  bool is_phishing;
};

// Parses server response for verify_urls. The expected format is:
//
// first.random.url.com/   internal-test-shavar   yes
// second.random.url.com/  internal-test-shavar   yes
// ...
bool ParsePhishingUrls(const std::string& data,
                       std::vector<PhishingUrl>* phishing_urls) {
  if (data.empty())
    return false;

  std::vector<std::string> urls;
  base::SplitString(data, '\n', &urls);
  for (size_t i = 0; i < urls.size(); ++i) {
    if (urls[i].empty())
      continue;
    PhishingUrl phishing_url;
    std::vector<std::string> record_parts;
    base::SplitString(urls[i], '\t', &record_parts);
    if (record_parts.size() != 3) {
      LOG(ERROR) << "Unexpected URL format in phishing URL list: "
                 << urls[i];
      return false;
    }
    phishing_url.url = std::string(url::kHttpScheme) + "://" + record_parts[0];
    phishing_url.list_name = record_parts[1];
    if (record_parts[2] == "yes") {
      phishing_url.is_phishing = true;
    } else if (record_parts[2] == "no") {
      phishing_url.is_phishing = false;
    } else {
      LOG(ERROR) << "Unrecognized expectation in " << urls[i]
                 << ": " << record_parts[2];
      return false;
    }
    phishing_urls->push_back(phishing_url);
  }
  return true;
}

class FakeSafeBrowsingService : public SafeBrowsingService {
 public:
  explicit FakeSafeBrowsingService(const std::string& url_prefix)
      : url_prefix_(url_prefix) {}

  virtual SafeBrowsingProtocolConfig GetProtocolConfig() const OVERRIDE {
    SafeBrowsingProtocolConfig config;
    config.url_prefix = url_prefix_;
    // Makes sure the auto update is not triggered. The tests will force the
    // update when needed.
    config.disable_auto_update = true;
#if defined(OS_ANDROID)
    config.disable_connection_check = true;
#endif
    config.client_name = "browser_tests";
    return config;
  }

 private:
  virtual ~FakeSafeBrowsingService() {}

  std::string url_prefix_;

  DISALLOW_COPY_AND_ASSIGN(FakeSafeBrowsingService);
};

// Factory that creates FakeSafeBrowsingService instances.
class TestSafeBrowsingServiceFactory : public SafeBrowsingServiceFactory {
 public:
  explicit TestSafeBrowsingServiceFactory(const std::string& url_prefix)
      : url_prefix_(url_prefix) {}

  virtual SafeBrowsingService* CreateSafeBrowsingService() OVERRIDE {
    return new FakeSafeBrowsingService(url_prefix_);
  }

 private:
  std::string url_prefix_;
};

}  // namespace

// This starts the browser and keeps status of states related to SafeBrowsing.
class SafeBrowsingServerTest : public InProcessBrowserTest {
 public:
  SafeBrowsingServerTest()
    : safe_browsing_service_(NULL),
      is_database_ready_(true),
      is_update_scheduled_(false),
      is_checked_url_in_db_(false),
      is_checked_url_safe_(false) {
  }

  virtual ~SafeBrowsingServerTest() {
  }

  void UpdateSafeBrowsingStatus() {
    ASSERT_TRUE(safe_browsing_service_);
    base::AutoLock lock(update_status_mutex_);
    last_update_ = safe_browsing_service_->protocol_manager_->last_update();
    is_update_scheduled_ =
        safe_browsing_service_->protocol_manager_->update_timer_.IsRunning();
  }

  void ForceUpdate() {
    content::WindowedNotificationObserver observer(
        chrome::NOTIFICATION_SAFE_BROWSING_UPDATE_COMPLETE,
        content::Source<SafeBrowsingDatabaseManager>(database_manager()));
    BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
        base::Bind(&SafeBrowsingServerTest::ForceUpdateOnIOThread,
                   this));
    observer.Wait();
  }

  void ForceUpdateOnIOThread() {
    EXPECT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));
    ASSERT_TRUE(safe_browsing_service_);
    safe_browsing_service_->protocol_manager_->ForceScheduleNextUpdate(
        base::TimeDelta::FromSeconds(0));
  }

  void CheckIsDatabaseReady() {
    base::AutoLock lock(update_status_mutex_);
    is_database_ready_ = !database_manager()->database_update_in_progress_;
  }

  void CheckUrl(SafeBrowsingDatabaseManager::Client* helper, const GURL& url) {
    ASSERT_TRUE(safe_browsing_service_);
    base::AutoLock lock(update_status_mutex_);
    if (database_manager()->CheckBrowseUrl(url, helper)) {
      is_checked_url_in_db_ = false;
      is_checked_url_safe_ = true;
    } else {
      // In this case, Safebrowsing service will fetch the full hash
      // from the server and examine that. Once it is done,
      // set_is_checked_url_safe() will be called via callback.
      is_checked_url_in_db_ = true;
    }
  }

  SafeBrowsingDatabaseManager* database_manager() {
    return safe_browsing_service_->database_manager().get();
  }

  bool is_checked_url_in_db() {
    base::AutoLock l(update_status_mutex_);
    return is_checked_url_in_db_;
  }

  void set_is_checked_url_safe(bool safe) {
    base::AutoLock l(update_status_mutex_);
    is_checked_url_safe_ = safe;
  }

  bool is_checked_url_safe() {
    base::AutoLock l(update_status_mutex_);
    return is_checked_url_safe_;
  }

  bool is_database_ready() {
    base::AutoLock l(update_status_mutex_);
    return is_database_ready_;
  }

  base::Time last_update() {
    base::AutoLock l(update_status_mutex_);
    return last_update_;
  }

  bool is_update_scheduled() {
    base::AutoLock l(update_status_mutex_);
    return is_update_scheduled_;
  }

  base::MessageLoop* SafeBrowsingMessageLoop() {
    return database_manager()->safe_browsing_thread_->message_loop();
  }

  const net::SpawnedTestServer& test_server() const {
    return *test_server_;
  }

 protected:
  bool InitSafeBrowsingService() {
    safe_browsing_service_ = g_browser_process->safe_browsing_service();
    return safe_browsing_service_ != NULL;
  }

  virtual void SetUp() OVERRIDE {
    base::FilePath datafile_path;
    ASSERT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &datafile_path));

    datafile_path = datafile_path.Append(FILE_PATH_LITERAL("third_party"))
          .Append(FILE_PATH_LITERAL("safe_browsing"))
          .Append(FILE_PATH_LITERAL("testing"))
          .Append(kDataFile);
    test_server_.reset(new LocalSafeBrowsingTestServer(datafile_path));
    ASSERT_TRUE(test_server_->Start());
    LOG(INFO) << "server is " << test_server_->host_port_pair().ToString();

    // Point to the testing server for all SafeBrowsing requests.
    std::string url_prefix = test_server_->GetURL("safebrowsing").spec();
    sb_factory_.reset(new TestSafeBrowsingServiceFactory(url_prefix));
    SafeBrowsingService::RegisterFactory(sb_factory_.get());

    InProcessBrowserTest::SetUp();
  }

  virtual void TearDown() OVERRIDE {
    InProcessBrowserTest::TearDown();

    SafeBrowsingService::RegisterFactory(NULL);
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    // This test uses loopback. No need to use IPv6 especially it makes
    // local requests slow on Windows trybot when ipv6 local address [::1]
    // is not setup.
    command_line->AppendSwitch(switches::kDisableIPv6);

    // TODO(lzheng): The test server does not understand download related
    // requests. We need to fix the server.
    command_line->AppendSwitch(switches::kSbDisableDownloadProtection);

    // TODO(gcasto): Generate new testing data that includes the
    // client-side phishing whitelist.
    command_line->AppendSwitch(
        switches::kDisableClientSidePhishingDetection);

    // TODO(kalman): Generate new testing data that includes the extension
    // blacklist.
    command_line->AppendSwitch(switches::kSbDisableExtensionBlacklist);

    // TODO(tburkard): Generate new testing data that includes the side-effect
    // free whitelist.
    command_line->AppendSwitch(switches::kSbDisableSideEffectFreeWhitelist);
  }

  void SetTestStep(int step) {
    std::string test_step = base::StringPrintf("test_step=%d", step);
    safe_browsing_service_->protocol_manager_->set_additional_query(test_step);
  }

 private:
  scoped_ptr<TestSafeBrowsingServiceFactory> sb_factory_;

  SafeBrowsingService* safe_browsing_service_;

  scoped_ptr<net::SpawnedTestServer> test_server_;

  // Protects all variables below since they are read on UI thread
  // but updated on IO thread or safebrowsing thread.
  base::Lock update_status_mutex_;

  // States associated with safebrowsing service updates.
  bool is_database_ready_;
  base::Time last_update_;
  bool is_update_scheduled_;
  // Indicates if there is a match between a URL's prefix and safebrowsing
  // database (thus potentially it is a phishing URL).
  bool is_checked_url_in_db_;
  // True if last verified URL is not a phishing URL and thus it is safe.
  bool is_checked_url_safe_;

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingServerTest);
};

// A ref counted helper class that handles callbacks between IO thread and UI
// thread.
class SafeBrowsingServerTestHelper
    : public base::RefCountedThreadSafe<SafeBrowsingServerTestHelper>,
      public SafeBrowsingDatabaseManager::Client,
      public net::URLFetcherDelegate {
 public:
  SafeBrowsingServerTestHelper(SafeBrowsingServerTest* safe_browsing_test,
                               net::URLRequestContextGetter* request_context)
      : safe_browsing_test_(safe_browsing_test),
        response_status_(net::URLRequestStatus::FAILED),
        request_context_(request_context) {
  }

  // Callbacks for SafeBrowsingDatabaseManager::Client.
  virtual void OnCheckBrowseUrlResult(const GURL& url,
                                      SBThreatType threat_type) OVERRIDE {
    EXPECT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));
    EXPECT_TRUE(safe_browsing_test_->is_checked_url_in_db());
    safe_browsing_test_->set_is_checked_url_safe(
        threat_type == SB_THREAT_TYPE_SAFE);
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&SafeBrowsingServerTestHelper::OnCheckUrlDone, this));
  }

  virtual void OnBlockingPageComplete(bool proceed) {
    NOTREACHED() << "Not implemented.";
  }

  // Functions and callbacks related to CheckUrl. These are used to verify
  // phishing URLs.
  void CheckUrl(const GURL& url) {
    BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
        base::Bind(&SafeBrowsingServerTestHelper::CheckUrlOnIOThread,
                   this, url));
    content::RunMessageLoop();
  }
  void CheckUrlOnIOThread(const GURL& url) {
    EXPECT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));
    safe_browsing_test_->CheckUrl(this, url);
    if (!safe_browsing_test_->is_checked_url_in_db()) {
      // Ends the checking since this URL's prefix is not in database.
      BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&SafeBrowsingServerTestHelper::OnCheckUrlDone, this));
    }
    // Otherwise, OnCheckUrlDone is called in OnUrlCheckResult since
    // safebrowsing service further fetches hashes from safebrowsing server.
  }

  void OnCheckUrlDone() {
    StopUILoop();
  }

  // Updates status from IO Thread.
  void CheckStatusOnIOThread() {
    EXPECT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));
    safe_browsing_test_->UpdateSafeBrowsingStatus();
    safe_browsing_test_->SafeBrowsingMessageLoop()->PostTask(FROM_HERE,
        base::Bind(&SafeBrowsingServerTestHelper::CheckIsDatabaseReady, this));
  }

  // Checks status in SafeBrowsing Thread.
  void CheckIsDatabaseReady() {
    EXPECT_EQ(base::MessageLoop::current(),
              safe_browsing_test_->SafeBrowsingMessageLoop());
    safe_browsing_test_->CheckIsDatabaseReady();
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&SafeBrowsingServerTestHelper::OnWaitForStatusUpdateDone,
                   this));
  }

  void OnWaitForStatusUpdateDone() {
    StopUILoop();
  }

  // Update safebrowsing status.
  void UpdateStatus() {
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(&SafeBrowsingServerTestHelper::CheckStatusOnIOThread, this));
    // Will continue after OnWaitForStatusUpdateDone().
    content::RunMessageLoop();
  }

  // Calls test server to fetch database for verification.
  net::URLRequestStatus::Status FetchDBToVerify(
      const net::SpawnedTestServer& test_server,
      int test_step) {
    // TODO(lzheng): Remove chunk_type=add once it is not needed by the server.
    std::string path = base::StringPrintf(
        "%s?client=chromium&appver=1.0&pver=3.0&test_step=%d&chunk_type=add",
        kDBVerifyPath, test_step);
    return FetchUrl(test_server.GetURL(path));
  }

  // Calls test server to fetch URLs for verification.
  net::URLRequestStatus::Status FetchUrlsToVerify(
      const net::SpawnedTestServer& test_server,
      int test_step) {
    std::string path = base::StringPrintf(
        "%s?client=chromium&appver=1.0&pver=3.0&test_step=%d",
        kUrlVerifyPath, test_step);
    return FetchUrl(test_server.GetURL(path));
  }

  // Calls test server to check if test data is done. E.g.: if there is a
  // bad URL that server expects test to fetch full hash but the test didn't,
  // this verification will fail.
  net::URLRequestStatus::Status VerifyTestComplete(
      const net::SpawnedTestServer& test_server,
      int test_step) {
    std::string path = base::StringPrintf(
        "%s?test_step=%d", kTestCompletePath, test_step);
    return FetchUrl(test_server.GetURL(path));
  }

  // Callback for URLFetcher.
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE {
    source->GetResponseAsString(&response_data_);
    response_status_ = source->GetStatus().status();
    StopUILoop();
  }

  const std::string& response_data() {
    return response_data_;
  }

 private:
  friend class base::RefCountedThreadSafe<SafeBrowsingServerTestHelper>;
  virtual ~SafeBrowsingServerTestHelper() {}

  // Stops UI loop after desired status is updated.
  void StopUILoop() {
    EXPECT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));
    base::MessageLoopForUI::current()->Quit();
  }

  // Fetch a URL. If message_loop_started is true, starts the message loop
  // so the caller could wait till OnURLFetchComplete is called.
  net::URLRequestStatus::Status FetchUrl(const GURL& url) {
    url_fetcher_.reset(net::URLFetcher::Create(
        url, net::URLFetcher::GET, this));
    url_fetcher_->SetLoadFlags(net::LOAD_DISABLE_CACHE);
    url_fetcher_->SetRequestContext(request_context_);
    url_fetcher_->Start();
    content::RunMessageLoop();
    return response_status_;
  }

  base::OneShotTimer<SafeBrowsingServerTestHelper> check_update_timer_;
  SafeBrowsingServerTest* safe_browsing_test_;
  scoped_ptr<net::URLFetcher> url_fetcher_;
  std::string response_data_;
  net::URLRequestStatus::Status response_status_;
  net::URLRequestContextGetter* request_context_;
  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingServerTestHelper);
};

// TODO(shess): Disabled pending new data for third_party/safe_browsing/testing/
IN_PROC_BROWSER_TEST_F(SafeBrowsingServerTest,
                       DISABLED_SafeBrowsingServerTest) {
  LOG(INFO) << "Start test";
  ASSERT_TRUE(InitSafeBrowsingService());

  net::URLRequestContextGetter* request_context =
      browser()->profile()->GetRequestContext();
  scoped_refptr<SafeBrowsingServerTestHelper> safe_browsing_helper(
      new SafeBrowsingServerTestHelper(this, request_context));
  int last_step = 0;

  // Waits and makes sure safebrowsing update is not happening.
  // The wait will stop once OnWaitForStatusUpdateDone in
  // safe_browsing_helper is called and status from safe_browsing_service_
  // is checked.
  safe_browsing_helper->UpdateStatus();
  EXPECT_TRUE(is_database_ready());
  EXPECT_FALSE(is_update_scheduled());
  EXPECT_TRUE(last_update().is_null());
  // Starts updates. After each update, the test will fetch a list of URLs with
  // expected results to verify with safebrowsing service. If there is no error,
  // the test moves on to the next step to get more update chunks.
  // This repeats till there is no update data.
  for (int step = 1;; step++) {
    // Every step should be a fresh start.
    SCOPED_TRACE(base::StringPrintf("step=%d", step));
    EXPECT_TRUE(is_database_ready());
    EXPECT_FALSE(is_update_scheduled());

    // Starts safebrowsing update on IO thread. Waits till scheduled
    // update finishes.
    base::Time now = base::Time::Now();
    SetTestStep(step);
    ForceUpdate();

    safe_browsing_helper->UpdateStatus();
    EXPECT_TRUE(is_database_ready());
    EXPECT_FALSE(is_update_scheduled());
    EXPECT_FALSE(last_update().is_null());
    if (last_update() < now) {
      // This means no data available anymore.
      break;
    }

    // Fetches URLs to verify and waits till server responses with data.
    EXPECT_EQ(net::URLRequestStatus::SUCCESS,
              safe_browsing_helper->FetchUrlsToVerify(test_server(), step));

    std::vector<PhishingUrl> phishing_urls;
    EXPECT_TRUE(ParsePhishingUrls(safe_browsing_helper->response_data(),
                                  &phishing_urls));
    EXPECT_GT(phishing_urls.size(), 0U);
    for (size_t j = 0; j < phishing_urls.size(); ++j) {
      // Verifes with server if a URL is a phishing URL and waits till server
      // responses.
      safe_browsing_helper->CheckUrl(GURL(phishing_urls[j].url));
      if (phishing_urls[j].is_phishing) {
        EXPECT_TRUE(is_checked_url_in_db())
            << phishing_urls[j].url
            << " is_phishing: " << phishing_urls[j].is_phishing
            << " test step: " << step;
        EXPECT_FALSE(is_checked_url_safe())
            << phishing_urls[j].url
            << " is_phishing: " << phishing_urls[j].is_phishing
            << " test step: " << step;
      } else {
        EXPECT_TRUE(is_checked_url_safe())
            << phishing_urls[j].url
            << " is_phishing: " << phishing_urls[j].is_phishing
            << " test step: " << step;
      }
    }
    // TODO(lzheng): We should verify the fetched database with local
    // database to make sure they match.
    EXPECT_EQ(net::URLRequestStatus::SUCCESS,
              safe_browsing_helper->FetchDBToVerify(test_server(), step));
    EXPECT_GT(safe_browsing_helper->response_data().size(), 0U);
    last_step = step;
  }

  // Verifies with server if test is done and waits till server responses.
  EXPECT_EQ(net::URLRequestStatus::SUCCESS,
            safe_browsing_helper->VerifyTestComplete(test_server(), last_step));
  EXPECT_EQ("yes", safe_browsing_helper->response_data());
}
