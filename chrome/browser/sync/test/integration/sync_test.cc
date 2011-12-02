// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/sync_test.h"

#include <vector>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/task.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/password_manager/encryptor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/sync/notifier/p2p_notifier.h"
#include "chrome/browser/sync/profile_sync_service_harness.h"
#include "chrome/browser/sync/protocol/sync.pb.h"
#include "chrome/browser/sync/test/integration/sync_datatype_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/net/gaia/gaia_urls.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/public/common/url_fetcher.h"
#include "content/public/common/url_fetcher_delegate.h"
#include "content/test/test_browser_thread.h"
#include "content/test/test_url_fetcher_factory.h"
#include "googleurl/src/gurl.h"
#include "net/base/escape.h"
#include "net/base/network_change_notifier.h"
#include "net/proxy/proxy_config.h"
#include "net/proxy/proxy_config_service_fixed.h"
#include "net/proxy/proxy_service.h"
#include "net/test/test_server.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"

using content::BrowserThread;

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
const char kOAuth2LoginTokenValidResponse[] =
    "{"
    "  \"refresh_token\": \"rt1\","
    "  \"access_token\": \"at1\","
    "  \"expires_in\": 3600,"
    "  \"token_type\": \"Bearer\""
    "}";
}

// Helper class that checks whether a sync test server is running or not.
class SyncServerStatusChecker : public content::URLFetcherDelegate {
 public:
  SyncServerStatusChecker() : running_(false) {}

  virtual void OnURLFetchComplete(const content::URLFetcher* source) {
    std::string data;
    source->GetResponseAsString(&data);
    running_ =
        (source->GetStatus().status() == net::URLRequestStatus::SUCCESS &&
        source->GetResponseCode() == 200 && data.find("ok") == 0);
    MessageLoop::current()->Quit();
  }

  bool running() const { return running_; }

 private:
  bool running_;
};

void SetProxyConfigCallback(
    base::WaitableEvent* done,
    net::URLRequestContextGetter* url_request_context_getter,
    const net::ProxyConfig& proxy_config) {
  net::ProxyService* proxy_service =
      url_request_context_getter->GetURLRequestContext()->proxy_service();
  proxy_service->ResetConfigService(
      new net::ProxyConfigServiceFixed(proxy_config));
  done->Signal();
}

SyncTest::SyncTest(TestType test_type)
    : sync_server_(net::TestServer::TYPE_SYNC, FilePath()),
      test_type_(test_type),
      server_type_(SERVER_TYPE_UNDECIDED),
      num_clients_(-1),
      use_verifier_(true),
      test_server_handle_(base::kNullProcessHandle) {
  InProcessBrowserTest::set_show_window(true);
  sync_datatype_helper::AssociateWithTest(this);
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
  }
}

SyncTest::~SyncTest() {}

void SyncTest::SetUp() {
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

  if (!cl->HasSwitch(switches::kSyncServiceURL) &&
      !cl->HasSwitch(switches::kSyncServerCommandLine)) {
    // If neither a sync server URL nor a sync server command line is
    // provided, start up a local python sync test server and point Chrome
    // to its URL.  This is the most common configuration, and the only
    // one that makes sense for most developers.
    server_type_ = LOCAL_PYTHON_SERVER;
  } else if (cl->HasSwitch(switches::kSyncServiceURL) &&
             cl->HasSwitch(switches::kSyncServerCommandLine)) {
    // If a sync server URL and a sync server command line are provided,
    // start up a local sync server by running the command line. Chrome
    // will connect to the server at the URL that was provided.
    server_type_ = LOCAL_LIVE_SERVER;
  } else if (cl->HasSwitch(switches::kSyncServiceURL) &&
             !cl->HasSwitch(switches::kSyncServerCommandLine)) {
    // If a sync server URL is provided, but not a server command line,
    // it is assumed that the server is already running. Chrome will
    // automatically connect to it at the URL provided. There is nothing
    // to do here.
    server_type_ = EXTERNAL_LIVE_SERVER;
  } else {
    // If a sync server command line is provided, but not a server URL,
    // we flag an error.
    LOG(FATAL) << "Can't figure out how to run a server.";
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

void SyncTest::TearDown() {
  // Allow the InProcessBrowserTest framework to perform its tear down.
  InProcessBrowserTest::TearDown();

  // Stop the local python test server. This is a no-op if one wasn't started.
  TearDownLocalPythonTestServer();

  // Stop the local sync test server. This is a no-op if one wasn't started.
  TearDownLocalTestServer();
}

void SyncTest::SetUpCommandLine(CommandLine* cl) {
  AddTestSwitches(cl);
  AddOptionalTypesToCommandLine(cl);
}

void SyncTest::AddTestSwitches(CommandLine* cl) {
  // TODO(rsimha): Until we implement a fake Tango server against which tests
  // can run, we need to set the --sync-notification-method to "p2p".
  if (!cl->HasSwitch(switches::kSyncNotificationMethod))
    cl->AppendSwitchASCII(switches::kSyncNotificationMethod, "p2p");

  // Disable non-essential access of external network resources.
  if (!cl->HasSwitch(switches::kDisableBackgroundNetworking))
    cl->AppendSwitch(switches::kDisableBackgroundNetworking);
}

void SyncTest::AddOptionalTypesToCommandLine(CommandLine* cl) {
  // TODO(sync): Remove this once sessions sync is enabled by default.
  if (!cl->HasSwitch(switches::kEnableSyncTabs))
    cl->AppendSwitch(switches::kEnableSyncTabs);
}

// static
Profile* SyncTest::MakeProfile(const FilePath::StringType name) {
  FilePath path;
  PathService::Get(chrome::DIR_USER_DATA, &path);
  path = path.Append(name);

  if (!file_util::PathExists(path))
    CHECK(file_util::CreateDirectory(path));

  Profile* profile = Profile::CreateProfile(path);
  g_browser_process->profile_manager()->RegisterTestingProfile(profile, true);
  return profile;
}

Profile* SyncTest::GetProfile(int index) {
  if (profiles_.empty())
    LOG(FATAL) << "SetupClients() has not yet been called.";
  if (index < 0 || index >= static_cast<int>(profiles_.size()))
    LOG(FATAL) << "GetProfile(): Index is out of bounds.";
  return profiles_[index];
}

Browser* SyncTest::GetBrowser(int index) {
  if (browsers_.empty())
    LOG(FATAL) << "SetupClients() has not yet been called.";
  if (index < 0 || index >= static_cast<int>(browsers_.size()))
    LOG(FATAL) << "GetBrowser(): Index is out of bounds.";
  return browsers_[index];
}

ProfileSyncServiceHarness* SyncTest::GetClient(int index) {
  if (clients_.empty())
    LOG(FATAL) << "SetupClients() has not yet been called.";
  if (index < 0 || index >= static_cast<int>(clients_.size()))
    LOG(FATAL) << "GetClient(): Index is out of bounds.";
  return clients_[index];
}

Profile* SyncTest::verifier() {
  if (verifier_ == NULL)
    LOG(FATAL) << "SetupClients() has not yet been called.";
  return verifier_;
}

void SyncTest::DisableVerifier() {
  use_verifier_ = false;
}

bool SyncTest::SetupClients() {
  if (num_clients_ <= 0)
    LOG(FATAL) << "num_clients_ incorrectly initialized.";
  if (!profiles_.empty() || !browsers_.empty() || !clients_.empty())
    LOG(FATAL) << "SetupClients() has already been called.";

  // Start up a sync test server if one is needed.
  SetUpTestServerIfRequired();

  // Create the required number of sync profiles, browsers and clients.
  for (int i = 0; i < num_clients_; ++i) {
    profiles_.push_back(MakeProfile(
        base::StringPrintf(FILE_PATH_LITERAL("Profile%d"), i)));
    EXPECT_FALSE(GetProfile(i) == NULL) << "GetProfile(" << i << ") failed.";

    browsers_.push_back(Browser::Create(GetProfile(i)));
    EXPECT_FALSE(GetBrowser(i) == NULL) << "GetBrowser(" << i << ") failed.";

    clients_.push_back(
        new ProfileSyncServiceHarness(GetProfile(i), username_, password_));
    EXPECT_FALSE(GetClient(i) == NULL) << "GetClient(" << i << ") failed.";

    ui_test_utils::WaitForBookmarkModelToLoad(
        GetProfile(i)->GetBookmarkModel());

    ui_test_utils::WaitForTemplateURLServiceToLoad(
        TemplateURLServiceFactory::GetForProfile(GetProfile(i)));
  }

  // Create the verifier profile.
  verifier_ = MakeProfile(FILE_PATH_LITERAL("Verifier"));
  ui_test_utils::WaitForBookmarkModelToLoad(verifier()->GetBookmarkModel());
  ui_test_utils::WaitForTemplateURLServiceToLoad(
      TemplateURLServiceFactory::GetForProfile(verifier()));
  return (verifier_ != NULL);
}

bool SyncTest::SetupSync() {
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

void SyncTest::CleanUpOnMainThread() {
  // Close all browser windows.
  BrowserList::CloseAllBrowsers();
  ui_test_utils::RunAllPendingInMessageLoop();

  // All browsers should be closed at this point, or else we could see memory
  // corruption in QuitBrowser().
  CHECK_EQ(0U, BrowserList::size());
  clients_.reset();
}

void SyncTest::SetUpInProcessBrowserTestFixture() {
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

void SyncTest::TearDownInProcessBrowserTestFixture() {
  mock_host_resolver_override_.reset();
}

void SyncTest::ReadPasswordFile() {
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
  ASSERT_EQ(2U, tokens.size()) << "Password file \""
      << password_file_.value()
      << "\" must contain exactly two lines of text.";
  username_ = tokens[0];
  password_ = tokens[1];
}

void SyncTest::SetupMockGaiaResponses() {
  username_ = "user@gmail.com";
  password_ = "password";
  factory_.reset(new URLFetcherImplFactory());
  fake_factory_.reset(new FakeURLFetcherFactory(factory_.get()));
  fake_factory_->SetFakeResponse(kClientLoginUrl, "SID=sid\nLSID=lsid", true);
  fake_factory_->SetFakeResponse(kGetUserInfoUrl, "email=user@gmail.com", true);
  fake_factory_->SetFakeResponse(kIssueAuthTokenUrl, "auth", true);
  fake_factory_->SetFakeResponse(kSearchDomainCheckUrl, ".google.com", true);
  fake_factory_->SetFakeResponse(
      GaiaUrls::GetInstance()->client_login_to_oauth2_url(),
      "some_response",
      true);
  fake_factory_->SetFakeResponse(
      GaiaUrls::GetInstance()->oauth2_token_url(),
      kOAuth2LoginTokenValidResponse,
      true);
}

// Start up a local sync server based on the value of server_type_, which
// was determined from the command line parameters.
void SyncTest::SetUpTestServerIfRequired() {
  if (server_type_ == LOCAL_PYTHON_SERVER) {
    if (!SetUpLocalPythonTestServer())
      LOG(FATAL) << "Failed to set up local python sync and XMPP servers";
  } else if (server_type_ == LOCAL_LIVE_SERVER) {
    // Using mock gaia credentials requires the use of a mock XMPP server.
    if (username_ == "user@gmail.com" && !SetUpLocalPythonTestServer())
      LOG(FATAL) << "Failed to set up local python XMPP server";
    if (!SetUpLocalTestServer())
      LOG(FATAL) << "Failed to set up local test server";
  } else if (server_type_ == EXTERNAL_LIVE_SERVER) {
    // Nothing to do; we'll just talk to the URL we were given.
  } else {
    LOG(FATAL) << "Don't know which server environment to run test in.";
  }
}

bool SyncTest::SetUpLocalPythonTestServer() {
  EXPECT_TRUE(sync_server_.Start())
      << "Could not launch local python test server.";

  CommandLine* cl = CommandLine::ForCurrentProcess();
  if (server_type_ == LOCAL_PYTHON_SERVER) {
    std::string sync_service_url = sync_server_.GetURL("chromiumsync").spec();
    cl->AppendSwitchASCII(switches::kSyncServiceURL, sync_service_url);
    DVLOG(1) << "Started local python sync server at " << sync_service_url;
  }

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
  DVLOG(1) << "Started local python XMPP server at "
           << xmpp_host_port_pair.ToString();

  return true;
}

bool SyncTest::SetUpLocalTestServer() {
  CommandLine* cl = CommandLine::ForCurrentProcess();
  CommandLine::StringType server_cmdline_string = cl->GetSwitchValueNative(
      switches::kSyncServerCommandLine);
  CommandLine::StringVector server_cmdline_vector;
  CommandLine::StringType delimiters(FILE_PATH_LITERAL(" "));
  Tokenize(server_cmdline_string, delimiters, &server_cmdline_vector);
  CommandLine server_cmdline(server_cmdline_vector);
  base::LaunchOptions options;
#if defined(OS_WIN)
  options.start_hidden = true;
#endif
  if (!base::LaunchProcess(server_cmdline, options, &test_server_handle_))
    LOG(ERROR) << "Could not launch local test server.";

  const int kMaxWaitTime = TestTimeouts::action_max_timeout_ms();
  const int kNumIntervals = 15;
  if (WaitForTestServerToStart(kMaxWaitTime, kNumIntervals)) {
    DVLOG(1) << "Started local test server at "
             << cl->GetSwitchValueASCII(switches::kSyncServiceURL) << ".";
    return true;
  } else {
    LOG(ERROR) << "Could not start local test server at "
               << cl->GetSwitchValueASCII(switches::kSyncServiceURL) << ".";
    return false;
  }
}

bool SyncTest::TearDownLocalPythonTestServer() {
  if (!sync_server_.Stop()) {
    LOG(ERROR) << "Could not stop local python test server.";
    return false;
  }
  xmpp_port_.reset();
  return true;
}

bool SyncTest::TearDownLocalTestServer() {
  if (test_server_handle_ != base::kNullProcessHandle) {
    EXPECT_TRUE(base::KillProcess(test_server_handle_, 0, false))
        << "Could not stop local test server.";
    base::CloseProcessHandle(test_server_handle_);
    test_server_handle_ = base::kNullProcessHandle;
  }
  return true;
}

bool SyncTest::WaitForTestServerToStart(int time_ms, int intervals) {
  for (int i = 0; i < intervals; ++i) {
    if (IsTestServerRunning())
      return true;
    base::PlatformThread::Sleep(time_ms / intervals);
  }
  return false;
}

bool SyncTest::IsTestServerRunning() {
  CommandLine* cl = CommandLine::ForCurrentProcess();
  std::string sync_url = cl->GetSwitchValueASCII(switches::kSyncServiceURL);
  GURL sync_url_status(sync_url.append("/healthz"));
  SyncServerStatusChecker delegate;
  scoped_ptr<content::URLFetcher> fetcher(content::URLFetcher::Create(
    sync_url_status, content::URLFetcher::GET, &delegate));
  fetcher->SetRequestContext(Profile::Deprecated::GetDefaultRequestContext());
  fetcher->Start();
  ui_test_utils::RunMessageLoop();
  return delegate.running();
}

void SyncTest::EnableNetwork(Profile* profile) {
  SetProxyConfig(profile->GetRequestContext(),
                 net::ProxyConfig::CreateDirect());
  // TODO(rsimha): Remove this line once http://crbug.com/53857 is fixed.
  net::NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
}

void SyncTest::DisableNetwork(Profile* profile) {
  // Set the current proxy configuration to a nonexistent proxy to effectively
  // disable networking.
  net::ProxyConfig config;
  config.proxy_rules().ParseFromString("http=127.0.0.1:0");
  SetProxyConfig(profile->GetRequestContext(), config);
  // TODO(rsimha): Remove this line once http://crbug.com/53857 is fixed.
  net::NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
}

bool SyncTest::EnableEncryption(int index, syncable::ModelType type) {
  return GetClient(index)->EnableEncryptionForType(type);
}

bool SyncTest::IsEncrypted(int index, syncable::ModelType type) {
  return GetClient(index)->IsTypeEncrypted(type);
}

bool SyncTest::AwaitQuiescence() {
  return ProfileSyncServiceHarness::AwaitQuiescence(clients());
}

bool SyncTest::ServerSupportsNotificationControl() const {
  EXPECT_NE(SERVER_TYPE_UNDECIDED, server_type_);

  // Supported only if we're using the python testserver.
  return server_type_ == LOCAL_PYTHON_SERVER;
}

void SyncTest::DisableNotifications() {
  ASSERT_TRUE(ServerSupportsNotificationControl());
  std::string path = "chromiumsync/disablenotifications";
  ui_test_utils::NavigateToURL(browser(), sync_server_.GetURL(path));
  ASSERT_EQ("Notifications disabled",
            UTF16ToASCII(browser()->GetSelectedTabContents()->GetTitle()));
}

void SyncTest::EnableNotifications() {
  ASSERT_TRUE(ServerSupportsNotificationControl());
  std::string path = "chromiumsync/enablenotifications";
  ui_test_utils::NavigateToURL(browser(), sync_server_.GetURL(path));
  ASSERT_EQ("Notifications enabled",
            UTF16ToASCII(browser()->GetSelectedTabContents()->GetTitle()));
}

void SyncTest::TriggerNotification(
    const syncable::ModelTypeSet& changed_types) {
  ASSERT_TRUE(ServerSupportsNotificationControl());
  const std::string& data =
      sync_notifier::P2PNotificationData("from_server",
                                         sync_notifier::NOTIFY_ALL,
                                         changed_types).ToString();
  const std::string& path =
      std::string("chromiumsync/sendnotification?channel=") +
      sync_notifier::kSyncP2PNotificationChannel + "&data=" + data;
  ui_test_utils::NavigateToURL(browser(), sync_server_.GetURL(path));
  ASSERT_EQ("Notification sent",
            UTF16ToASCII(browser()->GetSelectedTabContents()->GetTitle()));
}

bool SyncTest::ServerSupportsErrorTriggering() const {
  EXPECT_NE(SERVER_TYPE_UNDECIDED, server_type_);

  // Supported only if we're using the python testserver.
  return server_type_ == LOCAL_PYTHON_SERVER;
}

void SyncTest::TriggerMigrationDoneError(
    const syncable::ModelTypeSet& model_types) {
  ASSERT_TRUE(ServerSupportsErrorTriggering());
  std::string path = "chromiumsync/migrate";
  char joiner = '?';
  for (syncable::ModelTypeSet::const_iterator it = model_types.begin();
       it != model_types.end(); ++it) {
    path.append(base::StringPrintf("%ctype=%d", joiner,
        syncable::GetExtensionFieldNumberFromModelType(*it)));
    joiner = '&';
  }
  ui_test_utils::NavigateToURL(browser(), sync_server_.GetURL(path));
  ASSERT_EQ("Migration: 200",
            UTF16ToASCII(browser()->GetSelectedTabContents()->GetTitle()));
}

void SyncTest::TriggerBirthdayError() {
  ASSERT_TRUE(ServerSupportsErrorTriggering());
  std::string path = "chromiumsync/birthdayerror";
  ui_test_utils::NavigateToURL(browser(), sync_server_.GetURL(path));
  ASSERT_EQ("Birthday error",
            UTF16ToASCII(browser()->GetSelectedTabContents()->GetTitle()));
}

void SyncTest::TriggerTransientError() {
  ASSERT_TRUE(ServerSupportsErrorTriggering());
  std::string path = "chromiumsync/transienterror";
  ui_test_utils::NavigateToURL(browser(), sync_server_.GetURL(path));
  ASSERT_EQ("Transient error",
            UTF16ToASCII(browser()->GetSelectedTabContents()->GetTitle()));
}

namespace {

sync_pb::ClientToServerResponse::ErrorType
    GetClientToServerResponseErrorType(
        browser_sync::SyncProtocolErrorType error) {
  switch (error) {
    case browser_sync::SYNC_SUCCESS:
      return sync_pb::ClientToServerResponse::SUCCESS;
    case browser_sync::NOT_MY_BIRTHDAY:
      return sync_pb::ClientToServerResponse::NOT_MY_BIRTHDAY;
    case browser_sync::THROTTLED:
      return sync_pb::ClientToServerResponse::THROTTLED;
    case browser_sync::CLEAR_PENDING:
      return sync_pb::ClientToServerResponse::CLEAR_PENDING;
    case browser_sync::TRANSIENT_ERROR:
      return sync_pb::ClientToServerResponse::TRANSIENT_ERROR;
    case browser_sync::MIGRATION_DONE:
      return sync_pb::ClientToServerResponse::MIGRATION_DONE;
    case browser_sync::UNKNOWN_ERROR:
      return sync_pb::ClientToServerResponse::UNKNOWN;
    default:
      NOTREACHED();
      return sync_pb::ClientToServerResponse::UNKNOWN;
  }
}

sync_pb::ClientToServerResponse::Error::Action
    GetClientToServerResponseAction(
        const browser_sync::ClientAction& action) {
  switch (action) {
    case browser_sync::UPGRADE_CLIENT:
      return sync_pb::ClientToServerResponse::Error::UPGRADE_CLIENT;
    case browser_sync::CLEAR_USER_DATA_AND_RESYNC:
      return sync_pb::ClientToServerResponse::Error::CLEAR_USER_DATA_AND_RESYNC;
    case browser_sync::ENABLE_SYNC_ON_ACCOUNT:
      return sync_pb::ClientToServerResponse::Error::ENABLE_SYNC_ON_ACCOUNT;
    case browser_sync::STOP_AND_RESTART_SYNC:
      return sync_pb::ClientToServerResponse::Error::STOP_AND_RESTART_SYNC;
    case browser_sync::DISABLE_SYNC_ON_CLIENT:
      return sync_pb::ClientToServerResponse::Error::DISABLE_SYNC_ON_CLIENT;
    case browser_sync::UNKNOWN_ACTION:
      return sync_pb::ClientToServerResponse::Error::UNKNOWN_ACTION;
    default:
      NOTREACHED();
      return sync_pb::ClientToServerResponse::Error::UNKNOWN_ACTION;
  }
}

}  // namespace

void SyncTest::TriggerSyncError(const browser_sync::SyncProtocolError& error) {
  ASSERT_TRUE(ServerSupportsErrorTriggering());
  std::string path = "chromiumsync/error";
  int error_type =
      static_cast<int>(GetClientToServerResponseErrorType(
          error.error_type));
  int action = static_cast<int>(GetClientToServerResponseAction(
      error.action));

  path.append(base::StringPrintf("?error=%d", error_type));
  path.append(base::StringPrintf("&action=%d", action));

  path += "&error_description=" + error.error_description;
  path += "&url=" + error.url;

  ui_test_utils::NavigateToURL(browser(), sync_server_.GetURL(path));
  std::string output = UTF16ToASCII(
      browser()->GetSelectedTabContents()->GetTitle());
  ASSERT_TRUE(output.find("SetError: 200") != string16::npos);
}

void SyncTest::TriggerSetSyncTabs() {
  ASSERT_TRUE(ServerSupportsErrorTriggering());
  std::string path = "chromiumsync/synctabs";
  ui_test_utils::NavigateToURL(browser(), sync_server_.GetURL(path));
  ASSERT_EQ("Sync Tabs",
            UTF16ToASCII(browser()->GetSelectedTabContents()->GetTitle()));
}

int SyncTest::NumberOfDefaultSyncItems() const {
  // Just return the current number of basic sync items that are synced,
  // including preferences, themes, and search engines.
  // TODO(stevet): It would be nice if there was some mechanism for retrieving
  // this sum from each data type without having to manually count and update
  // this value.
  return 7;
}

void SyncTest::SetProxyConfig(net::URLRequestContextGetter* context_getter,
                              const net::ProxyConfig& proxy_config) {
  base::WaitableEvent done(false, false);
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&SetProxyConfigCallback, &done,
                 make_scoped_refptr(context_getter), proxy_config));
  done.Wait();
}
