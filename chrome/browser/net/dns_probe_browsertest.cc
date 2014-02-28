// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/prefs/pref_service.h"
#include "base/run_loop.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/net/dns_probe_test_util.h"
#include "chrome/browser/net/net_error_tab_helper.h"
#include "chrome/browser/net/url_request_mock_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/net/net_error_info.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/test/net/url_request_failed_job.h"
#include "content/test/net/url_request_mock_http_job.h"
#include "net/base/net_errors.h"
#include "net/dns/dns_test_util.h"
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_job.h"
#include "net/url_request/url_request_job_factory.h"

using base::Bind;
using base::Callback;
using base::Closure;
using base::ConstRef;
using base::FilePath;
using base::MessageLoop;
using base::Unretained;
using chrome_common_net::DnsProbeStatus;
using content::BrowserThread;
using content::URLRequestFailedJob;
using content::URLRequestMockHTTPJob;
using content::WebContents;
using google_util::LinkDoctorBaseURL;
using net::MockDnsClientRule;
using net::NetworkDelegate;
using net::URLRequest;
using net::URLRequestFilter;
using net::URLRequestJob;
using net::URLRequestJobFactory;
using ui_test_utils::NavigateToURL;
using ui_test_utils::NavigateToURLBlockUntilNavigationsComplete;

namespace chrome_browser_net {

namespace {

// Postable function to run a Closure on the UI thread.  Since
// BrowserThread::PostTask returns a bool, it can't directly be posted to
// another thread.
void RunClosureOnUIThread(const base::Closure& closure) {
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, closure);
}

// Wraps DnsProbeService and delays callbacks until someone calls
// CallDelayedCallbacks.  This allows the DnsProbeBrowserTest to enforce a
// stricter ordering of events.
class DelayingDnsProbeService : public DnsProbeService {
 public:
  DelayingDnsProbeService() {}

  virtual ~DelayingDnsProbeService() {
    EXPECT_TRUE(delayed_probes_.empty());
  }

  virtual void ProbeDns(const ProbeCallback& callback) OVERRIDE {
    delayed_probes_.push_back(callback);
  }

  void StartDelayedProbes() {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

    std::vector<ProbeCallback> probes;
    probes.swap(delayed_probes_);

    for (std::vector<ProbeCallback>::const_iterator i = probes.begin();
         i != probes.end(); ++i) {
      DnsProbeService::ProbeDns(*i);
    }
  }

  int delayed_probe_count() const {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    return delayed_probes_.size();
  }

 private:
  std::vector<ProbeCallback> delayed_probes_;
};

FilePath GetMockLinkDoctorFilePath() {
  FilePath root_http;
  PathService::Get(chrome::DIR_TEST_DATA, &root_http);
  return root_http.AppendASCII("mock-link-doctor.html");
}

// A request that can be delayed until Resume() is called.  Can also run a
// callback if destroyed without being resumed.  Resume can be called either
// before or after a the request is started.
class DelayableRequest {
 public:
  // Called by a DelayableRequest if it was set to be delayed, and has been
  // destroyed without Undelay being called.
  typedef base::Callback<void(DelayableRequest* request)> DestructionCallback;

  virtual void Resume() = 0;

 protected:
  virtual ~DelayableRequest() {}
};

class DelayableURLRequestFailedJob : public URLRequestFailedJob,
                                     public DelayableRequest {
 public:
  // |destruction_callback| is only called if a delayed request is destroyed
  // without being resumed.
  DelayableURLRequestFailedJob(net::URLRequest* request,
                               net::NetworkDelegate* network_delegate,
                               int net_error,
                               bool should_delay,
                               const DestructionCallback& destruction_callback)
      : URLRequestFailedJob(request, network_delegate, net_error),
        should_delay_(should_delay),
        start_delayed_(false),
        destruction_callback_(destruction_callback) {}

  virtual void Start() OVERRIDE {
    if (should_delay_) {
      DCHECK(!start_delayed_);
      start_delayed_ = true;
      return;
    }
    URLRequestFailedJob::Start();
  }

  virtual void Resume() OVERRIDE {
    DCHECK(should_delay_);
    should_delay_ = false;
    if (start_delayed_) {
      start_delayed_ = false;
      Start();
    }
  }

 private:
  virtual ~DelayableURLRequestFailedJob() {
    if (should_delay_)
      destruction_callback_.Run(this);
  }

  bool should_delay_;
  bool start_delayed_;
  const DestructionCallback destruction_callback_;
};

class DelayableURLRequestMockHTTPJob : public URLRequestMockHTTPJob,
                                       public DelayableRequest {
 public:
  DelayableURLRequestMockHTTPJob(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate,
      const base::FilePath& file_path,
      bool should_delay,
      const DestructionCallback& destruction_callback)
      : URLRequestMockHTTPJob(request, network_delegate, file_path),
        should_delay_(should_delay),
        start_delayed_(false),
        destruction_callback_(destruction_callback) {}

  virtual void Start() OVERRIDE {
    if (should_delay_) {
      DCHECK(!start_delayed_);
      start_delayed_ = true;
      return;
    }
    URLRequestMockHTTPJob::Start();
  }

  virtual void Resume() OVERRIDE {
    DCHECK(should_delay_);
    should_delay_ = false;
    if (start_delayed_) {
      start_delayed_ = false;
      Start();
    }
  }

 private:
  virtual ~DelayableURLRequestMockHTTPJob() {
    if (should_delay_)
      destruction_callback_.Run(this);
  }

  bool should_delay_;
  bool start_delayed_;
  const DestructionCallback destruction_callback_;
};

// ProtocolHandler for Link Doctor requests.  Can cause requests to fail with
// an error, and/or delay a request until a test allows to continue.  Also can
// run a callback when a delayed request is cancelled.
class BreakableLinkDoctorProtocolHandler
    : public URLRequestJobFactory::ProtocolHandler {
 public:
  explicit BreakableLinkDoctorProtocolHandler(
      const FilePath& mock_link_doctor_file_path)
      : mock_link_doctor_file_path_(mock_link_doctor_file_path),
        net_error_(net::OK),
        delay_requests_(false),
        on_request_destroyed_callback_(
            base::Bind(&BreakableLinkDoctorProtocolHandler::OnRequestDestroyed,
                       base::Unretained(this))) {
  }

  virtual ~BreakableLinkDoctorProtocolHandler() {
    // All delayed requests should have been resumed or cancelled by this point.
    EXPECT_TRUE(delayed_requests_.empty());
  }

  virtual URLRequestJob* MaybeCreateJob(
      URLRequest* request,
      NetworkDelegate* network_delegate) const OVERRIDE {
    if (net_error_ != net::OK) {
      DelayableURLRequestFailedJob* job =
          new DelayableURLRequestFailedJob(
              request, network_delegate, net_error_, delay_requests_,
              on_request_destroyed_callback_);
      if (delay_requests_)
        delayed_requests_.insert(job);
      return job;
    } else {
      DelayableURLRequestMockHTTPJob* job =
          new DelayableURLRequestMockHTTPJob(
              request, network_delegate, mock_link_doctor_file_path_,
              delay_requests_, on_request_destroyed_callback_);
      if (delay_requests_)
        delayed_requests_.insert(job);
      return job;
    }
  }

  void set_net_error(int net_error) { net_error_ = net_error; }

  void SetDelayRequests(bool delay_requests) {
    delay_requests_ = delay_requests;

    // Resume all delayed requests if no longer delaying requests.
    if (!delay_requests) {
      while (!delayed_requests_.empty()) {
        DelayableRequest* request = *delayed_requests_.begin();
        delayed_requests_.erase(request);
        request->Resume();
      }
    }
  }

  // Runs |callback| once all delayed requests have been destroyed.  Does not
  // wait for delayed requests that have been resumed.
  void SetRequestDestructionCallback(const base::Closure& callback) {
    ASSERT_TRUE(delayed_request_destruction_callback_.is_null());
    if (delayed_requests_.empty()) {
      callback.Run();
      return;
    }
    delayed_request_destruction_callback_ = callback;
  }

  void OnRequestDestroyed(DelayableRequest* request) {
    ASSERT_EQ(1u, delayed_requests_.count(request));
    delayed_requests_.erase(request);
    if (delayed_requests_.empty() &&
        !delayed_request_destruction_callback_.is_null()) {
      delayed_request_destruction_callback_.Run();
      delayed_request_destruction_callback_.Reset();
    }
  }

 private:
  const FilePath mock_link_doctor_file_path_;
  int net_error_;
  bool delay_requests_;

  // Called when a request is destroyed.  Memeber variable because
  // MaybeCreateJob is "const", so calling base::Bind in that function does
  // not work well.
  const DelayableRequest::DestructionCallback on_request_destroyed_callback_;

  // Mutable is needed because MaybeCreateJob is const.
  mutable std::set<DelayableRequest*> delayed_requests_;

  base::Closure delayed_request_destruction_callback_;
};

class DnsProbeBrowserTestIOThreadHelper {
 public:
  DnsProbeBrowserTestIOThreadHelper();

  void SetUpOnIOThread(IOThread* io_thread);
  void CleanUpOnIOThreadAndDeleteHelper();

  void SetMockDnsClientRules(MockDnsClientRule::Result system_good_result,
                             MockDnsClientRule::Result public_good_result);
  void SetLinkDoctorNetError(int link_doctor_net_error);
  void SetLinkDoctorDelayRequests(bool delay_requests);
  void SetRequestDestructionCallback(const base::Closure& callback);
  void StartDelayedProbes(int expected_delayed_probe_count);

 private:
  IOThread* io_thread_;
  DnsProbeService* original_dns_probe_service_;
  DelayingDnsProbeService* delaying_dns_probe_service_;
  BreakableLinkDoctorProtocolHandler* protocol_handler_;
  FilePath mock_link_doctor_file_path_;
};

DnsProbeBrowserTestIOThreadHelper::DnsProbeBrowserTestIOThreadHelper()
    : io_thread_(NULL),
      original_dns_probe_service_(NULL),
      delaying_dns_probe_service_(NULL),
      protocol_handler_(NULL),
      mock_link_doctor_file_path_(GetMockLinkDoctorFilePath()) {}

void DnsProbeBrowserTestIOThreadHelper::SetUpOnIOThread(IOThread* io_thread) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  CHECK(io_thread);
  CHECK(!io_thread_);
  CHECK(!original_dns_probe_service_);
  CHECK(!delaying_dns_probe_service_);
  CHECK(!protocol_handler_);

  io_thread_ = io_thread;

  delaying_dns_probe_service_ = new DelayingDnsProbeService();

  IOThread::Globals* globals = io_thread_->globals();
  original_dns_probe_service_ = globals->dns_probe_service.release();
  globals->dns_probe_service.reset(delaying_dns_probe_service_);

  URLRequestFailedJob::AddUrlHandler();

  scoped_ptr<URLRequestJobFactory::ProtocolHandler> protocol_handler(
      new BreakableLinkDoctorProtocolHandler(mock_link_doctor_file_path_));
  protocol_handler_ =
      static_cast<BreakableLinkDoctorProtocolHandler*>(protocol_handler.get());
  const GURL link_doctor_base_url = LinkDoctorBaseURL();
  const std::string link_doctor_host = link_doctor_base_url.host();
  URLRequestFilter::GetInstance()->AddHostnameProtocolHandler(
      "http", link_doctor_host, protocol_handler.Pass());
}

void DnsProbeBrowserTestIOThreadHelper::CleanUpOnIOThreadAndDeleteHelper() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  URLRequestFilter::GetInstance()->ClearHandlers();

  IOThread::Globals* globals = io_thread_->globals();
  scoped_ptr<DnsProbeService> delaying_dns_probe_service(
      globals->dns_probe_service.release());
  globals->dns_probe_service.reset(original_dns_probe_service_);

  CHECK_EQ(delaying_dns_probe_service_, delaying_dns_probe_service.get());

  delete this;
}

void DnsProbeBrowserTestIOThreadHelper::SetMockDnsClientRules(
    MockDnsClientRule::Result system_result,
    MockDnsClientRule::Result public_result) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  DnsProbeService* service = io_thread_->globals()->dns_probe_service.get();
  service->SetSystemClientForTesting(
      CreateMockDnsClientForProbes(system_result));
  service->SetPublicClientForTesting(
      CreateMockDnsClientForProbes(public_result));
}

void DnsProbeBrowserTestIOThreadHelper::SetLinkDoctorNetError(
    int link_doctor_net_error) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  protocol_handler_->set_net_error(link_doctor_net_error);
}

void DnsProbeBrowserTestIOThreadHelper::SetLinkDoctorDelayRequests(
    bool delay_requests) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  protocol_handler_->SetDelayRequests(delay_requests);
}

void DnsProbeBrowserTestIOThreadHelper::SetRequestDestructionCallback(
    const base::Closure& callback) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  protocol_handler_->SetRequestDestructionCallback(callback);
}

void DnsProbeBrowserTestIOThreadHelper::StartDelayedProbes(
    int expected_delayed_probe_count) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  CHECK(delaying_dns_probe_service_);

  int actual_delayed_probe_count =
      delaying_dns_probe_service_->delayed_probe_count();
  EXPECT_EQ(expected_delayed_probe_count, actual_delayed_probe_count);

  delaying_dns_probe_service_->StartDelayedProbes();
}

class DnsProbeBrowserTest : public InProcessBrowserTest {
 public:
  DnsProbeBrowserTest();
  virtual ~DnsProbeBrowserTest();

  virtual void SetUpOnMainThread() OVERRIDE;
  virtual void CleanUpOnMainThread() OVERRIDE;

 protected:
  // Sets the browser object that other methods apply to, and that has the
  // DnsProbeStatus messages of its currently active tab monitored.
  void SetActiveBrowser(Browser* browser);

  void SetLinkDoctorBroken(bool broken);
  void SetLinkDoctorDelayRequests(bool delay_requests);
  void WaitForDelayedRequestDestruction();
  void SetMockDnsClientRules(MockDnsClientRule::Result system_result,
                             MockDnsClientRule::Result public_result);

  // These functions are often used to wait for two navigations because the Link
  // Doctor loads two pages: a blank page, so the user stops seeing the previous
  // page, and then either the Link Doctor page or a regular error page.  Often
  // need to wait for both to finish in a row.
  void NavigateToDnsError(int num_navigations);
  void NavigateToOtherError(int num_navigations);

  void StartDelayedProbes(int expected_delayed_probe_count);
  DnsProbeStatus WaitForSentStatus();
  int pending_status_count() const { return dns_probe_status_queue_.size(); }

  std::string Title();
  bool PageContains(const std::string& expected);

 private:
  void OnDnsProbeStatusSent(DnsProbeStatus dns_probe_status);

  DnsProbeBrowserTestIOThreadHelper* helper_;

  // Browser that methods apply to.
  Browser* active_browser_;
  // Helper that current has its DnsProbeStatus messages monitored.
  NetErrorTabHelper* monitored_tab_helper_;

  bool awaiting_dns_probe_status_;
  // Queue of statuses received but not yet consumed by WaitForSentStatus().
  std::list<DnsProbeStatus> dns_probe_status_queue_;
};

DnsProbeBrowserTest::DnsProbeBrowserTest()
    : helper_(new DnsProbeBrowserTestIOThreadHelper()),
      active_browser_(NULL),
      monitored_tab_helper_(NULL),
      awaiting_dns_probe_status_(false) {
}

DnsProbeBrowserTest::~DnsProbeBrowserTest() {
  // No tests should have any unconsumed probe statuses.
  EXPECT_EQ(0, pending_status_count());
}

void DnsProbeBrowserTest::SetUpOnMainThread() {
  NetErrorTabHelper::set_state_for_testing(
      NetErrorTabHelper::TESTING_DEFAULT);

  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kAlternateErrorPagesEnabled, true);

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      Bind(&DnsProbeBrowserTestIOThreadHelper::SetUpOnIOThread,
           Unretained(helper_),
           g_browser_process->io_thread()));

  SetActiveBrowser(browser());
}

void DnsProbeBrowserTest::CleanUpOnMainThread() {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      Bind(&DnsProbeBrowserTestIOThreadHelper::CleanUpOnIOThreadAndDeleteHelper,
           Unretained(helper_)));

  NetErrorTabHelper::set_state_for_testing(
      NetErrorTabHelper::TESTING_DEFAULT);
}

void DnsProbeBrowserTest::SetActiveBrowser(Browser* browser) {
  // If currently watching a NetErrorTabHelper, stop doing so before start
  // watching another.
  if (monitored_tab_helper_) {
    monitored_tab_helper_->set_dns_probe_status_snoop_callback_for_testing(
        NetErrorTabHelper::DnsProbeStatusSnoopCallback());
  }
  active_browser_ = browser;
  monitored_tab_helper_ = NetErrorTabHelper::FromWebContents(
      active_browser_->tab_strip_model()->GetActiveWebContents());
  monitored_tab_helper_->set_dns_probe_status_snoop_callback_for_testing(
      Bind(&DnsProbeBrowserTest::OnDnsProbeStatusSent, Unretained(this)));
}

void DnsProbeBrowserTest::SetLinkDoctorBroken(bool broken) {
  int net_error = broken ? net::ERR_NAME_NOT_RESOLVED : net::OK;

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      Bind(&DnsProbeBrowserTestIOThreadHelper::SetLinkDoctorNetError,
           Unretained(helper_),
           net_error));
}

void DnsProbeBrowserTest::SetLinkDoctorDelayRequests(bool delay_requests) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      Bind(&DnsProbeBrowserTestIOThreadHelper::SetLinkDoctorDelayRequests,
           Unretained(helper_),
           delay_requests));
}

void DnsProbeBrowserTest::WaitForDelayedRequestDestruction() {
  base::RunLoop run_loop;
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      Bind(&DnsProbeBrowserTestIOThreadHelper::SetRequestDestructionCallback,
           Unretained(helper_),
           base::Bind(&RunClosureOnUIThread,
                      run_loop.QuitClosure())));
  run_loop.Run();
}

void DnsProbeBrowserTest::NavigateToDnsError(int num_navigations) {
  NavigateToURLBlockUntilNavigationsComplete(
      active_browser_,
      URLRequestFailedJob::GetMockHttpUrl(net::ERR_NAME_NOT_RESOLVED),
      num_navigations);
}

void DnsProbeBrowserTest::NavigateToOtherError(int num_navigations) {
  NavigateToURLBlockUntilNavigationsComplete(
      active_browser_,
      URLRequestFailedJob::GetMockHttpUrl(net::ERR_CONNECTION_REFUSED),
      num_navigations);
}

void DnsProbeBrowserTest::SetMockDnsClientRules(
    MockDnsClientRule::Result system_result,
    MockDnsClientRule::Result public_result) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      Bind(&DnsProbeBrowserTestIOThreadHelper::SetMockDnsClientRules,
           Unretained(helper_),
           system_result,
           public_result));
}

void DnsProbeBrowserTest::StartDelayedProbes(
    int expected_delayed_probe_count) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      Bind(&DnsProbeBrowserTestIOThreadHelper::StartDelayedProbes,
           Unretained(helper_),
           expected_delayed_probe_count));
}

DnsProbeStatus DnsProbeBrowserTest::WaitForSentStatus() {
  CHECK(!awaiting_dns_probe_status_);
  while (dns_probe_status_queue_.empty()) {
    awaiting_dns_probe_status_ = true;
    MessageLoop::current()->Run();
    awaiting_dns_probe_status_ = false;
  }

  CHECK(!dns_probe_status_queue_.empty());
  DnsProbeStatus status = dns_probe_status_queue_.front();
  dns_probe_status_queue_.pop_front();
  return status;
}

// Check title by roundtripping to renderer, to make sure any probe results
// sent before this have been applied.
std::string DnsProbeBrowserTest::Title() {
  std::string title;

  WebContents* contents =
      active_browser_->tab_strip_model()->GetActiveWebContents();

  bool rv = content::ExecuteScriptAndExtractString(
      contents,
      "domAutomationController.send(document.title);",
      &title);
  if (!rv)
    return "";

  return title;
}

// Check text by roundtripping to renderer, to make sure any probe results
// sent before this have been applied.
bool DnsProbeBrowserTest::PageContains(const std::string& expected) {
  std::string text_content;

  bool rv = content::ExecuteScriptAndExtractString(
      active_browser_->tab_strip_model()->GetActiveWebContents(),
      "domAutomationController.send(document.body.textContent);",
      &text_content);
  if (!rv)
    return false;

  return text_content.find(expected) != std::string::npos;
}

void DnsProbeBrowserTest::OnDnsProbeStatusSent(
    DnsProbeStatus dns_probe_status) {
  dns_probe_status_queue_.push_back(dns_probe_status);
  if (awaiting_dns_probe_status_)
    MessageLoop::current()->Quit();
}

// Make sure probes don't break non-DNS error pages when Link Doctor loads.
IN_PROC_BROWSER_TEST_F(DnsProbeBrowserTest, OtherErrorWithWorkingLinkDoctor) {
  SetLinkDoctorBroken(false);

  NavigateToOtherError(2);
  EXPECT_EQ("Mock Link Doctor", Title());
}

// Make sure probes don't break non-DNS error pages when Link Doctor doesn't
// load.
IN_PROC_BROWSER_TEST_F(DnsProbeBrowserTest, OtherErrorWithBrokenLinkDoctor) {
  SetLinkDoctorBroken(true);

  NavigateToOtherError(2);
  EXPECT_TRUE(PageContains("CONNECTION_REFUSED"));
}

// Make sure probes don't break DNS error pages when Link doctor loads.
IN_PROC_BROWSER_TEST_F(DnsProbeBrowserTest,
                       NxdomainProbeResultWithWorkingLinkDoctor) {
  SetLinkDoctorBroken(false);
  SetMockDnsClientRules(MockDnsClientRule::OK, MockDnsClientRule::OK);

  NavigateToDnsError(2);
  EXPECT_EQ("Mock Link Doctor", Title());

  // One status for committing a blank page before the Link Doctor, and one for
  // when the Link Doctor is committed.
  EXPECT_EQ(chrome_common_net::DNS_PROBE_STARTED, WaitForSentStatus());
  EXPECT_EQ(chrome_common_net::DNS_PROBE_STARTED, WaitForSentStatus());
  EXPECT_EQ(0, pending_status_count());
  EXPECT_EQ("Mock Link Doctor", Title());

  StartDelayedProbes(1);

  EXPECT_EQ(chrome_common_net::DNS_PROBE_FINISHED_NXDOMAIN,
            WaitForSentStatus());
  EXPECT_EQ(0, pending_status_count());
  EXPECT_EQ("Mock Link Doctor", Title());
}

// Make sure probes don't break Link Doctor when probes complete before the
// Link Doctor loads.
IN_PROC_BROWSER_TEST_F(DnsProbeBrowserTest,
                       NxdomainProbeResultWithWorkingSlowLinkDoctor) {
  SetLinkDoctorBroken(false);
  SetLinkDoctorDelayRequests(true);
  SetMockDnsClientRules(MockDnsClientRule::OK, MockDnsClientRule::OK);

  NavigateToDnsError(1);
  // A blank page should be displayed while the Link Doctor page loads.
  EXPECT_EQ("", Title());

  // A single probe should be triggered by the error page load, and it should
  // be ignored.
  EXPECT_EQ(chrome_common_net::DNS_PROBE_STARTED, WaitForSentStatus());
  EXPECT_EQ(0, pending_status_count());
  EXPECT_EQ("", Title());

  StartDelayedProbes(1);
  EXPECT_EQ(chrome_common_net::DNS_PROBE_FINISHED_NXDOMAIN,
            WaitForSentStatus());
  EXPECT_EQ(0, pending_status_count());
  EXPECT_EQ("", Title());

  content::TestNavigationObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents(), 1);
  // The Link Doctor page finishes loading.
  SetLinkDoctorDelayRequests(false);
  // Wait for it to commit.
  observer.Wait();
  EXPECT_EQ("Mock Link Doctor", Title());

  // Committing the Link Doctor page should trigger sending the probe result
  // again.
  EXPECT_EQ(chrome_common_net::DNS_PROBE_FINISHED_NXDOMAIN,
            WaitForSentStatus());
  EXPECT_EQ("Mock Link Doctor", Title());
}

// Make sure probes update DNS error page properly when they're supposed to.
IN_PROC_BROWSER_TEST_F(DnsProbeBrowserTest,
                       NoInternetProbeResultWithBrokenLinkDoctor) {
  SetLinkDoctorBroken(true);
  SetMockDnsClientRules(MockDnsClientRule::TIMEOUT,
                        MockDnsClientRule::TIMEOUT);

  NavigateToDnsError(2);

  EXPECT_EQ(chrome_common_net::DNS_PROBE_STARTED, WaitForSentStatus());
  EXPECT_EQ(chrome_common_net::DNS_PROBE_STARTED, WaitForSentStatus());

  // PageContains runs the RunLoop, so make sure nothing hairy happens.
  EXPECT_EQ(0, pending_status_count());
  EXPECT_TRUE(PageContains("DNS_PROBE_STARTED"));
  EXPECT_EQ(0, pending_status_count());

  StartDelayedProbes(1);

  EXPECT_EQ(chrome_common_net::DNS_PROBE_FINISHED_NO_INTERNET,
            WaitForSentStatus());

  // PageContains runs the RunLoop, so make sure nothing hairy happens.
  EXPECT_EQ(0, pending_status_count());
  EXPECT_TRUE(PageContains("DNS_PROBE_FINISHED_NO_INTERNET"));
}

// Make sure probes don't break Link Doctor when probes complete before the
// Link Doctor request returns an error.
IN_PROC_BROWSER_TEST_F(DnsProbeBrowserTest,
                       NoInternetProbeResultWithSlowBrokenLinkDoctor) {
  SetLinkDoctorBroken(true);
  SetLinkDoctorDelayRequests(true);
  SetMockDnsClientRules(MockDnsClientRule::TIMEOUT,
                        MockDnsClientRule::TIMEOUT);

  NavigateToDnsError(1);
  // A blank page should be displayed while the Link Doctor page loads.
  EXPECT_EQ("", Title());

  // A single probe should be triggered by the error page load, and it should
  // be ignored.
  EXPECT_EQ(chrome_common_net::DNS_PROBE_STARTED, WaitForSentStatus());
  EXPECT_EQ(0, pending_status_count());
  EXPECT_EQ("", Title());

  StartDelayedProbes(1);
  EXPECT_EQ(chrome_common_net::DNS_PROBE_FINISHED_NO_INTERNET,
            WaitForSentStatus());
  EXPECT_EQ("", Title());
  EXPECT_EQ(0, pending_status_count());

  content::TestNavigationObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents(), 1);
  // The Link Doctor request fails.
  SetLinkDoctorDelayRequests(false);
  // Wait for the DNS error page to load instead.
  observer.Wait();
  // The page committing should result in sending the probe results again.
  EXPECT_EQ(chrome_common_net::DNS_PROBE_FINISHED_NO_INTERNET,
            WaitForSentStatus());

  EXPECT_EQ(0, pending_status_count());
  EXPECT_TRUE(PageContains("DNS_PROBE_FINISHED_NO_INTERNET"));
}

// Double-check to make sure sync failures don't explode.
IN_PROC_BROWSER_TEST_F(DnsProbeBrowserTest, SyncFailureWithBrokenLinkDoctor) {
  SetLinkDoctorBroken(true);
  SetMockDnsClientRules(MockDnsClientRule::FAIL, MockDnsClientRule::FAIL);

  NavigateToDnsError(2);

  EXPECT_EQ(chrome_common_net::DNS_PROBE_STARTED, WaitForSentStatus());
  EXPECT_EQ(chrome_common_net::DNS_PROBE_STARTED, WaitForSentStatus());

  // PageContains runs the RunLoop, so make sure nothing hairy happens.
  EXPECT_EQ(0, pending_status_count());
  EXPECT_TRUE(PageContains("DNS_PROBE_STARTED"));
  EXPECT_EQ(0, pending_status_count());

  StartDelayedProbes(1);

  EXPECT_EQ(chrome_common_net::DNS_PROBE_FINISHED_INCONCLUSIVE,
            WaitForSentStatus());

  // PageContains runs the RunLoop, so make sure nothing hairy happens.
  EXPECT_EQ(0, pending_status_count());
  EXPECT_TRUE(PageContains("NAME_NOT_RESOLVED"));
  EXPECT_EQ(0, pending_status_count());
}

// Test that pressing the stop button cancels loading the Link Doctor page.
// TODO(mmenke):  Add a test for the cross process navigation case.
// TODO(mmenke):  This test could flakily pass due to the timeout on downloading
//                the Link Doctor page.  Disable that timeout for browser tests.
IN_PROC_BROWSER_TEST_F(DnsProbeBrowserTest, LinkDoctorLoadStopped) {
  SetLinkDoctorDelayRequests(true);
  SetLinkDoctorBroken(true);
  SetMockDnsClientRules(MockDnsClientRule::TIMEOUT, MockDnsClientRule::TIMEOUT);

  NavigateToDnsError(1);

  EXPECT_EQ(chrome_common_net::DNS_PROBE_STARTED, WaitForSentStatus());
  StartDelayedProbes(1);
  EXPECT_EQ(chrome_common_net::DNS_PROBE_FINISHED_NO_INTERNET,
            WaitForSentStatus());

  EXPECT_EQ("", Title());
  EXPECT_EQ(0, pending_status_count());

  chrome::Stop(browser());
  WaitForDelayedRequestDestruction();

  // End up displaying a blank page.
  EXPECT_EQ("", Title());
}

// Test that pressing the stop button cancels the load of the Link Doctor error
// page, and receiving a probe result afterwards does not swap in a DNS error
// page.
IN_PROC_BROWSER_TEST_F(DnsProbeBrowserTest, LinkDoctorLoadStoppedSlowProbe) {
  SetLinkDoctorDelayRequests(true);
  SetLinkDoctorBroken(true);
  SetMockDnsClientRules(MockDnsClientRule::TIMEOUT, MockDnsClientRule::TIMEOUT);

  NavigateToDnsError(1);

  EXPECT_EQ(chrome_common_net::DNS_PROBE_STARTED, WaitForSentStatus());

  EXPECT_EQ("", Title());
  EXPECT_EQ(0, pending_status_count());

  chrome::Stop(browser());
  WaitForDelayedRequestDestruction();

  EXPECT_EQ("", Title());
  EXPECT_EQ(0, pending_status_count());

  StartDelayedProbes(1);
  EXPECT_EQ(chrome_common_net::DNS_PROBE_FINISHED_NO_INTERNET,
            WaitForSentStatus());

  EXPECT_EQ("", Title());
}

// Make sure probes don't run for subframe DNS errors.
IN_PROC_BROWSER_TEST_F(DnsProbeBrowserTest, NoProbeInSubframe) {
  SetLinkDoctorBroken(false);

  const FilePath::CharType kIframeDnsErrorHtmlName[] =
      FILE_PATH_LITERAL("iframe_dns_error.html");

  NavigateToURL(
      browser(),
      URLRequestMockHTTPJob::GetMockUrl(FilePath(kIframeDnsErrorHtmlName)));

  // By the time NavigateToURL returns, the browser will have seen the failed
  // provisional load.  If a probe was started (or considered but not run),
  // then the NetErrorTabHelper would have sent a NetErrorInfo message.  Thus,
  // if one hasn't been sent by now, the NetErrorTabHelper has not (and won't)
  // start a probe for this DNS error.
  EXPECT_EQ(0, pending_status_count());
}

// Make sure browser sends NOT_RUN properly when probes are disabled.
IN_PROC_BROWSER_TEST_F(DnsProbeBrowserTest, ProbesDisabled) {
  // Disable probes (And Link Doctor).
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kAlternateErrorPagesEnabled, false);

  SetLinkDoctorBroken(true);
  SetMockDnsClientRules(MockDnsClientRule::TIMEOUT, MockDnsClientRule::TIMEOUT);

  NavigateToDnsError(1);

  EXPECT_EQ(chrome_common_net::DNS_PROBE_NOT_RUN, WaitForSentStatus());

  // PageContains runs the RunLoop, so make sure nothing hairy happens.
  EXPECT_EQ(0, pending_status_count());
  EXPECT_TRUE(PageContains("NAME_NOT_RESOLVED"));
}

// Test the case that Link Doctor is disabled, but DNS probes are enabled.  This
// is the case with Chromium builds.
IN_PROC_BROWSER_TEST_F(DnsProbeBrowserTest, LinkDoctorDisabled) {
  // Disable Link Doctor.
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kAlternateErrorPagesEnabled, false);
  // Requests to the Link Doctor should work if any are made, so the test fails
  // if that happens unexpectedly.
  SetLinkDoctorBroken(false);
  // Normally disabling the LinkDoctor disables DNS probes, so force DNS probes
  // to be enabled.
  NetErrorTabHelper::set_state_for_testing(
      NetErrorTabHelper::TESTING_FORCE_ENABLED);

  SetMockDnsClientRules(MockDnsClientRule::FAIL, MockDnsClientRule::FAIL);

  // Just one commit and one sent status, since the Link Doctor is disabled.
  NavigateToDnsError(1);
  EXPECT_EQ(chrome_common_net::DNS_PROBE_STARTED, WaitForSentStatus());

  // PageContains runs the RunLoop, so make sure nothing hairy happens.
  EXPECT_EQ(0, pending_status_count());
  EXPECT_TRUE(PageContains("DNS_PROBE_STARTED"));
  EXPECT_EQ(0, pending_status_count());

  StartDelayedProbes(1);

  EXPECT_EQ(chrome_common_net::DNS_PROBE_FINISHED_INCONCLUSIVE,
            WaitForSentStatus());
  EXPECT_EQ(0, pending_status_count());
  EXPECT_TRUE(PageContains("NAME_NOT_RESOLVED"));
}

// Test incognito mode.  Link Doctor should be disabled, but DNS probes are
// still enabled.
IN_PROC_BROWSER_TEST_F(DnsProbeBrowserTest, Incognito) {
  // Requests to the Link Doctor should work if any are made, so the test will
  // fail if one is requested unexpectedly.
  SetLinkDoctorBroken(false);

  Browser* incognito = CreateIncognitoBrowser();
  SetActiveBrowser(incognito);

  SetMockDnsClientRules(MockDnsClientRule::FAIL, MockDnsClientRule::FAIL);

  // Just one commit and one sent status, since the Link Doctor is disabled.
  NavigateToDnsError(1);
  EXPECT_EQ(chrome_common_net::DNS_PROBE_STARTED, WaitForSentStatus());

  // PageContains runs the RunLoop, so make sure nothing hairy happens.
  EXPECT_EQ(0, pending_status_count());
  EXPECT_TRUE(PageContains("DNS_PROBE_STARTED"));
  EXPECT_EQ(0, pending_status_count());

  StartDelayedProbes(1);

  EXPECT_EQ(chrome_common_net::DNS_PROBE_FINISHED_INCONCLUSIVE,
            WaitForSentStatus());
  EXPECT_EQ(0, pending_status_count());
  EXPECT_TRUE(PageContains("NAME_NOT_RESOLVED"));
}

}  // namespace

}  // namespace chrome_browser_net
