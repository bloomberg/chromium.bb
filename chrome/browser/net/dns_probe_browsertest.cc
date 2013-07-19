// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/net/dns_probe_test_util.h"
#include "chrome/browser/net/net_error_tab_helper.h"
#include "chrome/browser/net/url_request_mock_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/net/net_error_info.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
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

class BreakableLinkDoctorProtocolHandler
    : public URLRequestJobFactory::ProtocolHandler {
 public:
  explicit BreakableLinkDoctorProtocolHandler(
      const FilePath& mock_link_doctor_file_path)
      : mock_link_doctor_file_path_(mock_link_doctor_file_path),
        net_error_(net::OK) {}

  virtual ~BreakableLinkDoctorProtocolHandler() {}

  virtual URLRequestJob* MaybeCreateJob(
      URLRequest* request, NetworkDelegate* network_delegate) const OVERRIDE {
    if (net_error_ != net::OK) {
      return new URLRequestFailedJob(request, network_delegate, net_error_);
    } else {
      return new URLRequestMockHTTPJob(
          request, network_delegate, mock_link_doctor_file_path_);
    }
  }

  void set_net_error(int net_error) { net_error_ = net_error; }

 private:
  const FilePath mock_link_doctor_file_path_;
  int net_error_;
};

class DnsProbeBrowserTestIOThreadHelper {
 public:
  DnsProbeBrowserTestIOThreadHelper();

  void SetUpOnIOThread(IOThread* io_thread);
  void CleanUpOnIOThreadAndDeleteHelper();

  void SetMockDnsClientRules(MockDnsClientRule::Result system_good_result,
                             MockDnsClientRule::Result public_good_result);
  void SetLinkDoctorNetError(int link_doctor_net_error);
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

  virtual void SetUpOnMainThread() OVERRIDE;
  virtual void CleanUpOnMainThread() OVERRIDE;

 protected:
  void SetLinkDoctorBroken(bool broken);
  void SetMockDnsClientRules(MockDnsClientRule::Result system_result,
                             MockDnsClientRule::Result public_result);
  void NavigateToDnsError();
  void NavigateToOtherError();

  void StartDelayedProbes(int expected_delayed_probe_count);
  DnsProbeStatus WaitForSentStatus();
  int pending_status_count() const { return dns_probe_status_queue_.size(); }

  std::string Title();
  bool PageContains(const std::string& expected);

 private:
  void OnDnsProbeStatusSent(DnsProbeStatus dns_probe_status);

  DnsProbeBrowserTestIOThreadHelper* helper_;

  bool awaiting_dns_probe_status_;
  // Queue of statuses received but not yet consumed by WaitForSentStatus().
  std::list<DnsProbeStatus> dns_probe_status_queue_;
};

DnsProbeBrowserTest::DnsProbeBrowserTest()
    : helper_(new DnsProbeBrowserTestIOThreadHelper()),
      awaiting_dns_probe_status_(false) {
}

void DnsProbeBrowserTest::SetUpOnMainThread() {
  NetErrorTabHelper::set_state_for_testing(
      NetErrorTabHelper::TESTING_FORCE_ENABLED);

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      Bind(&DnsProbeBrowserTestIOThreadHelper::SetUpOnIOThread,
           Unretained(helper_),
           g_browser_process->io_thread()));

  NetErrorTabHelper* tab_helper = NetErrorTabHelper::FromWebContents(
      browser()->tab_strip_model()->GetActiveWebContents());
  tab_helper->set_dns_probe_status_snoop_callback_for_testing(Bind(
      &DnsProbeBrowserTest::OnDnsProbeStatusSent,
      Unretained(this)));
}

void DnsProbeBrowserTest::CleanUpOnMainThread() {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      Bind(&DnsProbeBrowserTestIOThreadHelper::CleanUpOnIOThreadAndDeleteHelper,
           Unretained(helper_)));

  NetErrorTabHelper::set_state_for_testing(
      NetErrorTabHelper::TESTING_DEFAULT);
}

void DnsProbeBrowserTest::SetLinkDoctorBroken(bool broken) {
  int net_error = broken ? net::ERR_NAME_NOT_RESOLVED : net::OK;

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      Bind(&DnsProbeBrowserTestIOThreadHelper::SetLinkDoctorNetError,
           Unretained(helper_),
           net_error));
}

// These two functions wait for two navigations because Link Doctor loads two
// pages: a blank page, so the user stops seeing the previous page, and then
// either the Link Doctor page or a regular error page.  We want to wait for
// the error page, so we wait for both loads to finish.

void DnsProbeBrowserTest::NavigateToDnsError() {
  NavigateToURLBlockUntilNavigationsComplete(
      browser(),
      URLRequestFailedJob::GetMockHttpUrl(net::ERR_NAME_NOT_RESOLVED),
      2);
}

void DnsProbeBrowserTest::NavigateToOtherError() {
  NavigateToURLBlockUntilNavigationsComplete(
      browser(),
      URLRequestFailedJob::GetMockHttpUrl(net::ERR_CONNECTION_REFUSED),
      2);
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
      browser()->tab_strip_model()->GetActiveWebContents();

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
      browser()->tab_strip_model()->GetActiveWebContents(),
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

  NavigateToOtherError();
  EXPECT_EQ("Mock Link Doctor", Title());

  EXPECT_EQ(0, pending_status_count());
}

// Make sure probes don't break non-DNS error pages when Link Doctor doesn't
// load.
IN_PROC_BROWSER_TEST_F(DnsProbeBrowserTest, OtherErrorWithBrokenLinkDoctor) {
  SetLinkDoctorBroken(true);

  NavigateToOtherError();
  EXPECT_TRUE(PageContains("CONNECTION_REFUSED"));

  EXPECT_EQ(0, pending_status_count());
}

// Make sure probes don't break DNS error pages when Link doctor loads.
IN_PROC_BROWSER_TEST_F(DnsProbeBrowserTest,
    NxdomainProbeResultWithWorkingLinkDoctor) {
  SetLinkDoctorBroken(false);
  SetMockDnsClientRules(MockDnsClientRule::OK, MockDnsClientRule::OK);

  NavigateToDnsError();
  EXPECT_EQ("Mock Link Doctor", Title());

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

// Make sure probes update DNS error page properly when they're supposed to.
IN_PROC_BROWSER_TEST_F(DnsProbeBrowserTest,
    NoInternetProbeResultWithBrokenLinkDoctor) {
  SetLinkDoctorBroken(true);
  SetMockDnsClientRules(MockDnsClientRule::TIMEOUT,
                        MockDnsClientRule::TIMEOUT);

  NavigateToDnsError();

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
  EXPECT_EQ(0, pending_status_count());
}

// Double-check to make sure sync failures don't explode.
IN_PROC_BROWSER_TEST_F(DnsProbeBrowserTest, SyncFailureWithBrokenLinkDoctor) {
  SetLinkDoctorBroken(true);
  SetMockDnsClientRules(MockDnsClientRule::FAIL, MockDnsClientRule::FAIL);

  NavigateToDnsError();

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
  NetErrorTabHelper::set_state_for_testing(
      NetErrorTabHelper::TESTING_FORCE_DISABLED);

  SetLinkDoctorBroken(true);
  SetMockDnsClientRules(MockDnsClientRule::TIMEOUT,
                        MockDnsClientRule::TIMEOUT);

  NavigateToDnsError();

  EXPECT_EQ(chrome_common_net::DNS_PROBE_NOT_RUN, WaitForSentStatus());
  EXPECT_EQ(chrome_common_net::DNS_PROBE_NOT_RUN, WaitForSentStatus());

  // PageContains runs the RunLoop, so make sure nothing hairy happens.
  EXPECT_EQ(0, pending_status_count());
  EXPECT_TRUE(PageContains("NAME_NOT_RESOLVED"));
  EXPECT_EQ(0, pending_status_count());
}

}  // namespace

}  // namespace chrome_browser_net
