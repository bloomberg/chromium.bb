// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <memory>

#include "base/base64.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/json/json_string_value_serializer.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/predictor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/dns/host_resolver_proc.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_transaction_factory.h"
#include "net/socket/stream_socket.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/embedded_test_server_connection_listener.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/url_request/url_request_failed_job.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_interceptor.h"
#include "net/url_request/url_request_test_job.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

using content::BrowserThread;
using testing::HasSubstr;

namespace {

net::URLRequestJob* CreateEmptyBodyRequestJob(net::URLRequest* request,
                                              net::NetworkDelegate* delegate) {
  const char kPlainTextHeaders[] =
      "HTTP/1.1 200 OK\n"
      "Content-Type: text/plain\n"
      "Access-Control-Allow-Origin: *\n"
      "\n";
  return new net::URLRequestTestJob(request, delegate, kPlainTextHeaders, "",
                                    true);
}

// Override the test server to redirect requests matching some path. This is
// used because the predictor only learns simple redirects with a path of "/"
std::unique_ptr<net::test_server::HttpResponse> RedirectForPathHandler(
    const std::string& path,
    const GURL& redirect_url,
    const net::test_server::HttpRequest& request) {
  if (request.GetURL().path() != path)
    return nullptr;
  std::unique_ptr<net::test_server::BasicHttpResponse> response(
      new net::test_server::BasicHttpResponse);
  response->set_code(net::HTTP_MOVED_PERMANENTLY);
  response->AddCustomHeader("Location", redirect_url.spec());
  return std::move(response);
}

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
  ConnectionListener()
      : task_runner_(base::ThreadTaskRunnerHandle::Get()),
        num_accepted_connections_needed_(0),
        num_accepted_connections_loop_(nullptr) {}

  ~ConnectionListener() override {}

  // Get called from the EmbeddedTestServer thread to be notified that
  // a connection was accepted.
  void AcceptedSocket(const net::StreamSocket& connection) override {
    base::AutoLock lock(lock_);
    uint16_t socket = GetPort(connection);
    EXPECT_TRUE(sockets_.find(socket) == sockets_.end());

    sockets_[socket] = SOCKET_ACCEPTED;
    task_runner_->PostTask(FROM_HERE, accept_loop_.QuitClosure());
    CheckAccepted();
  }

  // Get called from the EmbeddedTestServer thread to be notified that
  // a connection was read from.
  void ReadFromSocket(const net::StreamSocket& connection, int rv) override {
    // Don't log a read if no data was transferred. This case often happens if
    // the sockets of the test server are being flushed and disconnected.
    if (rv <= 0)
      return;
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

  // The UI thread will wait for exactly |n| items in |sockets_|. |n| must be
  // greater than 0.
  void WaitForAcceptedConnectionsOnUI(size_t num_connections) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    DCHECK(!num_accepted_connections_loop_);
    DCHECK_GT(num_connections, 0u);
    base::RunLoop run_loop;
    {
      base::AutoLock lock(lock_);
      EXPECT_GE(num_connections, sockets_.size());
      num_accepted_connections_loop_ = &run_loop;
      num_accepted_connections_needed_ = num_connections;
      CheckAccepted();
    }
    // Note that the previous call to CheckAccepted can quit this run loop
    // before this call, which will make this call a no-op.
    run_loop.Run();

    // Grab the mutex again and make sure that the number of accepted sockets is
    // indeed |num_connections|.
    base::AutoLock lock(lock_);
    EXPECT_EQ(num_connections, sockets_.size());
  }

  void CheckAcceptedLocked() {
    base::AutoLock lock(lock_);
    CheckAccepted();
  }

  // Helper function to stop the waiting for sockets to be accepted for
  // WaitForAcceptedConnectionsOnUI. |num_accepted_connections_loop_| spins
  // until |num_accepted_connections_needed_| sockets are accepted by the test
  // server. The values will be null/0 if the loop is not running.
  void CheckAccepted() {
    lock_.AssertAcquired();
    // |num_accepted_connections_loop_| null implies
    // |num_accepted_connections_needed_| == 0.
    DCHECK(num_accepted_connections_loop_ ||
           num_accepted_connections_needed_ == 0);
    if (!num_accepted_connections_loop_ ||
        num_accepted_connections_needed_ != sockets_.size()) {
      return;
    }

    task_runner_->PostTask(FROM_HERE,
                           num_accepted_connections_loop_->QuitClosure());
    num_accepted_connections_needed_ = 0;
    num_accepted_connections_loop_ = nullptr;
  }

  void ResetCounts() {
    base::AutoLock lock(lock_);
    sockets_.clear();
  }

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

  base::RunLoop accept_loop_;
  base::RunLoop read_loop_;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // This lock protects all the members below, which each are used on both the
  // IO and UI thread. Members declared after the lock are protected by it.
  mutable base::Lock lock_;
  typedef base::hash_map<uint16_t, SocketStatus> SocketContainer;
  SocketContainer sockets_;

  // If |num_accepted_connections_needed_| is non zero, then the object is
  // waiting for |num_accepted_connections_needed_| sockets to be accepted
  // before quitting the |num_accepted_connections_loop_|.
  size_t num_accepted_connections_needed_;
  base::RunLoop* num_accepted_connections_loop_;

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

// This class intercepts URLRequests and responds with the URLRequestJob*
// callback provided by the constructor. Note that the port of the URL must
// match the port given in the constructor.
class MatchingPortRequestInterceptor : public net::URLRequestInterceptor {
 public:
  typedef base::Callback<net::URLRequestJob*(net::URLRequest*,
                                             net::NetworkDelegate*)>
      CreateJobCallback;

  MatchingPortRequestInterceptor(
      int port,
      base::Callback<net::URLRequestJob*(net::URLRequest*,
                                         net::NetworkDelegate*)>
          create_job_callback)
      : create_job_callback_(create_job_callback), port_(port) {}
  ~MatchingPortRequestInterceptor() override {}

 private:
  net::URLRequestJob* MaybeInterceptRequest(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override {
    if (request->url().EffectiveIntPort() != port_)
      return nullptr;
    return create_job_callback_.Run(request, network_delegate);
  }

  const CreateJobCallback create_job_callback_;
  const int port_;

  DISALLOW_COPY_AND_ASSIGN(MatchingPortRequestInterceptor);
};

// This class is owned by the test harness, and listens to Predictor events. It
// takes as input the source host and cross site host used by the test harness,
// and asserts that only valid preconnects and learning can occur between the
// two.
class CrossSitePredictorObserver
    : public chrome_browser_net::PredictorObserver {
 public:
  CrossSitePredictorObserver(const GURL& source_host,
                             const GURL& cross_site_host)
      : source_host_(source_host),
        cross_site_host_(cross_site_host),
        cross_site_learned_(0),
        cross_site_preconnected_(0),
        same_site_preconnected_(0) {}

  void OnPreconnectUrl(
      const GURL& original_url,
      const GURL& first_party_for_cookies,
      chrome_browser_net::UrlInfo::ResolutionMotivation motivation,
      int count) override {
    base::AutoLock lock(lock_);
    if (original_url == cross_site_host_) {
      cross_site_preconnected_ = std::max(cross_site_preconnected_, count);
    } else if (original_url == source_host_) {
      same_site_preconnected_ = std::max(same_site_preconnected_, count);
    } else {
      ADD_FAILURE() << "Preconnected " << original_url
                    << " when should only be preconnecting the source host: "
                    << source_host_
                    << " or the cross site host: " << cross_site_host_;
    }
  }

  void OnLearnFromNavigation(const GURL& referring_url,
                             const GURL& target_url) override {
    base::AutoLock lock(lock_);
    // There are three possibilities:
    //     source => target
    //     source => source
    //     target => target
    if (referring_url == source_host_ && target_url == cross_site_host_) {
      cross_site_learned_++;
    } else if (referring_url == source_host_ && target_url == source_host_) {
      // Same site learned. Branch retained for clarity.
    } else if (!(referring_url == cross_site_host_ &&
                 target_url == cross_site_host_)) {
      ADD_FAILURE() << "Learned " << referring_url << " => " << target_url
                    << " when should only be learning the source host: "
                    << source_host_
                    << " or the cross site host: " << cross_site_host_;
    }
  }

  void ResetCounts() {
    base::AutoLock lock(lock_);
    cross_site_learned_ = 0;
    cross_site_preconnected_ = 0;
    same_site_preconnected_ = 0;
  }

  int CrossSiteLearned() {
    base::AutoLock lock(lock_);
    return cross_site_learned_;
  }

  int CrossSitePreconnected() {
    base::AutoLock lock(lock_);
    return cross_site_preconnected_;
  }

  int SameSitePreconnected() {
    base::AutoLock lock(lock_);
    return same_site_preconnected_;
  }

 private:
  const GURL source_host_;
  const GURL cross_site_host_;

  // Protects all following members. They are read and updated from different
  // threads.
  base::Lock lock_;

  int cross_site_learned_;
  int cross_site_preconnected_;
  int same_site_preconnected_;

  DISALLOW_COPY_AND_ASSIGN(CrossSitePredictorObserver);
};

}  // namespace

namespace chrome_browser_net {

class PredictorBrowserTest : public InProcessBrowserTest {
 public:
  PredictorBrowserTest()
      : startup_url_("http://host1:1"),
        referring_url_("http://host2:1"),
        target_url_("http://host3:1"),
        host_resolution_request_recorder_(new HostResolutionRequestRecorder),
        cross_site_test_server_(new net::EmbeddedTestServer()) {}

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
    command_line->AppendSwitchASCII(switches::kEnableFeatures,
                                    "PreconnectMore");
  }

  void SetUpOnMainThread() override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    task_runner_ = base::ThreadTaskRunnerHandle::Get();
    cross_site_test_server_->ServeFilesFromSourceDirectory("chrome/test/data/");

    connection_listener_.reset(new ConnectionListener());
    cross_site_connection_listener_.reset(new ConnectionListener());
    embedded_test_server()->SetConnectionListener(connection_listener_.get());
    cross_site_test_server_->SetConnectionListener(
        cross_site_connection_listener_.get());
    ASSERT_TRUE(embedded_test_server()->Start());
    ASSERT_TRUE(cross_site_test_server_->Start());

    embedded_test_server()->RegisterRequestHandler(
        base::Bind(&RedirectForPathHandler, "/",
                   cross_site_test_server()->GetURL("/title1.html")));

    predictor()->SetPreconnectEnabledForTest(true);
    InstallPredictorObserver(embedded_test_server()->base_url(),
                             cross_site_test_server()->base_url());
    StartInterceptingCrossSiteOnUI();
  }

  // Intercepts all requests to the specified host and returns a response with
  // an empty body. Needed to prevent requests from actually going to the test
  // server, to avoid any races related to socket accounting. Note, the
  // interceptor also looks at the port, to differentiate between the
  // two test servers.
  static void StartInterceptingHost(const GURL& url) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    net::URLRequestFilter::GetInstance()->AddHostnameInterceptor(
        url.scheme(), url.host(),
        base::WrapUnique(new MatchingPortRequestInterceptor(
            url.EffectiveIntPort(), base::Bind(&CreateEmptyBodyRequestJob))));
  }

  static void StopInterceptingHost(const GURL& url) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    net::URLRequestFilter::GetInstance()->RemoveHostnameHandler(url.scheme(),
                                                                url.host());
  }

  void StartInterceptingCrossSiteOnUI() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    content::BrowserThread::PostTask(
        content::BrowserThread::IO, FROM_HERE,
        base::Bind(&PredictorBrowserTest::StartInterceptingHost,
                   cross_site_test_server()->base_url()));
  }

  void StopInterceptingCrossSiteOnUI() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    content::BrowserThread::PostTask(
        content::BrowserThread::IO, FROM_HERE,
        base::Bind(&PredictorBrowserTest::StopInterceptingHost,
                   cross_site_test_server()->base_url()));
  }

  void TearDownOnMainThread() override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    ASSERT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
  }

  // Navigates to a data URL containing the given content, with a MIME type of
  // text/html.
  void NavigateToDataURLWithContent(const std::string& content) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
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
    BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                            base::Bind(&Predictor::LearnAboutInitialNavigation,
                                       base::Unretained(predictor()), url));
    content::RunAllPendingInMessageLoop(BrowserThread::IO);
  }

  void LearnFromNavigation(const GURL& referring_url, const GURL& target_url) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&Predictor::LearnFromNavigation,
                   base::Unretained(predictor()), referring_url, target_url));
    content::RunAllPendingInMessageLoop(BrowserThread::IO);
  }

  void PrepareFrameSubresources(const GURL& url) {
    predictor()->PredictFrameSubresources(url, GURL());
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

  net::EmbeddedTestServer* cross_site_test_server() {
    return cross_site_test_server_.get();
  }

  Predictor* predictor() { return browser()->profile()->GetNetworkPredictor(); }

  void InstallPredictorObserver(const GURL& source_host,
                                const GURL& cross_site_host) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    observer_.reset(
        new CrossSitePredictorObserver(source_host, cross_site_host));
    base::RunLoop run_loop;
    BrowserThread::PostTaskAndReply(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&PredictorBrowserTest::InstallPredictorObserverOnIOThread,
                   base::Unretained(this), base::Unretained(predictor())),
        run_loop.QuitClosure());
    run_loop.Run();
  }

  void InstallPredictorObserverOnIOThread(
      chrome_browser_net::Predictor* predictor) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    predictor->SetObserver(observer_.get());
  }

  // Note: For many of the tests to get to a "clean slate" mid run, they must
  // flush sockets on both the client and server.
  void FlushClientSockets() {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    predictor()
        ->url_request_context_getter_for_test()
        ->GetURLRequestContext()
        ->http_transaction_factory()
        ->GetSession()
        ->CloseAllConnections();
  }

  void FlushClientSocketsOnUIThread() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    base::RunLoop run_loop;
    BrowserThread::PostTaskAndReply(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&PredictorBrowserTest::FlushClientSockets,
                   base::Unretained(this)),
        run_loop.QuitClosure());
    run_loop.Run();
  }

  void FlushServerSocketsOnUIThread(net::EmbeddedTestServer* test_server) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    EXPECT_TRUE(test_server->FlushAllSocketsAndConnectionsOnUIThread());
  }

  CrossSitePredictorObserver* observer() { return observer_.get(); }

  // Navigate to an html file on embedded_test_server and tell it to request
  // |num_cors| resources from the cross_site_test_server. It then waits for
  // those requests to complete. Note that "cors" here means using cors-mode in
  // correspondence with the fetch spec.
  void NavigateToCrossSiteHtmlUrl(int num_cors, const char* file_suffix) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    const GURL& base_url = cross_site_test_server()->base_url();
    std::string path = base::StringPrintf(
        "/predictor/"
        "predictor_cross_site%s.html?subresourceHost=%s&"
        "numCORSResources=%d",
        file_suffix, base_url.spec().c_str(), num_cors);
    ui_test_utils::NavigateToURL(browser(),
                                 embedded_test_server()->GetURL(path));
    bool result = false;
    EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
        browser()->tab_strip_model()->GetActiveWebContents(),
        "startFetchesAndWaitForReply()", &result));
    EXPECT_TRUE(result);
  }

  const GURL startup_url_;
  const GURL referring_url_;
  const GURL target_url_;
  std::unique_ptr<ConnectionListener> connection_listener_;
  std::unique_ptr<ConnectionListener> cross_site_connection_listener_;

 private:
  scoped_refptr<HostResolutionRequestRecorder>
      host_resolution_request_recorder_;
  std::unique_ptr<net::ScopedDefaultHostResolverProc>
      scoped_host_resolver_proc_;
  std::unique_ptr<net::EmbeddedTestServer> cross_site_test_server_;
  std::unique_ptr<CrossSitePredictorObserver> observer_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

IN_PROC_BROWSER_TEST_F(PredictorBrowserTest, SimplePreconnectOne) {
  predictor()->PreconnectUrl(
      embedded_test_server()->base_url(), GURL(),
      UrlInfo::ResolutionMotivation::EARLY_LOAD_MOTIVATED,
      false /* allow credentials */, 1);
  connection_listener_->WaitForAcceptedConnectionsOnUI(1u);
}

IN_PROC_BROWSER_TEST_F(PredictorBrowserTest, SimplePreconnectTwo) {
  predictor()->PreconnectUrl(
      embedded_test_server()->base_url(), GURL(),
      UrlInfo::ResolutionMotivation::EARLY_LOAD_MOTIVATED,
      false /* allow credentials */, 2);
  connection_listener_->WaitForAcceptedConnectionsOnUI(2u);
}

IN_PROC_BROWSER_TEST_F(PredictorBrowserTest, SimplePreconnectFour) {
  predictor()->PreconnectUrl(
      embedded_test_server()->base_url(), GURL(),
      UrlInfo::ResolutionMotivation::EARLY_LOAD_MOTIVATED,
      false /* allow credentials */, 4);
  connection_listener_->WaitForAcceptedConnectionsOnUI(4u);
}

// Test the html test harness used to initiate cross site fetches. These
// initiate cross site subresource requests to the cross site test server.
// Inspect the predictor's internal state to make sure that they are properly
// logged.
IN_PROC_BROWSER_TEST_F(PredictorBrowserTest, CrossSiteUseOneSocket) {
  StopInterceptingCrossSiteOnUI();
  NavigateToCrossSiteHtmlUrl(1 /* num_cors */, "" /* file_suffix */);
  EXPECT_EQ(1u, cross_site_connection_listener_->GetAcceptedSocketCount());
  EXPECT_EQ(1, observer()->CrossSiteLearned());
  EXPECT_EQ(2, observer()->SameSitePreconnected());
}

IN_PROC_BROWSER_TEST_F(PredictorBrowserTest, CrossSiteUseThreeSockets) {
  StopInterceptingCrossSiteOnUI();
  NavigateToCrossSiteHtmlUrl(3 /* num_cors */, "" /* file_suffix */);
  EXPECT_EQ(3u, cross_site_connection_listener_->GetAcceptedSocketCount());
  EXPECT_EQ(3, observer()->CrossSiteLearned());
  EXPECT_EQ(2, observer()->SameSitePreconnected());
}

// The following tests confirm that Chrome accurately predicts preconnects after
// learning from navigations. Note that every "learned" connection adds ~.33 to
// the expected connection number, which starts at 2. Every preconnect Chrome
// performs multiplies the expected connections by .66.
//
// In order to simplify things, many of the following tests intercept requests
// to the cross site test server. This allows the test server to maintain a
// "clean slate" with no connected sockets, so that when the tests need to check
// that predictor initiated preconnects occur, there are no races and the
// connections are deterministic.
//
// One additional complexity is that if a preconnect from A to B is learned, an
// extra preconnect will be issued if the host part of A and B match (ignoring
// port). TODO(csharrison): This logic could probably be removed from the
// predictor.
IN_PROC_BROWSER_TEST_F(PredictorBrowserTest,
                       CrossSiteSimplePredictionAfterOneNavigation) {
  NavigateToCrossSiteHtmlUrl(2 /* num_cors */, "" /* file_suffix */);
  EXPECT_EQ(2, observer()->CrossSiteLearned());

  EXPECT_EQ(0u, cross_site_connection_listener_->GetAcceptedSocketCount());

  // Navigate again and confirm a preconnect. Note that because the two
  // embedded test servers have the same host name, an extra preconnect is
  // issued. This results in ceil(2.66) + 1 = 4 preconnects.
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/title1.html"));
  EXPECT_EQ(4, observer()->CrossSitePreconnected());
  cross_site_connection_listener_->WaitForAcceptedConnectionsOnUI(4u);
}

// This test does not intercept initial requests / preconnects to the cross site
// test server. This is a sanity check to make sure that the interceptor doesn't
// change the logic in the predictor. Note that the test does not have the
// ability to inspect at the socket layer, but verifies that the predictor is at
// least making preconnect requests.
IN_PROC_BROWSER_TEST_F(
    PredictorBrowserTest,
    CrossSiteSimplePredictionAfterOneNavigationNoInterceptor) {
  StopInterceptingCrossSiteOnUI();
  NavigateToCrossSiteHtmlUrl(1 /* num_cors */, "" /* file_suffix */);
  EXPECT_EQ(1, observer()->CrossSiteLearned());
  cross_site_connection_listener_->WaitForAcceptedConnectionsOnUI(1u);

  // Navigate again and confirm a preconnect. Note that because the two
  // embedded test servers have the same host_piece, an extra preconnect is
  // issued. This results in ceil(2.33) + 1 = 4 preconnects.
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/title1.html"));
  // Just check that predictor has initiated preconnects to the cross site test
  // server. It's tricky to reset the connections to the test server, and
  // sockets can be reused.
  EXPECT_EQ(4, observer()->CrossSitePreconnected());
}

// Expect that the predictor correctly predicts subframe navigations.
IN_PROC_BROWSER_TEST_F(PredictorBrowserTest, SubframeCrossSitePrediction) {
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/predictor/predictor_cross_site_subframe_nav.html"));
  bool result = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      browser()->tab_strip_model()->GetActiveWebContents(),
      base::StringPrintf(
          "navigateSubframe('%s')",
          cross_site_test_server()->GetURL("/title1.html").spec().c_str()),
      &result));
  EXPECT_TRUE(result);
  EXPECT_EQ(1, observer()->CrossSiteLearned());

  // The subframe navigation initiates two preconnects.
  cross_site_connection_listener_->WaitForAcceptedConnectionsOnUI(2u);

  FlushClientSocketsOnUIThread();
  FlushServerSocketsOnUIThread(cross_site_test_server());
  cross_site_connection_listener_->ResetCounts();

  EXPECT_EQ(0u, cross_site_connection_listener_->GetAcceptedSocketCount());

  // Navigate again and confirm a preconnect. Note that because the two
  // embedded test servers have the same host_piece, an extra preconnect is
  // issued. This results in ceil(2 + .33) + 1 = 4 preconnects.
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/title1.html"));
  EXPECT_EQ(4, observer()->CrossSitePreconnected());
  cross_site_connection_listener_->WaitForAcceptedConnectionsOnUI(4u);
}

// Expect that the predictor correctly preconnects an already learned resource
// if the host shows up in a subframe. This test is equivalent to
// CrossSiteSimplePredictionAfterOneNavigation, with the second navigation
// (which initiates the preconnect) happening for a subframe load.
IN_PROC_BROWSER_TEST_F(PredictorBrowserTest, SubframeInitiatesPreconnects) {
  // Navigate to the normal cross site URL to learn the relationship.
  NavigateToCrossSiteHtmlUrl(1 /* num_cors */, "" /* file_suffix */);
  EXPECT_EQ(1, observer()->CrossSiteLearned());

  EXPECT_EQ(0u, cross_site_connection_listener_->GetAcceptedSocketCount());

  // Navigate again and confirm a preconnect. Note that because the two
  // embedded test servers have the same host_piece, an extra preconnect is
  // issued. This results in ceil(2 + .33) + 1 = 4 preconnects.
  NavigateToDataURLWithContent(
      "<iframe src=\"" + embedded_test_server()->GetURL("/title1.html").spec() +
      "\"></iframe>");
  EXPECT_EQ(4, observer()->CrossSitePreconnected());
  cross_site_connection_listener_->WaitForAcceptedConnectionsOnUI(4u);
}

// Expect that the predictor correctly learns the subresources a subframe needs
// to preconnect to.
IN_PROC_BROWSER_TEST_F(PredictorBrowserTest, SubframeLearning) {
  std::string path = base::StringPrintf(
      "/predictor/"
      "predictor_cross_site.html?subresourceHost=%s&"
      "numCORSResources=1&sendImmediately=1",
      cross_site_test_server()->base_url().spec().c_str());
  NavigateToDataURLWithContent(
      base::StringPrintf("<iframe src=\"%s\"></iframe>",
                         embedded_test_server()->GetURL(path).spec().c_str()));
  EXPECT_EQ(1, observer()->CrossSiteLearned());

  EXPECT_EQ(0u, cross_site_connection_listener_->GetAcceptedSocketCount());

  // Navigate again and confirm a preconnect. Note that because the two
  // embedded test servers have the same host_piece, an extra preconnect is
  // issued. This results in ceil(2 + .33) + 1 = 4 preconnects.
  NavigateToDataURLWithContent(
      "<iframe src=\"" + embedded_test_server()->GetURL("/title1.html").spec() +
      "\"></iframe>");
  cross_site_connection_listener_->WaitForAcceptedConnectionsOnUI(4u);
  EXPECT_EQ(4u, cross_site_connection_listener_->GetAcceptedSocketCount());
  EXPECT_EQ(4, observer()->CrossSitePreconnected());
}

// This test navigates to an html file with a tag:
// <meta name="referrer" content="never">. This tests the implementation details
// of the predictor. The current predictor only learns via the referrer header.
IN_PROC_BROWSER_TEST_F(PredictorBrowserTest,
                       CrossSiteNoReferrerNoPredictionAfterOneNavigation) {
  NavigateToCrossSiteHtmlUrl(2 /* num_cors */,
                             "_no_referrer" /* file_suffix */);
  EXPECT_EQ(0, observer()->CrossSiteLearned());
  EXPECT_EQ(2, observer()->SameSitePreconnected());

  EXPECT_EQ(0u, cross_site_connection_listener_->GetAcceptedSocketCount());

  // Navigate again and confirm that no preconnects occurred.
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/title1.html"));
  EXPECT_EQ(0, observer()->CrossSitePreconnected());
  EXPECT_EQ(0u, cross_site_connection_listener_->GetAcceptedSocketCount());
}

IN_PROC_BROWSER_TEST_F(PredictorBrowserTest,
                       CrossSiteSimplePredictionAfterTwoNavigations) {
  NavigateToCrossSiteHtmlUrl(1 /* num_cors */, "" /* file_suffix */);
  EXPECT_EQ(0, observer()->CrossSitePreconnected());
  EXPECT_EQ(1, observer()->CrossSiteLearned());

  NavigateToCrossSiteHtmlUrl(1 /* num_cors */, "" /* file_suffix */);
  EXPECT_EQ(4, observer()->CrossSitePreconnected());
  cross_site_connection_listener_->WaitForAcceptedConnectionsOnUI(4u);

  FlushClientSocketsOnUIThread();
  FlushServerSocketsOnUIThread(cross_site_test_server());
  cross_site_connection_listener_->ResetCounts();
  observer()->ResetCounts();

  EXPECT_EQ(0u, cross_site_connection_listener_->GetAcceptedSocketCount());

  // Navigate again and confirm a preconnect. Note that because the two
  // embedded test servers have the same host_piece, an extra preconnect is
  // issued. This results in ceil(((2 + .33) + .33)*.66) + 1 = 3 preconnects.
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/title1.html"));
  EXPECT_EQ(3, observer()->CrossSitePreconnected());
  cross_site_connection_listener_->WaitForAcceptedConnectionsOnUI(3u);
}

IN_PROC_BROWSER_TEST_F(PredictorBrowserTest,
                       CrossSiteSimplePredictionAfterTwoNavigations2) {
  NavigateToCrossSiteHtmlUrl(2 /* num_cors */, "" /* file_suffix */);
  EXPECT_EQ(0, observer()->CrossSitePreconnected());
  EXPECT_EQ(2, observer()->CrossSiteLearned());

  NavigateToCrossSiteHtmlUrl(2 /* num_cors */, "" /* file_suffix */);

  EXPECT_EQ(4, observer()->CrossSitePreconnected());
  cross_site_connection_listener_->WaitForAcceptedConnectionsOnUI(4u);

  FlushClientSocketsOnUIThread();
  FlushServerSocketsOnUIThread(cross_site_test_server());
  cross_site_connection_listener_->ResetCounts();
  observer()->ResetCounts();

  EXPECT_EQ(0u, cross_site_connection_listener_->GetAcceptedSocketCount());

  // ((2 + .66) + .66)*.66 + 1 ~= 3.2.
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/title1.html"));
  EXPECT_EQ(4, observer()->CrossSitePreconnected());
  cross_site_connection_listener_->WaitForAcceptedConnectionsOnUI(4u);
}

// The first navigation uses a subresource. Subsequent navigations don't use
// that subresource. This tests how the predictor forgets about these bad
// navigations.
IN_PROC_BROWSER_TEST_F(PredictorBrowserTest, ForgetBadPrediction) {
  NavigateToCrossSiteHtmlUrl(1 /* num_cors */, "" /* file_suffix */);
  EXPECT_EQ(1, observer()->CrossSiteLearned());

  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/title1.html"));
  // (2 + .33) + 1 = 3.33.
  EXPECT_EQ(4, observer()->CrossSitePreconnected());
  observer()->ResetCounts();
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/title1.html"));
  // ceil((2 + .33) * .66) + 1 = 3.
  EXPECT_EQ(3, observer()->CrossSitePreconnected());
  observer()->ResetCounts();
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/title1.html"));
  // ceil((2 + .33) * .66 * .66) + 1 = 3.
  EXPECT_EQ(3, observer()->CrossSitePreconnected());
  observer()->ResetCounts();

  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/title1.html"));
  // Finally, (2 + .33) * .66^3 ~= .67. Not enough for a preconnect.
  EXPECT_EQ(0, observer()->CrossSitePreconnected());
}

// The predictor does not follow redirects if the original url had a non-empty
// path (a path that was more than just "/").
IN_PROC_BROWSER_TEST_F(PredictorBrowserTest,
                       CrossSiteRedirectNoPredictionWithPath) {
  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL(base::StringPrintf(
          "/server-redirect?%s",
          cross_site_test_server()->GetURL("/title1.html").spec().c_str())));
  EXPECT_EQ(0, observer()->CrossSiteLearned());

  EXPECT_EQ(2, observer()->CrossSitePreconnected());
  cross_site_connection_listener_->WaitForAcceptedConnectionsOnUI(2u);

  FlushClientSocketsOnUIThread();
  FlushServerSocketsOnUIThread(cross_site_test_server());
  cross_site_connection_listener_->ResetCounts();
  observer()->ResetCounts();

  EXPECT_EQ(0u, cross_site_connection_listener_->GetAcceptedSocketCount());

  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/title1.html"));
  EXPECT_EQ(0u, cross_site_connection_listener_->GetAcceptedSocketCount());
  EXPECT_EQ(0, observer()->CrossSitePreconnected());
}

// The predictor does follow redirects if the original url had an empty path
// (a path that was more than just "/"). Use the registered "/" path to redirect
// to the target test server.
IN_PROC_BROWSER_TEST_F(PredictorBrowserTest,
                       CrossSiteRedirectPredictionWithNoPath) {
  ui_test_utils::NavigateToURL(browser(), embedded_test_server()->base_url());
  EXPECT_EQ(1, observer()->CrossSiteLearned());

  EXPECT_EQ(2, observer()->CrossSitePreconnected());
  cross_site_connection_listener_->WaitForAcceptedConnectionsOnUI(2u);

  FlushClientSocketsOnUIThread();
  FlushServerSocketsOnUIThread(cross_site_test_server());
  cross_site_connection_listener_->ResetCounts();
  observer()->ResetCounts();

  EXPECT_EQ(0u, cross_site_connection_listener_->GetAcceptedSocketCount());

  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/title1.html"));
  // Preconnect 4 sockets because ceil(2 + .33) + 1 = 4.
  EXPECT_EQ(4, observer()->CrossSitePreconnected());
  cross_site_connection_listener_->WaitForAcceptedConnectionsOnUI(4u);
}

// This test uses "localhost" instead of "127.0.0.1" to avoid extra preconnects
// to hosts with the same host piece (ignoring port). Note that the preconnect
// observer is not used here due to its strict checks on hostname.
IN_PROC_BROWSER_TEST_F(PredictorBrowserTest,
                       CrossSiteRedirectPredictionWithNoPathDifferentHostName) {
  std::string same_site_localhost_host = base::StringPrintf(
      "%s://localhost:%s", embedded_test_server()->base_url().scheme().c_str(),
      embedded_test_server()->base_url().port().c_str());
  // The default predictor observer does not use "localhost".
  InstallPredictorObserver(GURL(same_site_localhost_host),
                           cross_site_test_server()->base_url());
  GURL localhost_source = GURL(base::StringPrintf(
      "%s/predictor/"
      "predictor_cross_site.html?subresourceHost=%s&numCORSResources=1",
      same_site_localhost_host.c_str(),
      cross_site_test_server()->base_url().spec().c_str()));
  ui_test_utils::NavigateToURL(browser(), localhost_source);
  bool result = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "startFetchesAndWaitForReply()", &result));
  EXPECT_TRUE(result);

  EXPECT_EQ(0u, cross_site_connection_listener_->GetAcceptedSocketCount());

  ui_test_utils::NavigateToURL(
      browser(), GURL(base::StringPrintf("%s/title1.html",
                                         same_site_localhost_host.c_str())));
  // Preconnect 3 sockets because ceil(2 + .33) = 3. Note that this time there
  // is no additional connection due to same host.
  cross_site_connection_listener_->WaitForAcceptedConnectionsOnUI(3u);
}

// Perform the "/" redirect twice and make sure the predictor updates twice.
IN_PROC_BROWSER_TEST_F(PredictorBrowserTest,
                       CrossSiteTwoRedirectsPredictionWithNoPath) {
  // Navigate once and redirect.
  ui_test_utils::NavigateToURL(browser(), embedded_test_server()->base_url());
  EXPECT_EQ(1, observer()->CrossSiteLearned());

  EXPECT_EQ(2, observer()->CrossSitePreconnected());
  cross_site_connection_listener_->WaitForAcceptedConnectionsOnUI(2u);

  FlushClientSocketsOnUIThread();
  FlushServerSocketsOnUIThread(cross_site_test_server());
  cross_site_connection_listener_->ResetCounts();
  observer()->ResetCounts();

  // Navigate again and redirect.
  ui_test_utils::NavigateToURL(browser(), embedded_test_server()->base_url());
  EXPECT_EQ(1, observer()->CrossSiteLearned());

  // 2 + .33 + 1 = 3.33.
  EXPECT_EQ(4, observer()->CrossSitePreconnected());
  cross_site_connection_listener_->WaitForAcceptedConnectionsOnUI(4u);

  FlushClientSocketsOnUIThread();
  FlushServerSocketsOnUIThread(cross_site_test_server());
  cross_site_connection_listener_->ResetCounts();
  observer()->ResetCounts();

  EXPECT_EQ(0u, cross_site_connection_listener_->GetAcceptedSocketCount());

  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/title1.html"));
  // 3 preconnects expected because ((2 + .33)*.66 + .33)*.66 + 1 = 2.23.
  EXPECT_EQ(3, observer()->CrossSitePreconnected());
  cross_site_connection_listener_->WaitForAcceptedConnectionsOnUI(3u);
}

IN_PROC_BROWSER_TEST_F(PredictorBrowserTest,
                       RendererInitiatedNavigationPreconnect) {
  NavigateToCrossSiteHtmlUrl(1 /* num_cors */, "" /* file_suffix */);
  EXPECT_EQ(1, observer()->CrossSiteLearned());
  EXPECT_EQ(0u, cross_site_connection_listener_->GetAcceptedSocketCount());

  // Now, navigate using a renderer initiated navigation and expect preconnects.
  EXPECT_TRUE(content::ExecuteScript(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "window.location.href='/title1.html'"));

  // The renderer initiated navigation is not synchronous, so just wait for the
  // preconnects to go through.
  // Note that ceil(2 + .33) + 1 = 4.
  cross_site_connection_listener_->WaitForAcceptedConnectionsOnUI(4u);
}

IN_PROC_BROWSER_TEST_F(PredictorBrowserTest,
                       PRE_ShutdownStartupCyclePreresolve) {
  // Prepare state that will be serialized on this shut-down and read on next
  // start-up. Ensure preresolution over preconnection.
  LearnAboutInitialNavigation(startup_url_);
  // The target url will have a expected connection count of 2 after this call.
  InstallPredictorObserver(referring_url_, target_url_);
  LearnFromNavigation(referring_url_, target_url_);

  // In order to reduce the expected connection count < .8, issue predictions 3
  // times. 2 * .66^3 ~= .58.
  PrepareFrameSubresources(referring_url_);
  PrepareFrameSubresources(referring_url_);
  PrepareFrameSubresources(referring_url_);
}

IN_PROC_BROWSER_TEST_F(PredictorBrowserTest, ShutdownStartupCyclePreresolve) {
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
