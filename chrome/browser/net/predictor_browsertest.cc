// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base64.h"
#include "base/command_line.h"
#include "base/json/json_string_value_serializer.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/chrome_net_log.h"
#include "chrome/browser/net/predictor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/test_utils.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/dns/host_resolver_proc.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"

using content::BrowserThread;
using testing::HasSubstr;

namespace {

const char kBlinkPreconnectFeature[] = "LinkPreconnect";
const char kChromiumHostname[] = "chromium.org";
const char kInvalidLongHostname[] = "illegally-long-hostname-over-255-"
    "characters-should-not-send-an-ipc-message-to-the-browser-"
    "0000000000000000000000000000000000000000000000000000000000000000000000000"
    "0000000000000000000000000000000000000000000000000000000000000000000000000"
    "000000000000000000000000000000000000000000000000000000.org";

// Records a history of all hostnames for which resolving has been requested,
// and immediately fails the resolution requests themselves.
class HostResolutionRequestRecorder : public net::HostResolverProc {
 public:
  HostResolutionRequestRecorder()
      : HostResolverProc(NULL),
        is_waiting_for_hostname_(false) {
  }

  int Resolve(const std::string& host,
              net::AddressFamily address_family,
              net::HostResolverFlags host_resolver_flags,
              net::AddressList* addrlist,
              int* os_error) override {
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&HostResolutionRequestRecorder::AddToHistory,
                   base::Unretained(this),
                   host));
    return net::ERR_NAME_NOT_RESOLVED;
  }

  int RequestedHostnameCount() const {
    return requested_hostnames_.size();
  }

  bool HasHostBeenRequested(const std::string& hostname) const {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    return std::find(requested_hostnames_.begin(),
                     requested_hostnames_.end(),
                     hostname) != requested_hostnames_.end();
  }

  void WaitUntilHostHasBeenRequested(const std::string& hostname) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    DCHECK(!is_waiting_for_hostname_);
    if (HasHostBeenRequested(hostname))
      return;
    waiting_for_hostname_ = hostname;
    is_waiting_for_hostname_ = true;
    content::RunMessageLoop();
  }

 private:
  ~HostResolutionRequestRecorder() override {}

  void AddToHistory(const std::string& hostname) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    requested_hostnames_.push_back(hostname);
    if (is_waiting_for_hostname_ && waiting_for_hostname_ == hostname) {
      is_waiting_for_hostname_ = false;
      waiting_for_hostname_.clear();
      base::MessageLoop::current()->Quit();
    }
  }

  // The hostname which WaitUntilHostHasBeenRequested is currently waiting for
  // to be requested.
  std::string waiting_for_hostname_;

  // Whether WaitUntilHostHasBeenRequested is waiting for a hostname to be
  // requested and thus is running a nested message loop.
  bool is_waiting_for_hostname_;

  // A list of hostnames for which resolution has already been requested. Only
  // to be accessed from the UI thread.
  std::vector<std::string> requested_hostnames_;

  DISALLOW_COPY_AND_ASSIGN(HostResolutionRequestRecorder);
};

// Watches the NetLog event stream for a connect event to the provided
// host:port pair.
class ConnectNetLogObserver : public net::NetLog::ThreadSafeObserver {
 public:
  explicit ConnectNetLogObserver(const std::string& host_port_pair)
      : host_port_pair_(host_port_pair) {
  }

  ~ConnectNetLogObserver() override {
  }

  void Attach() {
    g_browser_process->net_log()->AddThreadSafeObserver(
        this, net::NetLog::LOG_ALL_BUT_BYTES);
  }

  void Detach() {
    if (net_log())
      net_log()->RemoveThreadSafeObserver(this);
  }

  void WaitForConnect() {
    run_loop_.Run();
  }

 private:
  void OnAddEntry(const net::NetLog::Entry& entry) override {
    scoped_ptr<base::Value> param_value(entry.ParametersToValue());
    base::DictionaryValue* param_dict = NULL;
    std::string group_name;

    if (entry.source().type == net::NetLog::SOURCE_CONNECT_JOB &&
        param_value.get() != NULL &&
        param_value->GetAsDictionary(&param_dict) &&
        param_dict != NULL &&
        param_dict->GetString("group_name", &group_name) &&
        host_port_pair_ == group_name) {
      run_loop_.Quit();
    }
  }

  base::RunLoop run_loop_;
  const std::string host_port_pair_;
};

}  // namespace

namespace chrome_browser_net {

class PredictorBrowserTest : public InProcessBrowserTest {
 public:
  PredictorBrowserTest()
      : startup_url_("http://host1:1"),
        referring_url_("http://host2:1"),
        target_url_("http://host3:1"),
        host_resolution_request_recorder_(new HostResolutionRequestRecorder) {
  }

 protected:
  void SetUpInProcessBrowserTestFixture() override {
    scoped_host_resolver_proc_.reset(new net::ScopedDefaultHostResolverProc(
        host_resolution_request_recorder_.get()));
    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
    command_line->AppendSwitchASCII(
        switches::kEnableBlinkFeatures, kBlinkPreconnectFeature);
  }

  void TearDownInProcessBrowserTestFixture() override {
    InProcessBrowserTest::TearDownInProcessBrowserTestFixture();
    scoped_host_resolver_proc_.reset();
  }

  void LearnAboutInitialNavigation(const GURL& url) {
    Predictor* predictor = browser()->profile()->GetNetworkPredictor();
    BrowserThread::PostTask(BrowserThread::IO,
                            FROM_HERE,
                            base::Bind(&Predictor::LearnAboutInitialNavigation,
                                       base::Unretained(predictor),
                                       url));
    content::RunAllPendingInMessageLoop(BrowserThread::IO);
  }

  void LearnFromNavigation(const GURL& referring_url, const GURL& target_url) {
    Predictor* predictor = browser()->profile()->GetNetworkPredictor();
    BrowserThread::PostTask(BrowserThread::IO,
                            FROM_HERE,
                            base::Bind(&Predictor::LearnFromNavigation,
                                       base::Unretained(predictor),
                                       referring_url,
                                       target_url));
    content::RunAllPendingInMessageLoop(BrowserThread::IO);
  }

  void PrepareFrameSubresources(const GURL& url) {
    Predictor* predictor = browser()->profile()->GetNetworkPredictor();
    predictor->PredictFrameSubresources(url, GURL());
  }

  void GetListFromPrefsAsString(const char* list_path,
                                std::string* value_as_string) const {
    PrefService* prefs = browser()->profile()->GetPrefs();
    const base::ListValue* list_value = prefs->GetList(list_path);
    JSONStringValueSerializer serializer(value_as_string);
    serializer.Serialize(*list_value);
  }

  bool HasHostBeenRequested(const std::string& hostname) const {
    return host_resolution_request_recorder_->HasHostBeenRequested(hostname);
  }

  void WaitUntilHostHasBeenRequested(const std::string& hostname) {
    host_resolution_request_recorder_->WaitUntilHostHasBeenRequested(hostname);
  }

  int RequestedHostnameCount() const {
    return host_resolution_request_recorder_->RequestedHostnameCount();
  }

  const GURL startup_url_;
  const GURL referring_url_;
  const GURL target_url_;

 private:
  scoped_refptr<HostResolutionRequestRecorder>
      host_resolution_request_recorder_;
  scoped_ptr<net::ScopedDefaultHostResolverProc> scoped_host_resolver_proc_;
};

IN_PROC_BROWSER_TEST_F(PredictorBrowserTest, PRE_ShutdownStartupCycle) {
  // Prepare state that will be serialized on this shut-down and read on next
  // start-up.
  LearnAboutInitialNavigation(startup_url_);
  LearnFromNavigation(referring_url_, target_url_);
}

IN_PROC_BROWSER_TEST_F(PredictorBrowserTest, ShutdownStartupCycle) {
  // Make sure that the Preferences file is actually wiped of all DNS prefetch
  // related data after start-up.
  std::string cleared_startup_list;
  std::string cleared_referral_list;
  GetListFromPrefsAsString(prefs::kDnsPrefetchingStartupList,
                           &cleared_startup_list);
  GetListFromPrefsAsString(prefs::kDnsPrefetchingHostReferralList,
                           &cleared_referral_list);

  EXPECT_THAT(cleared_startup_list, Not(HasSubstr(startup_url_.host())));
  EXPECT_THAT(cleared_referral_list, Not(HasSubstr(referring_url_.host())));
  EXPECT_THAT(cleared_referral_list, Not(HasSubstr(target_url_.host())));

  // But also make sure this data has been first loaded into the Predictor, by
  // inspecting that the Predictor starts making the expected hostname requests.
  PrepareFrameSubresources(referring_url_);
  WaitUntilHostHasBeenRequested(startup_url_.host());
  WaitUntilHostHasBeenRequested(target_url_.host());
}

// Flaky on Windows: http://crbug.com/469120
#if defined(OS_WIN)
#define MAYBE_DnsPrefetch DISABLED_DnsPrefetch
#else
#define MAYBE_DnsPrefetch DnsPrefetch
#endif
IN_PROC_BROWSER_TEST_F(PredictorBrowserTest, MAYBE_DnsPrefetch) {
  ASSERT_TRUE(test_server()->Start());
  int hostnames_requested_before_load = RequestedHostnameCount();
  ui_test_utils::NavigateToURL(
      browser(),
      GURL(test_server()->GetURL("files/predictor/dns_prefetch.html")));
  WaitUntilHostHasBeenRequested(kChromiumHostname);
  ASSERT_FALSE(HasHostBeenRequested(kInvalidLongHostname));
  ASSERT_EQ(hostnames_requested_before_load + 1, RequestedHostnameCount());
}

IN_PROC_BROWSER_TEST_F(PredictorBrowserTest, Preconnect) {
  ASSERT_TRUE(test_server()->Start());

  // Create a HTML preconnect reference to the local server in the form
  // <link rel="preconnect" href="http://test-server/">
  // and navigate to it as a data URI.
  GURL preconnect_url = test_server()->GetURL("");
  std::string preconnect_content =
      "<link rel=\"preconnect\" href=\"" + preconnect_url.spec() + "\">";
  std::string encoded;
  base::Base64Encode(preconnect_content, &encoded);
  std::string data_uri = "data:text/html;base64," + encoded;

  net::HostPortPair host_port_pair = net::HostPortPair::FromURL(preconnect_url);
  ConnectNetLogObserver net_log_observer(host_port_pair.ToString());
  net_log_observer.Attach();

  ui_test_utils::NavigateToURL(browser(), GURL(data_uri));

  net_log_observer.WaitForConnect();
  net_log_observer.Detach();
}

}  // namespace chrome_browser_net

