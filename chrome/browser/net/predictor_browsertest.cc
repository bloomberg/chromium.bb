// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/json/json_string_value_serializer.h"
#include "base/macros.h"
#include "base/synchronization/lock.h"
#include "base/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/predictor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/test_utils.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/dns/host_resolver_proc.h"
#include "net/dns/mock_host_resolver.h"
#include "net/socket/stream_socket.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/embedded_test_server_connection_listener.h"
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

// Gets notified by the EmbeddedTestServer on incoming connections being
// accepted or read from, keeps track of them and exposes that info to
// the tests.
// A port being reused is currently considered an error.  If a test
// needs to verify multiple connections are opened in sequence, that will need
// to be changed.
class ConnectionListener
    : public net::test_server::EmbeddedTestServerConnectionListener {
 public:
  ConnectionListener() : task_runner_(base::ThreadTaskRunnerHandle::Get()) {}

  ~ConnectionListener() override {}

  // Get called from the EmbeddedTestServer thread to be notified that
  // a connection was accepted.
  void AcceptedSocket(const net::StreamSocket& connection) override {
    base::AutoLock lock(lock_);
    uint16_t socket = GetPort(connection);
    EXPECT_TRUE(sockets_.find(socket) == sockets_.end());

    sockets_[socket] = SOCKET_ACCEPTED;
    task_runner_->PostTask(FROM_HERE, accept_loop_.QuitClosure());
  }

  // Get called from the EmbeddedTestServer thread to be notified that
  // a connection was read from.
  void ReadFromSocket(const net::StreamSocket& connection) override {
    base::AutoLock lock(lock_);
    uint16_t socket = GetPort(connection);
    EXPECT_FALSE(sockets_.find(socket) == sockets_.end());

    sockets_[socket] = SOCKET_READ_FROM;
    task_runner_->PostTask(FROM_HERE, read_loop_.QuitClosure());
  }

  // Returns the number of sockets that were accepted by the server.
  size_t GetAcceptedSocketCount() const {
    base::AutoLock lock(lock_);
    return sockets_.size();
  }

  // Returns the number of sockets that were read from by the server.
  size_t GetReadSocketCount() const {
    base::AutoLock lock(lock_);
    size_t read_sockets = 0;
    for (const auto& socket : sockets_) {
      if (socket.second == SOCKET_READ_FROM)
        ++read_sockets;
    }
    return read_sockets;
  }

  void WaitUntilFirstConnectionAccepted() { accept_loop_.Run(); }

  void WaitUntilFirstConnectionRead() { read_loop_.Run(); }

 private:
  static uint16_t GetPort(const net::StreamSocket& connection) {
    // Get the remote port of the peer, since the local port will always be the
    // port the test server is listening on. This isn't strictly correct - it's
    // possible for multiple peers to connect with the same remote port but
    // different remote IPs - but the tests here assume that connections to the
    // test server (running on localhost) will always come from localhost, and
    // thus the peer port is all thats needed to distinguish two connections.
    // This also would be problematic if the OS reused ports, but that's not
    // something to worry about for these tests.
    net::IPEndPoint address;
    EXPECT_EQ(net::OK, connection.GetPeerAddress(&address));
    return address.port();
  }

  enum SocketStatus { SOCKET_ACCEPTED, SOCKET_READ_FROM };

  typedef base::hash_map<uint16_t, SocketStatus> SocketContainer;
  SocketContainer sockets_;

  base::RunLoop accept_loop_;
  base::RunLoop read_loop_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  mutable base::Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(ConnectionListener);
};

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
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    return std::find(requested_hostnames_.begin(),
                     requested_hostnames_.end(),
                     hostname) != requested_hostnames_.end();
  }

  void WaitUntilHostHasBeenRequested(const std::string& hostname) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
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
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    requested_hostnames_.push_back(hostname);
    if (is_waiting_for_hostname_ && waiting_for_hostname_ == hostname) {
      is_waiting_for_hostname_ = false;
      waiting_for_hostname_.clear();
      base::MessageLoop::current()->QuitWhenIdle();
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

  void SetUpOnMainThread() override {
    connection_listener_.reset(new ConnectionListener());
    embedded_test_server()->SetConnectionListener(connection_listener_.get());
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void TearDownOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
  }

  // Navigates to a data URL containing the given content, with a MIME type of
  // text/html.
  void NavigateToDataURLWithContent(const std::string& content) {
    std::string encoded_content;
    base::Base64Encode(content, &encoded_content);
    std::string data_uri_content = "data:text/html;base64," + encoded_content;
    ui_test_utils::NavigateToURL(browser(), GURL(data_uri_content));
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
  scoped_ptr<ConnectionListener> connection_listener_;

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
  int hostnames_requested_before_load = RequestedHostnameCount();
  ui_test_utils::NavigateToURL(
      browser(),
      GURL(embedded_test_server()->GetURL("/predictor/dns_prefetch.html")));
  WaitUntilHostHasBeenRequested(kChromiumHostname);
  ASSERT_FALSE(HasHostBeenRequested(kInvalidLongHostname));
  ASSERT_EQ(hostnames_requested_before_load + 1, RequestedHostnameCount());
}

// Tests that preconnect warms up a socket connection to a test server.
// Note: This test uses a data URI to serve the preconnect hint, to make sure
// that the network stack doesn't just re-use its connection to the test server.
IN_PROC_BROWSER_TEST_F(PredictorBrowserTest, PreconnectNonCORS) {
  GURL preconnect_url = embedded_test_server()->base_url();
  std::string preconnect_content =
      "<link rel=\"preconnect\" href=\"" + preconnect_url.spec() + "\">";
  NavigateToDataURLWithContent(preconnect_content);
  connection_listener_->WaitUntilFirstConnectionAccepted();
  EXPECT_EQ(1u, connection_listener_->GetAcceptedSocketCount());
  EXPECT_EQ(0u, connection_listener_->GetReadSocketCount());
}

// Tests that preconnect warms up a socket connection to a test server,
// and that that socket is later used when fetching a resource.
// Note: This test uses a data URI to serve the preconnect hint, to make sure
// that the network stack doesn't just re-use its connection to the test server.
IN_PROC_BROWSER_TEST_F(PredictorBrowserTest, PreconnectAndFetchNonCORS) {
  GURL preconnect_url = embedded_test_server()->base_url();
  // First navigation to content with a preconnect hint.
  std::string preconnect_content =
      "<link rel=\"preconnect\" href=\"" + preconnect_url.spec() + "\">";
  NavigateToDataURLWithContent(preconnect_content);
  connection_listener_->WaitUntilFirstConnectionAccepted();
  EXPECT_EQ(1u, connection_listener_->GetAcceptedSocketCount());
  EXPECT_EQ(0u, connection_listener_->GetReadSocketCount());

  // Second navigation to content with an img.
  std::string img_content =
      "<img src=\"" + preconnect_url.spec() + "test.gif\">";
  NavigateToDataURLWithContent(img_content);
  connection_listener_->WaitUntilFirstConnectionRead();
  EXPECT_EQ(1u, connection_listener_->GetAcceptedSocketCount());
  EXPECT_EQ(1u, connection_listener_->GetReadSocketCount());
}

// Tests that preconnect warms up a CORS connection to a test
// server, and that socket is later used when fetching a CORS resource.
// Note: This test uses a data URI to serve the preconnect hint, to make sure
// that the network stack doesn't just re-use its connection to the test server.
IN_PROC_BROWSER_TEST_F(PredictorBrowserTest, PreconnectAndFetchCORS) {
  GURL preconnect_url = embedded_test_server()->base_url();
  // First navigation to content with a preconnect hint.
  std::string preconnect_content = "<link rel=\"preconnect\" href=\"" +
                                   preconnect_url.spec() + "\" crossorigin>";
  NavigateToDataURLWithContent(preconnect_content);
  connection_listener_->WaitUntilFirstConnectionAccepted();
  EXPECT_EQ(1u, connection_listener_->GetAcceptedSocketCount());
  EXPECT_EQ(0u, connection_listener_->GetReadSocketCount());

  // Second navigation to content with a font.
  std::string font_content = "<script>var font = new FontFace('FontA', 'url(" +
                             preconnect_url.spec() +
                             "test.woff2)');font.load();</script>";
  NavigateToDataURLWithContent(font_content);
  connection_listener_->WaitUntilFirstConnectionRead();
  EXPECT_EQ(1u, connection_listener_->GetAcceptedSocketCount());
  EXPECT_EQ(1u, connection_listener_->GetReadSocketCount());
}

// Tests that preconnect warms up a non-CORS connection to a test
// server, but that socket is not used when fetching a CORS resource.
// Note: This test uses a data URI to serve the preconnect hint, to make sure
// that the network stack doesn't just re-use its connection to the test server.
IN_PROC_BROWSER_TEST_F(PredictorBrowserTest, PreconnectNonCORSAndFetchCORS) {
  GURL preconnect_url = embedded_test_server()->base_url();
  // First navigation to content with a preconnect hint.
  std::string preconnect_content =
      "<link rel=\"preconnect\" href=\"" + preconnect_url.spec() + "\">";
  NavigateToDataURLWithContent(preconnect_content);
  connection_listener_->WaitUntilFirstConnectionAccepted();
  EXPECT_EQ(1u, connection_listener_->GetAcceptedSocketCount());
  EXPECT_EQ(0u, connection_listener_->GetReadSocketCount());

  // Second navigation to content with a font.
  std::string font_content = "<script>var font = new FontFace('FontA', 'url(" +
                             preconnect_url.spec() +
                             "test.woff2)');font.load();</script>";
  NavigateToDataURLWithContent(font_content);
  connection_listener_->WaitUntilFirstConnectionRead();
  EXPECT_EQ(2u, connection_listener_->GetAcceptedSocketCount());
  EXPECT_EQ(1u, connection_listener_->GetReadSocketCount());
}

// Tests that preconnect warms up a CORS connection to a test server,
// but that socket is not used when fetching a non-CORS resource.
// Note: This test uses a data URI to serve the preconnect hint, to make sure
// that the network stack doesn't just re-use its connection to the test server.
IN_PROC_BROWSER_TEST_F(PredictorBrowserTest, PreconnectCORSAndFetchNonCORS) {
  GURL preconnect_url = embedded_test_server()->base_url();
  // First navigation to content with a preconnect hint.
  std::string preconnect_content = "<link rel=\"preconnect\" href=\"" +
                                   preconnect_url.spec() + "\" crossorigin>";
  NavigateToDataURLWithContent(preconnect_content);
  connection_listener_->WaitUntilFirstConnectionAccepted();
  EXPECT_EQ(1u, connection_listener_->GetAcceptedSocketCount());
  EXPECT_EQ(0u, connection_listener_->GetReadSocketCount());

  // Second navigation to content with an img.
  std::string img_content =
      "<img src=\"" + preconnect_url.spec() + "test.gif\">";
  NavigateToDataURLWithContent(img_content);
  connection_listener_->WaitUntilFirstConnectionRead();
  EXPECT_EQ(2u, connection_listener_->GetAcceptedSocketCount());
  EXPECT_EQ(1u, connection_listener_->GetReadSocketCount());
}

}  // namespace chrome_browser_net
