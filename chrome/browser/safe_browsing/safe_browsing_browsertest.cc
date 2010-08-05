// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/command_line.h"
#include "base/lock.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/resource_dispatcher_host.h"
#include "chrome/browser/safe_browsing/protocol_manager.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
#include "net/base/host_resolver.h"
#include "net/base/load_flags.h"
#include "net/base/net_log.h"
#include "net/socket/tcp_pinger.h"
#include "testing/gtest/include/gtest/gtest.h"

const char kHost[] = "127.0.0.1";
const char kPort[] = "40101";
const char kDataFile[] = "datafile.dat";
const char kUrlVerifyPath[] = "/safebrowsing/verify_urls";
const char kTestCompletePath[] = "/test_complete";

class SafeBrowsingTestServer {
 public:
  SafeBrowsingTestServer(const std::string& hostname,
                         const std::string& port,
                         const std::string& datafile)
      : hostname_(hostname),
        port_(port),
        datafile_(datafile),
        server_handle_(base::kNullProcessHandle) {
  }
  ~SafeBrowsingTestServer() {
    EXPECT_EQ(server_handle_, base::kNullProcessHandle);
  }

  // Start the python server test suite.
  bool Start() {
    // Get path to python server script
    FilePath testserver_path;
    if (!PathService::Get(base::DIR_SOURCE_ROOT, &testserver_path))
      return false;
    testserver_path = testserver_path
        .Append(FILE_PATH_LITERAL("chrome"))
        .Append(FILE_PATH_LITERAL("browser"))
        .Append(FILE_PATH_LITERAL("safe_browsing"))
        .Append(FILE_PATH_LITERAL("testserver"));
    AppendToPythonPath(testserver_path);

    FilePath testserver = testserver_path.Append(
        FILE_PATH_LITERAL("safebrowsing_test_server.py"));

#if defined(OS_WIN)
    FilePath datafile = testserver_path.Append(ASCIIToWide(datafile_));
    FilePath python_runtime;
    // Get path to python interpreter
    if (!PathService::Get(base::DIR_SOURCE_ROOT, &python_runtime))
      return false;
    python_runtime = python_runtime
        .Append(FILE_PATH_LITERAL("third_party"))
        .Append(FILE_PATH_LITERAL("python_24"))
        .Append(FILE_PATH_LITERAL("python.exe"));

    std::wstring command_line =
        L"\"" + python_runtime.ToWStringHack() + L"\" " +
        L"\"" + testserver.ToWStringHack() +
        L"\" --port=" + UTF8ToWide(port_) +
        L"\" --datafile=" + datafile.ToWStringHack() + L"\"";

    if (!base::LaunchApp(command_line, false, true, &server_handle_)) {
      LOG(ERROR) << "Failed to launch " << command_line;
      return false;
    }
#elif defined(OS_POSIX)
    FilePath datafile = testserver_path.Append(datafile_);
    std::vector<std::string> command_line;
    command_line.push_back("python");
    command_line.push_back(testserver.value());
    command_line.push_back("--port=" + port_);
    command_line.push_back("--datafile=" + datafile.value());

    base::file_handle_mapping_vector no_mappings;
    LOG(INFO) << "Trying to launch " << command_line[0] << " ...";
    if (!base::LaunchApp(command_line, no_mappings, false, &server_handle_)) {
      LOG(ERROR) << "Failed to launch " << command_line[0] << " ...";
      return false;
    }
#endif

    // Let the server start, then verify that it's up.
    // Our server is Python, and takes about 500ms to start
    // up the first time, and about 200ms after that.
    if (!WaitServerToStart(hostname_, StringToInt(port_))) {
      LOG(ERROR) << "Failed to connect to server";
      Stop();
      return false;
    }

    LOG(INFO) << "Started on port " << port_;
    return true;
  }

  // Stop the python server test suite.
  bool Stop() {
    if (!server_handle_) {
      return true;
    }

    // First check if the process has already terminated.
    bool ret = base::WaitForSingleProcess(server_handle_, 0);
    if (!ret) {
      ret = base::KillProcess(server_handle_, 1, true);
    }

    if (ret) {
      base::CloseProcessHandle(server_handle_);
      server_handle_ = base::kNullProcessHandle;
      LOG(INFO) << "Stopped.";
    } else {
      LOG(INFO) << "Kill failed?";
      return false;
    }
    return true;
  }

 private:
  // Make sure the test server is actually started and ready to serve.
  bool WaitServerToStart(const std::string& host_name, int port) {
    net::AddressList addr;
    scoped_refptr<net::HostResolver> resolver(net::CreateSystemHostResolver(
        net::HostResolver::kDefaultParallelism));
    net::HostResolver::RequestInfo info(host_name, port);
    int rv = resolver->Resolve(info, &addr, NULL, NULL, net::BoundNetLog());
    if (rv != net::OK)
      return false;

    net::TCPPinger pinger(addr);
    rv = pinger.Ping(base::TimeDelta::FromSeconds(kConnectionTimeoutSec),
                     kRetryAttempts);
    return rv == net::OK;
  }

  void AppendToPythonPath(const FilePath& dir) {
#if defined(OS_WIN)
    const wchar_t kPythonPath[] = L"PYTHONPATH";
    wchar_t oldpath[8192];
    if (GetEnvironmentVariable(kPythonPath, oldpath, arraysize(oldpath)) == 0) {
      SetEnvironmentVariableW(kPythonPath, dir.value().c_str());
    } else if (!wcsstr(oldpath, dir.value().c_str())) {
      std::wstring newpath(oldpath);
      newpath.append(L";");
      newpath.append(dir.value());
      SetEnvironmentVariableW(kPythonPath, newpath.c_str());
    }
#elif defined(OS_POSIX)
    const char kPythonPath[] = "PYTHONPATH";
    const char* oldpath = getenv(kPythonPath);
    // setenv() leaks memory intentionally on Mac
    if (!oldpath) {
      setenv(kPythonPath, dir.value().c_str(), 1);
    } else if (!strstr(oldpath, dir.value().c_str())) {
      std::string newpath(oldpath);
      newpath.append(":");
      newpath.append(dir.value());
      setenv(kPythonPath, newpath.c_str(), 1);
    }
#endif
  }

  std::string hostname_;
  std::string port_;
  std::string datafile_;
  static const int kConnectionTimeoutSec = 5;
  static const int kRetryAttempts = 3;
  base::ProcessHandle server_handle_;
};

typedef struct {
  std::string url;
  std::string list_name;
  bool is_phishing;
} PhishingUrl;

// Parses server response for verify_urls. The expected format is:
//
// first.random.url.com/   internal-test-shavar   yes
// second.random.url.com/  internal-test-shavar   yes
// ...
static bool ParsePhishingUrls(const std::string& data,
                              std::vector<PhishingUrl>* phishing_urls) {
  std::vector<std::string> urls;
  SplitString(data, '\n', &urls);
  for (size_t i = 0; i < urls.size(); ++i) {
    PhishingUrl phishing_url;
    std::vector<std::string> url_parts;
    SplitStringAlongWhitespace(urls[i], &url_parts);
    if (url_parts.size() < 3) {
      CHECK(false) << "Unexpected URL format in phishing URL list: "
                   << urls[i];
      return false;
    }
    phishing_url.url = url_parts[0];
    phishing_url.list_name = url_parts[1];
    if (url_parts[2] == "yes") {
      phishing_url.is_phishing = true;
    } else {
      CHECK_EQ("no", url_parts[2]);
      phishing_url.is_phishing = false;
    }
    phishing_urls->push_back(phishing_url);
  }
  return true;
}

// This starts the browser and keeps status of states related to SafeBrowsing.
class SafeBrowsingServiceTest : public InProcessBrowserTest {
 public:
  SafeBrowsingServiceTest()
    : safe_browsing_service_(NULL),
      is_update_in_progress_(false),
      is_initial_request_(false),
      is_update_scheduled_(false),
      is_checked_url_in_db_(false),
      is_checked_url_safe_(false) {
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

  void ForceUpdate() {
    CHECK(safe_browsing_service_);
    safe_browsing_service_->protocol_manager_->ForceScheduleNextUpdate(0);
  }

  void CheckUrl(SafeBrowsingService::Client* helper, const GURL& url) {
    CHECK(safe_browsing_service_);
    AutoLock lock(update_status_mutex_);
    if (safe_browsing_service_->CheckUrl(url, helper)) {
      is_checked_url_in_db_ = false;
      return;
    }
    is_checked_url_in_db_ = true;
  }

  bool is_checked_url_in_db() {
    AutoLock l(update_status_mutex_);
    return is_checked_url_in_db_;
  }

  void set_is_checked_url_safe(bool safe) {
    AutoLock l(update_status_mutex_);
    is_checked_url_safe_ = safe;
  }

  bool is_checked_url_safe() {
    AutoLock l(update_status_mutex_);
    return is_checked_url_safe_;
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
    command_line->AppendSwitchWithValue(
        switches::kSbInfoURLPrefix, "http://localhost:40101/safebrowsing");
    command_line->AppendSwitchWithValue(
        switches::kSbMacKeyURLPrefix, "http://localhost:40101/safebrowsing");
  }

  void SetTestStep(int step) {
    std::string test_step = StringPrintf("&test_step=%d", step);
    safe_browsing_service_->protocol_manager_->set_additional_query(test_step);
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
      public URLFetcher::Delegate {
 public:
  explicit SafeBrowsingServiceTestHelper(
      SafeBrowsingServiceTest* safe_browsing_test)
      : safe_browsing_test_(safe_browsing_test) {
  }

  // Callbacks for SafeBrowsingService::Client.
  virtual void OnUrlCheckResult(const GURL& url,
                                SafeBrowsingService::UrlCheckResult result) {
    CHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
    CHECK(safe_browsing_test_->is_checked_url_in_db());
    safe_browsing_test_->set_is_checked_url_safe(
        result == SafeBrowsingService::URL_SAFE);
    ChromeThread::PostTask(ChromeThread::UI, FROM_HERE, NewRunnableMethod(this,
        &SafeBrowsingServiceTestHelper::OnCheckUrlOnIOThreadDone));
  }
  virtual void OnBlockingPageComplete(bool proceed) {
    NOTREACHED() << "Not implemented.";
  }

  // Functions and callbacks to start the safebrowsing database update.
  void ForceUpdate() {
    ChromeThread::PostTask(ChromeThread::IO, FROM_HERE, NewRunnableMethod(this,
        &SafeBrowsingServiceTestHelper::ForceUpdateInIOThread));
  }
  void ForceUpdateInIOThread() {
    CHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
    safe_browsing_test_->ForceUpdate();
    ChromeThread::PostTask(ChromeThread::UI, FROM_HERE, NewRunnableMethod(this,
        &SafeBrowsingServiceTestHelper::OnForceUpdateDone));
  }
  void OnForceUpdateDone() {
    StopUILoop();
  }

  // Functions and callbacks related to CheckUrl. These are used to verify
  // phishing URLs.
  void CheckUrl(const GURL& url) {
    ChromeThread::PostTask(ChromeThread::IO, FROM_HERE, NewRunnableMethod(this,
        &SafeBrowsingServiceTestHelper::CheckUrlOnIOThread, url));
  }
  void CheckUrlOnIOThread(const GURL& url) {
    CHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
    safe_browsing_test_->CheckUrl(this, url);
    if (!safe_browsing_test_->is_checked_url_in_db()) {
      // Ends the checking since this url's prefix is not in database.
      ChromeThread::PostTask(ChromeThread::UI, FROM_HERE, NewRunnableMethod(
          this, &SafeBrowsingServiceTestHelper::OnCheckUrlOnIOThreadDone));
    }
    // Otherwise, OnCheckUrlOnIOThreadDone is called in OnUrlCheckResult since
    // safebrowsing service further fetches hashes from safebrowsing server.
  }
  void OnCheckUrlOnIOThreadDone() {
    StopUILoop();
  }

  // Functions and callbacks related to safebrowsing server status.
  void CheckStatusAfterDelay(int64 wait_time_sec) {
    ChromeThread::PostDelayedTask(
        ChromeThread::IO,
        FROM_HERE,
        NewRunnableMethod(this,
            &SafeBrowsingServiceTestHelper::CheckStatusOnIOThread),
        wait_time_sec * 1000);
  }
  void CheckStatusOnIOThread() {
    DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
    safe_browsing_test_->UpdateSafeBrowsingStatus();
    ChromeThread::PostTask(ChromeThread::UI, FROM_HERE, NewRunnableMethod(this,
        &SafeBrowsingServiceTestHelper::OnCheckStatusOnIOThreadDone));
  }
  void OnCheckStatusOnIOThreadDone() {
    StopUILoop();
  }

  // Calls test server to fetch urls for verification.
  void FetchUrlsToVerify(const std::string& host,
                         const std::string& port,
                         int test_step) {
    GURL url(StringPrintf("http://%s:%s/%s?"
                          "client=chromium&appver=1.0&pver=2.2&test_step=%d",
                          host.c_str(), port.c_str(), kUrlVerifyPath,
                          test_step));
    url_fetcher_.reset(new URLFetcher(url, URLFetcher::GET, this));
    url_fetcher_->set_load_flags(net::LOAD_DISABLE_CACHE);
    url_fetcher_->set_request_context(Profile::GetDefaultRequestContext());
    url_fetcher_->Start();
  }

  // Calls test server to check if test data is done.
  void VerifyTestComplete(const std::string& host,
                          const std::string& port,
                          int test_step) {
    GURL url(StringPrintf("http://%s:%s/%s?test_step=%d",
                          host.c_str(), port.c_str(), kTestCompletePath,
                          test_step));
    url_fetcher_.reset(new URLFetcher(url, URLFetcher::GET, this));
    url_fetcher_->set_load_flags(net::LOAD_DISABLE_CACHE);
    url_fetcher_->set_request_context(Profile::GetDefaultRequestContext());
    url_fetcher_->Start();
  }

  // Callback for URLFetcher.
  virtual void OnURLFetchComplete(const URLFetcher* source,
                                  const GURL& url,
                                  const URLRequestStatus& status,
                                  int response_code,
                                  const ResponseCookies& cookies,
                                  const std::string& data) {
    response_data_ = data;
    StopUILoop();
  }

  const std::string& response_data() {
    return response_data_;
  }

 private:
  // Stops UI loop after desired status is updated.
  void StopUILoop() {
    CHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
    MessageLoopForUI::current()->Quit();
  }

  base::OneShotTimer<SafeBrowsingServiceTestHelper> check_update_timer_;
  SafeBrowsingServiceTest* safe_browsing_test_;
  scoped_ptr<URLFetcher> url_fetcher_;
  std::string response_data_;
  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingServiceTestHelper);
};

IN_PROC_BROWSER_TEST_F(SafeBrowsingServiceTest, SafeBrowsingSystemTest) {
  InitSafeBrowsingService();
  scoped_refptr<SafeBrowsingServiceTestHelper> safe_browsing_helper =
      new SafeBrowsingServiceTestHelper(this);

  // Waits for 1 sec and makes sure safebrowsing update is not happening.
  // MessageLoop will stop once OnCheckStatusOnIOThreadDone in
  // safe_browsing_helper is called and status from safe_browsing_service_
  // is checked.
  safe_browsing_helper->CheckStatusAfterDelay(1);
  ui_test_utils::RunMessageLoop();
  EXPECT_FALSE(is_update_in_progress());
  EXPECT_TRUE(is_initial_request());
  EXPECT_FALSE(is_update_scheduled());
  EXPECT_TRUE(last_update().is_null());

  // Verify with a test phishing URL. Loop will stop once
  // OnCheckUrlOnIOThreadDone in safe_browsing_helper is called and URL check
  // is done.
  const char test_url[] = "http://ianfette.org";
  safe_browsing_helper->CheckUrl(GURL(test_url));
  ui_test_utils::RunMessageLoop();
  EXPECT_TRUE(is_checked_url_in_db());

  SafeBrowsingTestServer test_server(kHost, kPort, kDataFile);
  test_server.Start();
  // Limits max test steps so the test won't run wild.
  const int kMaxSteps = 10;
  int last_step = 0;
  // Starts from test step 1.
  for (int step = 1; step < kMaxSteps; step++) {
    // Every step should be a fresh start.
    EXPECT_FALSE(is_update_in_progress());
    EXPECT_FALSE(is_update_scheduled());

    // Starts safebrowsing update on IO thread. Waits till scheduled
    // update finishes. Stops waiting after kMaxWaitSec if the update
    // could not finish.
    base::Time now = base::Time::Now();
    SetTestStep(step);
    safe_browsing_helper->ForceUpdate();
    ui_test_utils::RunMessageLoop();
    int wait_sec = 0;
    const int kMaxWaitSec = 10;
    while ((is_update_scheduled() || is_update_in_progress()) &&
           wait_sec++ < kMaxWaitSec) {
      safe_browsing_helper->CheckStatusAfterDelay(1);
      ui_test_utils::RunMessageLoop();
    }
    EXPECT_LT(wait_sec, kMaxWaitSec)
        << "Can't finish update in required time limit!";
    if (last_update() < now) {
      // This means no data available anymore.
      break;
    }

    // Fetches urls to verify and waits till server responses with data.
    safe_browsing_helper->FetchUrlsToVerify(kHost, kPort, step);
    ui_test_utils::RunMessageLoop();
    LOG(INFO) << safe_browsing_helper->response_data();

    std::vector<PhishingUrl> phishing_urls;
    EXPECT_TRUE(ParsePhishingUrls(safe_browsing_helper->response_data(),
                                  &phishing_urls));
    for (size_t j = 0; j < phishing_urls.size(); ++j) {
      // Verifes with server if a URL is a phishing URL and waits till server
      // responses.
      safe_browsing_helper->CheckUrl(GURL(phishing_urls[j].url));
      ui_test_utils::RunMessageLoop();
      if (phishing_urls[j].is_phishing) {
        EXPECT_TRUE(is_checked_url_in_db());
        EXPECT_FALSE(is_checked_url_safe());
      } else {
        EXPECT_TRUE(is_checked_url_in_db());
      }
    }
    last_step = step;
  }

  // Verifies with server if test is done and waits till server responses.
  safe_browsing_helper->VerifyTestComplete(kHost, kPort, last_step);
  ui_test_utils::RunMessageLoop();
  EXPECT_EQ("yes", safe_browsing_helper->response_data());
  test_server.Stop();
}
