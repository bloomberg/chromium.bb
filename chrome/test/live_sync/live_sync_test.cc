// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/live_sync/live_sync_test.h"

#include <vector>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/task.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/values.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/password_manager/encryptor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sync/profile_sync_service_harness.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/net/url_fetcher.h"
#include "chrome/common/net/url_request_context_getter.h"
#include "chrome/common/net/test_url_fetcher_factory.h"
#include "chrome/test/testing_browser_process.h"
#include "chrome/test/ui_test_utils.h"
#include "googleurl/src/gurl.h"
#include "net/base/escape.h"
#include "net/base/network_change_notifier.h"
#include "net/test/test_server.h"
#include "net/proxy/proxy_config.h"
#include "net/proxy/proxy_config_service_fixed.h"
#include "net/proxy/proxy_service.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_status.h"

namespace switches {
const char kPasswordFileForTest[] = "password-file-for-test";
const char kSyncUserForTest[] = "sync-user-for-test";
const char kSyncPasswordForTest[] = "sync-password-for-test";
const char kSyncServerCommandLine[] = "sync-server-command-line";
}

namespace {
// The URLs for different calls in the Google Accounts programmatic login API.
const char kClientLoginUrl[] = "https://www.google.com/accounts/ClientLogin";
const char kGetUserInfoUrl[] = "https://www.google.com/accounts/GetUserInfo";
const char kIssueAuthTokenUrl[] =
    "https://www.google.com/accounts/IssueAuthToken";
const char kSearchDomainCheckUrl[] =
    "https://www.google.com/searchdomaincheck?format=domain&type=chrome";
}

// Helper class that checks whether a sync test server is running or not.
class SyncServerStatusChecker : public URLFetcher::Delegate {
 public:
  SyncServerStatusChecker() : running_(false) {}

  virtual void OnURLFetchComplete(const URLFetcher* source,
                                  const GURL& url,
                                  const net::URLRequestStatus& status,
                                  int response_code,
                                  const ResponseCookies& cookies,
                                  const std::string& data) {
    running_ = (status.status() == net::URLRequestStatus::SUCCESS &&
                response_code == 200 && data.find("ok") == 0);
    MessageLoop::current()->Quit();
  }

  bool running() const { return running_; }

 private:
  bool running_;
};

class SetProxyConfigTask : public Task {
 public:
  SetProxyConfigTask(base::WaitableEvent* done,
                     URLRequestContextGetter* url_request_context_getter,
                     const net::ProxyConfig& proxy_config)
      : done_(done),
        url_request_context_getter_(url_request_context_getter),
        proxy_config_(proxy_config) {
  }

  void Run() {
    net::ProxyService* proxy_service =
        url_request_context_getter_->GetURLRequestContext()->proxy_service();
    proxy_service->ResetConfigService(
        new net::ProxyConfigServiceFixed(proxy_config_));
    done_->Signal();
  }

 private:
  base::WaitableEvent* done_;
  URLRequestContextGetter* url_request_context_getter_;
  net::ProxyConfig proxy_config_;
};

LiveSyncTest::LiveSyncTest(TestType test_type)
    : sync_server_(net::TestServer::TYPE_SYNC, FilePath()),
      test_type_(test_type),
      num_clients_(-1),
      test_server_handle_(base::kNullProcessHandle) {
  InProcessBrowserTest::set_show_window(true);

  switch (test_type_) {
    case SINGLE_CLIENT: {
      num_clients_ = 1;
      break;
    }
    case TWO_CLIENT: {
      num_clients_ = 2;
      break;
    }
    case MULTIPLE_CLIENT: {
      num_clients_ = 3;
      break;
    }
    case MANY_CLIENT: {
      num_clients_ = 10;
      break;
    }
  }
}

LiveSyncTest::~LiveSyncTest() {}

void LiveSyncTest::SetUp() {
  CommandLine* cl = CommandLine::ForCurrentProcess();
  if (cl->HasSwitch(switches::kPasswordFileForTest)) {
    ReadPasswordFile();
  } else if (cl->HasSwitch(switches::kSyncUserForTest) &&
             cl->HasSwitch(switches::kSyncPasswordForTest)) {
    username_ = cl->GetSwitchValueASCII(switches::kSyncUserForTest);
    password_ = cl->GetSwitchValueASCII(switches::kSyncPasswordForTest);
  } else {
    SetupMockGaiaResponses();
  }
  if (username_.empty() || password_.empty())
    LOG(FATAL) << "Cannot run sync tests without GAIA credentials.";

  // Mock the Mac Keychain service.  The real Keychain can block on user input.
#if defined(OS_MACOSX)
  Encryptor::UseMockKeychain(true);
#endif

  // Yield control back to the InProcessBrowserTest framework.
  InProcessBrowserTest::SetUp();
}

void LiveSyncTest::TearDown() {
  // Allow the InProcessBrowserTest framework to perform its tear down.
  InProcessBrowserTest::TearDown();

  // Stop the local python test server. This is a no-op if one wasn't started.
  TearDownLocalPythonTestServer();

  // Stop the local sync test server. This is a no-op if one wasn't started.
  TearDownLocalTestServer();
}

void LiveSyncTest::SetUpCommandLine(CommandLine* cl) {
  // TODO(rsimha): Until we implement a fake Tango server against which tests
  // can run, we need to set the --sync-notification-method to "p2p".
  if (!cl->HasSwitch(switches::kSyncNotificationMethod))
    cl->AppendSwitchASCII(switches::kSyncNotificationMethod, "p2p");

  // TODO(sync): Remove this once sessions sync is enabled by default.
  if (!cl->HasSwitch(switches::kEnableSyncSessions))
    cl->AppendSwitch(switches::kEnableSyncSessions);

  // Disable non-essential access of external network resources.
  if (!cl->HasSwitch(switches::kDisableBackgroundNetworking))
    cl->AppendSwitch(switches::kDisableBackgroundNetworking);
}

// static
Profile* LiveSyncTest::MakeProfile(const FilePath::StringType name) {
  FilePath path;
  PathService::Get(chrome::DIR_USER_DATA, &path);
  return ProfileManager::CreateProfile(path.Append(name));
}

Profile* LiveSyncTest::GetProfile(int index) {
  if (profiles_.empty())
    LOG(FATAL) << "SetupSync() has not yet been called.";
  if (index < 0 || index >= static_cast<int>(profiles_.size()))
    LOG(FATAL) << "GetProfile(): Index is out of bounds.";
  return profiles_[index];
}

ProfileSyncServiceHarness* LiveSyncTest::GetClient(int index) {
  if (clients_.empty())
    LOG(FATAL) << "SetupClients() has not yet been called.";
  if (index < 0 || index >= static_cast<int>(clients_.size()))
    LOG(FATAL) << "GetClient(): Index is out of bounds.";
  return clients_[index];
}

Profile* LiveSyncTest::verifier() {
  if (verifier_.get() == NULL)
    LOG(FATAL) << "SetupClients() has not yet been called.";
  return verifier_.get();
}

bool LiveSyncTest::SetupClients() {
  if (num_clients_ <= 0)
    LOG(FATAL) << "num_clients_ incorrectly initialized.";
  if (!profiles_.empty() || !clients_.empty())
    LOG(FATAL) << "SetupClients() has already been called.";

  // Start up a sync test server if one is needed.
  SetUpTestServerIfRequired();

  // Create the required number of sync profiles and clients.
  for (int i = 0; i < num_clients_; ++i) {
    profiles_.push_back(MakeProfile(
        StringPrintf(FILE_PATH_LITERAL("Profile%d"), i)));
    EXPECT_FALSE(GetProfile(i) == NULL) << "GetProfile(" << i << ") failed.";
    clients_.push_back(
        new ProfileSyncServiceHarness(GetProfile(i), username_, password_, i));
    EXPECT_FALSE(GetClient(i) == NULL) << "GetClient(" << i << ") failed.";
  }

  // Create the verifier profile.
  verifier_.reset(MakeProfile(FILE_PATH_LITERAL("Verifier")));
  return (verifier_.get() != NULL);
}

bool LiveSyncTest::SetupSync() {
  // Create sync profiles and clients if they haven't already been created.
  if (profiles_.empty()) {
    if (!SetupClients())
      LOG(FATAL) << "SetupClients() failed.";
  }

  // Sync each of the profiles.
  for (int i = 0; i < num_clients_; ++i) {
    if (!GetClient(i)->SetupSync())
      LOG(FATAL) << "SetupSync() failed.";
  }

  return true;
}

void LiveSyncTest::CleanUpOnMainThread() {
  profiles_.reset();
  clients_.reset();
  verifier_.reset(NULL);
}

void LiveSyncTest::SetUpInProcessBrowserTestFixture() {
  // We don't take a reference to |resolver|, but mock_host_resolver_override_
  // does, so effectively assumes ownership.
  net::RuleBasedHostResolverProc* resolver =
      new net::RuleBasedHostResolverProc(host_resolver());
  resolver->AllowDirectLookup("*.google.com");
  // On Linux, we use Chromium's NSS implementation which uses the following
  // hosts for certificate verification. Without these overrides, running the
  // integration tests on Linux causes error as we make external DNS lookups.
  resolver->AllowDirectLookup("*.thawte.com");
  resolver->AllowDirectLookup("*.geotrust.com");
  resolver->AllowDirectLookup("*.gstatic.com");
  mock_host_resolver_override_.reset(
      new net::ScopedDefaultHostResolverProc(resolver));
}

void LiveSyncTest::TearDownInProcessBrowserTestFixture() {
  mock_host_resolver_override_.reset();

  // Switch back to using the default URLFetcher factory. This is a no-op if
  // a fake factory wasn't used.
  URLFetcher::set_factory(NULL);
}

void LiveSyncTest::ReadPasswordFile() {
  CommandLine* cl = CommandLine::ForCurrentProcess();
  password_file_ = cl->GetSwitchValuePath(switches::kPasswordFileForTest);
  if (password_file_.empty())
    LOG(FATAL) << "Can't run live server test without specifying --"
               << switches::kPasswordFileForTest << "=<filename>";
  std::string file_contents;
  file_util::ReadFileToString(password_file_, &file_contents);
  ASSERT_NE(file_contents, "") << "Password file \""
      << password_file_.value() << "\" does not exist.";
  std::vector<std::string> tokens;
  std::string delimiters = "\r\n";
  Tokenize(file_contents, delimiters, &tokens);
  ASSERT_TRUE(tokens.size() == 2) << "Password file \""
      << password_file_.value()
      << "\" must contain exactly two lines of text.";
  username_ = tokens[0];
  password_ = tokens[1];
}

void LiveSyncTest::SetupMockGaiaResponses() {
  username_ = "user@gmail.com";
  password_ = "password";
  factory_.reset(new FakeURLFetcherFactory());
  factory_->SetFakeResponse(kClientLoginUrl, "SID=sid\nLSID=lsid", true);
  factory_->SetFakeResponse(kGetUserInfoUrl, "email=user@gmail.com", true);
  factory_->SetFakeResponse(kIssueAuthTokenUrl, "auth", true);
  factory_->SetFakeResponse(kSearchDomainCheckUrl, ".google.com", true);
  URLFetcher::set_factory(factory_.get());
}

// Start up a local sync server if required.
// - If a sync server URL and a sync server command line are provided, start up
//   a local sync server by running the command line. Chrome will connect to the
//   server at the URL that was provided.
// - If neither a sync server URL nor a sync server command line are provided,
//   start up a local python sync test server and point Chrome to its URL.
// - If a sync server URL is provided, but not a server command line, it is
//   assumed that the server is already running. Chrome will automatically
//   connect to it at the URL provided. There is nothing to do here.
// - If a sync server command line is provided, but not a server URL, we flag an
//   error.
void LiveSyncTest::SetUpTestServerIfRequired() {
  CommandLine* cl = CommandLine::ForCurrentProcess();
  if (cl->HasSwitch(switches::kSyncServiceURL) &&
      cl->HasSwitch(switches::kSyncServerCommandLine)) {
    if (!SetUpLocalTestServer())
      LOG(FATAL) << "Failed to set up local test server";
  } else if (!cl->HasSwitch(switches::kSyncServiceURL) &&
             !cl->HasSwitch(switches::kSyncServerCommandLine)) {
    if (!SetUpLocalPythonTestServer())
      LOG(FATAL) << "Failed to set up local python test server";
  } else if (!cl->HasSwitch(switches::kSyncServiceURL) &&
             cl->HasSwitch(switches::kSyncServerCommandLine)) {
    LOG(FATAL) << "Sync server command line must be accompanied by sync "
                  "service URL.";
  }
}

bool LiveSyncTest::SetUpLocalPythonTestServer() {
  EXPECT_TRUE(sync_server_.Start())
      << "Could not launch local python test server.";

  CommandLine* cl = CommandLine::ForCurrentProcess();
  std::string sync_service_url = sync_server_.GetURL("chromiumsync").spec();
  cl->AppendSwitchASCII(switches::kSyncServiceURL, sync_service_url);
  VLOG(1) << "Started local python test server at " << sync_service_url;

  int xmpp_port = 0;
  if (!sync_server_.server_data().GetInteger("xmpp_port", &xmpp_port)) {
    LOG(ERROR) << "Could not find valid xmpp_port value";
    return false;
  }
  if ((xmpp_port <= 0) || (xmpp_port > kuint16max)) {
    LOG(ERROR) << "Invalid xmpp port: " << xmpp_port;
    return false;
  }

  net::HostPortPair xmpp_host_port_pair(sync_server_.host_port_pair());
  xmpp_host_port_pair.set_port(xmpp_port);
  xmpp_port_.reset(new net::ScopedPortException(xmpp_port));

  if (!cl->HasSwitch(switches::kSyncNotificationHost)) {
    cl->AppendSwitchASCII(switches::kSyncNotificationHost,
                          xmpp_host_port_pair.ToString());
    // The local XMPP server only supports insecure connections.
    cl->AppendSwitch(switches::kSyncAllowInsecureXmppConnection);
  }

  return true;
}

bool LiveSyncTest::SetUpLocalTestServer() {
  CommandLine* cl = CommandLine::ForCurrentProcess();
  CommandLine::StringType server_cmdline_string = cl->GetSwitchValueNative(
      switches::kSyncServerCommandLine);
#if defined(OS_WIN)
  CommandLine server_cmdline = CommandLine::FromString(server_cmdline_string);
#else
  std::vector<std::string> server_cmdline_vector;
  std::string delimiters(" ");
  Tokenize(server_cmdline_string, delimiters, &server_cmdline_vector);
  CommandLine server_cmdline(server_cmdline_vector);
#endif
  if (!base::LaunchApp(server_cmdline, false, true, &test_server_handle_))
    LOG(ERROR) << "Could not launch local test server.";

  const int kMaxWaitTime = TestTimeouts::live_operation_timeout_ms();
  const int kNumIntervals = 15;
  if (WaitForTestServerToStart(kMaxWaitTime, kNumIntervals)) {
    VLOG(1) << "Started local test server at "
            << cl->GetSwitchValueASCII(switches::kSyncServiceURL) << ".";
    return true;
  } else {
    LOG(ERROR) << "Could not start local test server at "
               << cl->GetSwitchValueASCII(switches::kSyncServiceURL) << ".";
    return false;
  }
}

bool LiveSyncTest::TearDownLocalPythonTestServer() {
  if (!sync_server_.Stop()) {
    LOG(ERROR) << "Could not stop local python test server.";
    return false;
  }
  xmpp_port_.reset();
  return true;
}

bool LiveSyncTest::TearDownLocalTestServer() {
  if (test_server_handle_ != base::kNullProcessHandle) {
    EXPECT_TRUE(base::KillProcess(test_server_handle_, 0, false))
        << "Could not stop local test server.";
    base::CloseProcessHandle(test_server_handle_);
    test_server_handle_ = base::kNullProcessHandle;
  }
  return true;
}

bool LiveSyncTest::WaitForTestServerToStart(int time_ms, int intervals) {
  for (int i = 0; i < intervals; ++i) {
    if (IsTestServerRunning())
      return true;
    base::PlatformThread::Sleep(time_ms / intervals);
  }
  return false;
}

bool LiveSyncTest::IsTestServerRunning() {
  CommandLine* cl = CommandLine::ForCurrentProcess();
  std::string sync_url = cl->GetSwitchValueASCII(switches::kSyncServiceURL);
  GURL sync_url_status(sync_url.append("/healthz"));
  SyncServerStatusChecker delegate;
  URLFetcher fetcher(sync_url_status, URLFetcher::GET, &delegate);
  fetcher.set_request_context(Profile::GetDefaultRequestContext());
  fetcher.Start();
  ui_test_utils::RunMessageLoop();
  return delegate.running();
}

void LiveSyncTest::EnableNetwork(Profile* profile) {
  SetProxyConfig(profile->GetRequestContext(),
                 net::ProxyConfig::CreateDirect());
  // TODO(rsimha): Remove this line once http://crbug.com/53857 is fixed.
  net::NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
}

void LiveSyncTest::DisableNetwork(Profile* profile) {
  // Set the current proxy configuration to a nonexistent proxy to effectively
  // disable networking.
  net::ProxyConfig config;
  config.proxy_rules().ParseFromString("http=127.0.0.1:0");
  SetProxyConfig(profile->GetRequestContext(), config);
  // TODO(rsimha): Remove this line once http://crbug.com/53857 is fixed.
  net::NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
}

bool LiveSyncTest::AwaitQuiescence() {
  return ProfileSyncServiceHarness::AwaitQuiescence(clients());
}

void LiveSyncTest::SetProxyConfig(URLRequestContextGetter* context_getter,
                                  const net::ProxyConfig& proxy_config) {
  base::WaitableEvent done(false, false);
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      new SetProxyConfigTask(&done,
                             context_getter,
                             proxy_config));
  done.Wait();
}
