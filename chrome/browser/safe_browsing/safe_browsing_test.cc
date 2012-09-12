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
#include "base/process_util.h"
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/safe_browsing/protocol_manager.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/test_browser_thread.h"
#include "net/base/host_resolver.h"
#include "net/base/load_flags.h"
#include "net/base/net_log.h"
#include "net/test/python_utils.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_status.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

namespace {

const FilePath::CharType kDataFile[] =
    FILE_PATH_LITERAL("testing_input_nomac.dat");
const char kUrlVerifyPath[] = "/safebrowsing/verify_urls";
const char kDBVerifyPath[] = "/safebrowsing/verify_database";
const char kDBResetPath[] = "/reset";
const char kTestCompletePath[] = "/test_complete";

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
    phishing_url.url = std::string(chrome::kHttpScheme) +
        "://" + record_parts[0];
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

}  // namespace

class SafeBrowsingTestServer {
 public:
  explicit SafeBrowsingTestServer(const FilePath& datafile)
      : datafile_(datafile),
        server_handle_(base::kNullProcessHandle) {
  }

  ~SafeBrowsingTestServer() {
    EXPECT_EQ(base::kNullProcessHandle, server_handle_);
  }

  // Start the python server test suite.
  bool Start() {
    // Get path to python server script
    FilePath testserver_path;
    if (!PathService::Get(base::DIR_SOURCE_ROOT, &testserver_path)) {
      LOG(ERROR) << "Failed to get DIR_SOURCE_ROOT";
      return false;
    }
    testserver_path = testserver_path
        .Append(FILE_PATH_LITERAL("third_party"))
        .Append(FILE_PATH_LITERAL("safe_browsing"))
        .Append(FILE_PATH_LITERAL("testing"));
    AppendToPythonPath(testserver_path);
    FilePath testserver = testserver_path.Append(
        FILE_PATH_LITERAL("safebrowsing_test_server.py"));

    FilePath pyproto_code_dir;
    if (!GetPyProtoPath(&pyproto_code_dir)) {
      LOG(ERROR) << "Failed to get generated python protobuf dir";
      return false;
    }
    AppendToPythonPath(pyproto_code_dir);
    pyproto_code_dir = pyproto_code_dir.Append(FILE_PATH_LITERAL("google"));
    AppendToPythonPath(pyproto_code_dir);

    CommandLine cmd_line(CommandLine::NO_PROGRAM);
    EXPECT_TRUE(GetPythonCommand(&cmd_line));

    FilePath datafile = testserver_path.Append(datafile_);
    cmd_line.AppendArgPath(testserver);
    cmd_line.AppendArg(base::StringPrintf("--port=%d", kPort_));
    cmd_line.AppendArgNative(FILE_PATH_LITERAL("--datafile=") +
                             datafile.value());

    base::LaunchOptions options;
#if defined(OS_WIN)
    options.start_hidden = true;
#endif
    if (!base::LaunchProcess(cmd_line, options, &server_handle_)) {
      LOG(ERROR) << "Failed to launch server: "
                 << cmd_line.GetCommandLineString();
      return false;
    }
    return true;
  }

  // Stop the python server test suite.
  bool Stop() {
    if (server_handle_ == base::kNullProcessHandle)
      return true;

    // First check if the process has already terminated.
    if (!base::WaitForSingleProcess(server_handle_, base::TimeDelta()) &&
        !base::KillProcess(server_handle_, 1, true)) {
      VLOG(1) << "Kill failed?";
      return false;
    }

    base::CloseProcessHandle(server_handle_);
    server_handle_ = base::kNullProcessHandle;
    VLOG(1) << "Stopped.";
    return true;
  }

  static const char* Host() {
    return kHost_;
  }

  static int Port() {
    return kPort_;
  }

 private:
  static const char kHost_[];
  static const int kPort_;
  FilePath datafile_;
  base::ProcessHandle server_handle_;
  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingTestServer);
};

const char SafeBrowsingTestServer::kHost_[] = "localhost";
const int SafeBrowsingTestServer::kPort_ = 40102;

// This starts the browser and keeps status of states related to SafeBrowsing.
class SafeBrowsingServiceTest : public InProcessBrowserTest {
 public:
  SafeBrowsingServiceTest()
    : safe_browsing_service_(NULL),
      is_database_ready_(true),
      is_update_scheduled_(false),
      is_checked_url_in_db_(false),
      is_checked_url_safe_(false) {
  }

  virtual ~SafeBrowsingServiceTest() {
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
        content::Source<SafeBrowsingService>(safe_browsing_service_));
    BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
        base::Bind(&SafeBrowsingServiceTest::ForceUpdateOnIOThread,
                   this));
    observer.Wait();
  }

  void ForceUpdateOnIOThread() {
    EXPECT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));
    ASSERT_TRUE(safe_browsing_service_);
    safe_browsing_service_->protocol_manager_->ForceScheduleNextUpdate(0);
  }

  void CheckIsDatabaseReady() {
    base::AutoLock lock(update_status_mutex_);
    is_database_ready_ =
        !safe_browsing_service_->database_update_in_progress_;
  }

  void CheckUrl(SafeBrowsingService::Client* helper, const GURL& url) {
    ASSERT_TRUE(safe_browsing_service_);
    base::AutoLock lock(update_status_mutex_);
    if (safe_browsing_service_->CheckBrowseUrl(url, helper)) {
      is_checked_url_in_db_ = false;
      is_checked_url_safe_ = true;
    } else {
      // In this case, Safebrowsing service will fetch the full hash
      // from the server and examine that. Once it is done,
      // set_is_checked_url_safe() will be called via callback.
      is_checked_url_in_db_ = true;
    }
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

  MessageLoop* SafeBrowsingMessageLoop() {
    return safe_browsing_service_->safe_browsing_thread_->message_loop();
  }

 protected:
  bool InitSafeBrowsingService() {
    safe_browsing_service_ = g_browser_process->safe_browsing_service();
    return safe_browsing_service_ != NULL;
  }

  virtual void SetUpCommandLine(CommandLine* command_line) {
    // Makes sure the auto update is not triggered. This test will force the
    // update when needed.
    command_line->AppendSwitch(switches::kSbDisableAutoUpdate);

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

    // Point to the testing server for all SafeBrowsing requests.
    std::string url_prefix =
        base::StringPrintf("http://%s:%d/safebrowsing",
                           SafeBrowsingTestServer::Host(),
                           SafeBrowsingTestServer::Port());
    command_line->AppendSwitchASCII(switches::kSbURLPrefix, url_prefix);
  }

  void SetTestStep(int step) {
    std::string test_step = base::StringPrintf("test_step=%d", step);
    safe_browsing_service_->protocol_manager_->set_additional_query(test_step);
  }

 private:
  SafeBrowsingService* safe_browsing_service_;

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

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingServiceTest);
};

// A ref counted helper class that handles callbacks between IO thread and UI
// thread.
class SafeBrowsingServiceTestHelper
    : public base::RefCountedThreadSafe<SafeBrowsingServiceTestHelper>,
      public SafeBrowsingService::Client,
      public net::URLFetcherDelegate {
 public:
  SafeBrowsingServiceTestHelper(SafeBrowsingServiceTest* safe_browsing_test,
                                net::URLRequestContextGetter* request_context)
      : safe_browsing_test_(safe_browsing_test),
        response_status_(net::URLRequestStatus::FAILED),
        request_context_(request_context) {
  }

  // Callbacks for SafeBrowsingService::Client.
  virtual void OnBrowseUrlCheckResult(
      const GURL& url, SafeBrowsingService::UrlCheckResult result) {
    EXPECT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));
    EXPECT_TRUE(safe_browsing_test_->is_checked_url_in_db());
    safe_browsing_test_->set_is_checked_url_safe(
        result == SafeBrowsingService::SAFE);
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&SafeBrowsingServiceTestHelper::OnCheckUrlDone,
                   this));
  }
  virtual void OnDownloadUrlCheckResult(
      const std::vector<GURL>& url_chain,
      SafeBrowsingService::UrlCheckResult result) {
    // TODO(lzheng): Add test for DownloadUrl.
  }

  virtual void OnBlockingPageComplete(bool proceed) {
    NOTREACHED() << "Not implemented.";
  }

  // Functions and callbacks related to CheckUrl. These are used to verify
  // phishing URLs.
  void CheckUrl(const GURL& url) {
    BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
        base::Bind(&SafeBrowsingServiceTestHelper::CheckUrlOnIOThread,
                   this, url));
    content::RunMessageLoop();
  }
  void CheckUrlOnIOThread(const GURL& url) {
    EXPECT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));
    safe_browsing_test_->CheckUrl(this, url);
    if (!safe_browsing_test_->is_checked_url_in_db()) {
      // Ends the checking since this URL's prefix is not in database.
      BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&SafeBrowsingServiceTestHelper::OnCheckUrlDone,
                   this));
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
        base::Bind(&SafeBrowsingServiceTestHelper::CheckIsDatabaseReady, this));
  }

  // Checks status in SafeBrowsing Thread.
  void CheckIsDatabaseReady() {
    EXPECT_EQ(MessageLoop::current(),
              safe_browsing_test_->SafeBrowsingMessageLoop());
    safe_browsing_test_->CheckIsDatabaseReady();
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&SafeBrowsingServiceTestHelper::OnWaitForStatusUpdateDone,
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
        base::Bind(&SafeBrowsingServiceTestHelper::CheckStatusOnIOThread,
                   this));
    // Will continue after OnWaitForStatusUpdateDone().
    content::RunMessageLoop();
  }

  void WaitTillServerReady(const char* host, int port) {
    response_status_ = net::URLRequestStatus::FAILED;
    GURL url(base::StringPrintf("http://%s:%d%s?test_step=0",
                                host, port, kDBResetPath));
    // TODO(lzheng): We should have a way to reliably tell when a server is
    // ready so we could get rid of the Sleep and retry loop.
    while (true) {
      if (FetchUrl(url) == net::URLRequestStatus::SUCCESS)
        break;
      // Wait and try again if last fetch was failed. The loop will hit the
      // timeout in OutOfProcTestRunner if the fetch can not get success
      // response.
      base::PlatformThread::Sleep(TestTimeouts::tiny_timeout());
    }
  }

  // Calls test server to fetch database for verification.
  net::URLRequestStatus::Status FetchDBToVerify(const char* host, int port,
                                                int test_step) {
    // TODO(lzheng): Remove chunk_type=add once it is not needed by the server.
    GURL url(base::StringPrintf(
        "http://%s:%d%s?"
        "client=chromium&appver=1.0&pver=2.2&test_step=%d&"
        "chunk_type=add",
        host, port, kDBVerifyPath, test_step));
    return FetchUrl(url);
  }

  // Calls test server to fetch URLs for verification.
  net::URLRequestStatus::Status FetchUrlsToVerify(const char* host, int port,
                                                  int test_step) {
    GURL url(base::StringPrintf(
        "http://%s:%d%s?"
        "client=chromium&appver=1.0&pver=2.2&test_step=%d",
        host, port, kUrlVerifyPath, test_step));
    return FetchUrl(url);
  }

  // Calls test server to check if test data is done. E.g.: if there is a
  // bad URL that server expects test to fetch full hash but the test didn't,
  // this verification will fail.
  net::URLRequestStatus::Status VerifyTestComplete(const char* host, int port,
                                                   int test_step) {
    GURL url(StringPrintf("http://%s:%d%s?test_step=%d",
                          host, port, kTestCompletePath, test_step));
    return FetchUrl(url);
  }

  // Callback for URLFetcher.
  virtual void OnURLFetchComplete(const net::URLFetcher* source) {
    source->GetResponseAsString(&response_data_);
    response_status_ = source->GetStatus().status();
    StopUILoop();
  }

  const std::string& response_data() {
    return response_data_;
  }

 private:
  friend class base::RefCountedThreadSafe<SafeBrowsingServiceTestHelper>;
  virtual ~SafeBrowsingServiceTestHelper() {}

  // Stops UI loop after desired status is updated.
  void StopUILoop() {
    EXPECT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));
    MessageLoopForUI::current()->Quit();
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

  base::OneShotTimer<SafeBrowsingServiceTestHelper> check_update_timer_;
  SafeBrowsingServiceTest* safe_browsing_test_;
  scoped_ptr<net::URLFetcher> url_fetcher_;
  std::string response_data_;
  net::URLRequestStatus::Status response_status_;
  net::URLRequestContextGetter* request_context_;
  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingServiceTestHelper);
};

// See http://crbug.com/96459
IN_PROC_BROWSER_TEST_F(SafeBrowsingServiceTest,
                       DISABLED_SafeBrowsingSystemTest) {
  LOG(INFO) << "Start test";
  const char* server_host = SafeBrowsingTestServer::Host();
  int server_port = SafeBrowsingTestServer::Port();
  ASSERT_TRUE(InitSafeBrowsingService());

  net::URLRequestContextGetter* request_context =
      GetBrowserContext()->GetRequestContext();
  scoped_refptr<SafeBrowsingServiceTestHelper> safe_browsing_helper(
      new SafeBrowsingServiceTestHelper(this, request_context));
  int last_step = 0;
  FilePath datafile_path = FilePath(kDataFile);
  SafeBrowsingTestServer test_server(datafile_path);
  ASSERT_TRUE(test_server.Start());

  // Make sure the server is running.
  safe_browsing_helper->WaitTillServerReady(server_host, server_port);

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
              safe_browsing_helper->FetchUrlsToVerify(server_host,
                                                      server_port,
                                                      step));

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
              safe_browsing_helper->FetchDBToVerify(server_host,
                                                    server_port,
                                                    step));
    EXPECT_GT(safe_browsing_helper->response_data().size(), 0U);
    last_step = step;
  }

  // Verifies with server if test is done and waits till server responses.
  EXPECT_EQ(net::URLRequestStatus::SUCCESS,
            safe_browsing_helper->VerifyTestComplete(server_host,
                                                     server_port,
                                                     last_step));
  EXPECT_EQ("yes", safe_browsing_helper->response_data());
  test_server.Stop();
}
