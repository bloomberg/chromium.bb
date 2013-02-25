// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/google/google_url_tracker.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/password_manager/encryptor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/sync_datatype_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_browser_thread.h"
#include "google_apis/gaia/gaia_urls.h"
#include "googleurl/src/gurl.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/base/network_change_notifier.h"
#include "net/proxy/proxy_config.h"
#include "net/proxy/proxy_config_service_fixed.h"
#include "net/proxy/proxy_service.h"
#include "net/test/test_server.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"
#include "sync/engine/sync_scheduler_impl.h"
#include "sync/notifier/p2p_invalidator.h"
#include "sync/protocol/sync.pb.h"

using content::BrowserThread;

namespace switches {
const char kPasswordFileForTest[] = "password-file-for-test";
const char kSyncUserForTest[] = "sync-user-for-test";
const char kSyncPasswordForTest[] = "sync-password-for-test";
const char kSyncServerCommandLine[] = "sync-server-command-line";
}

// Helper class that checks whether a sync test server is running or not.
class SyncServerStatusChecker : public net::URLFetcherDelegate {
 public:
  SyncServerStatusChecker() : running_(false) {}

  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE {
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
    : test_type_(test_type),
      server_type_(SERVER_TYPE_UNDECIDED),
      num_clients_(-1),
      use_verifier_(true),
      notifications_enabled_(true),
      test_server_handle_(base::kNullProcessHandle),
      number_of_default_sync_items_(0) {
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
  // Clear any mock gaia responses that might have been set.
  ClearMockGaiaResponses();

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

  // TODO(sync): remove this once keystore encryption is enabled by default.
  if (!cl->HasSwitch(switches::kSyncKeystoreEncryption))
    cl->AppendSwitch(switches::kSyncKeystoreEncryption);

  if (!cl->HasSwitch(switches::kSyncShortInitialRetryOverride))
    cl->AppendSwitch(switches::kSyncShortInitialRetryOverride);
}

void SyncTest::AddOptionalTypesToCommandLine(CommandLine* cl) {}

// static
Profile* SyncTest::MakeProfile(const base::FilePath::StringType name) {
  base::FilePath path;
  PathService::Get(chrome::DIR_USER_DATA, &path);
  path = path.Append(name);

  if (!file_util::PathExists(path))
    CHECK(file_util::CreateDirectory(path));

  Profile* profile =
      Profile::CreateProfile(path, NULL, Profile::CREATE_MODE_SYNCHRONOUS);
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
  profiles_.resize(num_clients_);
  browsers_.resize(num_clients_);
  clients_.resize(num_clients_);
  for (int i = 0; i < num_clients_; ++i) {
    InitializeInstance(i);
  }

  // Create the verifier profile.
  verifier_ = MakeProfile(FILE_PATH_LITERAL("Verifier"));
  ui_test_utils::WaitForBookmarkModelToLoad(
      BookmarkModelFactory::GetForProfile(verifier()));
  ui_test_utils::WaitForHistoryToLoad(HistoryServiceFactory::GetForProfile(
      verifier(), Profile::EXPLICIT_ACCESS));
  ui_test_utils::WaitForTemplateURLServiceToLoad(
      TemplateURLServiceFactory::GetForProfile(verifier()));
  return (verifier_ != NULL);
}

void SyncTest::InitializeInstance(int index) {
  profiles_[index] = MakeProfile(
      base::StringPrintf(FILE_PATH_LITERAL("Profile%d"), index));
  EXPECT_FALSE(GetProfile(index) == NULL) << "Could not create Profile "
                                          << index << ".";

  browsers_[index] = new Browser(Browser::CreateParams(
      GetProfile(index), chrome::HOST_DESKTOP_TYPE_NATIVE));
  EXPECT_FALSE(GetBrowser(index) == NULL) << "Could not create Browser "
                                          << index << ".";

  // Make sure the ProfileSyncService has been created before creating the
  // ProfileSyncServiceHarness - some tests expect the ProfileSyncService to
  // already exist.
  ProfileSyncServiceFactory::GetForProfile(GetProfile(index));

  clients_[index] = new ProfileSyncServiceHarness(GetProfile(index),
                                                  username_,
                                                  password_);
  EXPECT_FALSE(GetClient(index) == NULL) << "Could not create Client "
                                         << index << ".";

  ui_test_utils::WaitForBookmarkModelToLoad(
      BookmarkModelFactory::GetForProfile(GetProfile(index)));
  ui_test_utils::WaitForHistoryToLoad(HistoryServiceFactory::GetForProfile(
      GetProfile(index), Profile::EXPLICIT_ACCESS));
  ui_test_utils::WaitForTemplateURLServiceToLoad(
      TemplateURLServiceFactory::GetForProfile(GetProfile(index)));
}

void SyncTest::RestartSyncService(int index) {
  DVLOG(1) << "Restarting profile sync service for profile " << index << ".";
  delete clients_[index];
  Profile* profile = GetProfile(index);
  ProfileSyncService* service =
      ProfileSyncServiceFactory::GetForProfile(profile);
  service->ResetForTest();
  clients_[index] = new ProfileSyncServiceHarness(profile,
                                                  username_,
                                                  password_);
  service->Initialize();
  GetClient(index)->AwaitSyncRestart();
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

  // Because clients may modify sync data as part of startup (for example local
  // session-releated data is rewritten), we need to ensure all startup-based
  // changes have propagated between the clients.
  AwaitQuiescence();

  // The number of default entries is the number of entries existing after
  // sync startup excluding top level folders and other permanent items.
  // This value must be updated whenever new permanent items are added (although
  // this should handle new datatype-specific top level folders).
  number_of_default_sync_items_ = GetClient(0)->GetNumEntries() -
                                  GetClient(0)->GetNumDatatypes() - 6;
  DVLOG(1) << "Setting " << number_of_default_sync_items_ << " as default "
           << " number of entries.";

  return true;
}

void SyncTest::CleanUpOnMainThread() {
  // Some of the pending messages might rely on browser windows still being
  // around, so run messages both before and after closing all browsers.
  content::RunAllPendingInMessageLoop();
  // Close all browser windows.
  chrome::CloseAllBrowsers();
  content::RunAllPendingInMessageLoop();

  // All browsers should be closed at this point, or else we could see memory
  // corruption in QuitBrowser().
  CHECK_EQ(0U, chrome::GetTotalBrowserCount());
  clients_.clear();
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
  factory_.reset(new net::URLFetcherImplFactory());
  fake_factory_.reset(new net::FakeURLFetcherFactory(factory_.get()));
  fake_factory_->SetFakeResponse(
      GaiaUrls::GetInstance()->client_login_url(),
      "SID=sid\nLSID=lsid",
      true);
  fake_factory_->SetFakeResponse(
      GaiaUrls::GetInstance()->get_user_info_url(),
      "email=user@gmail.com\ndisplayEmail=user@gmail.com",
      true);
  fake_factory_->SetFakeResponse(
      GaiaUrls::GetInstance()->issue_auth_token_url(),
      "auth",
      true);
  fake_factory_->SetFakeResponse(
      GoogleURLTracker::kSearchDomainCheckURL,
      ".google.com",
      true);
  fake_factory_->SetFakeResponse(
      GaiaUrls::GetInstance()->client_login_to_oauth2_url(),
      "some_response",
      true);
  fake_factory_->SetFakeResponse(
      GaiaUrls::GetInstance()->oauth2_token_url(),
      "{"
      "  \"refresh_token\": \"rt1\","
      "  \"access_token\": \"at1\","
      "  \"expires_in\": 3600,"
      "  \"token_type\": \"Bearer\""
      "}",
      true);
  fake_factory_->SetFakeResponse(
      GaiaUrls::GetInstance()->client_oauth_url(),
      "{"
      "  \"oauth2\": {"
      "    \"refresh_token\": \"rt1\","
      "    \"access_token\": \"at1\","
      "    \"expires_in\": 3600,"
      "    \"token_type\": \"Bearer\""
      "  }"
      "}",
      true);
  fake_factory_->SetFakeResponse(
      GaiaUrls::GetInstance()->oauth1_login_url(),
      "SID=sid\nLSID=lsid\nAuth=auth_token",
      true);
}

void SyncTest::ClearMockGaiaResponses() {
  // Clear any mock gaia responses that might have been set.
  if (fake_factory_.get()) {
    fake_factory_->ClearFakeResponses();
    fake_factory_.reset();
  }

  // Cancel any outstanding URL fetches and destroy the URLFetcherImplFactory we
  // created.
  net::URLFetcher::CancelAll();
  factory_.reset();
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

  if (!cl->HasSwitch(switches::kSyncNotificationHostPort)) {
    cl->AppendSwitchASCII(switches::kSyncNotificationHostPort,
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

  const base::TimeDelta kMaxWaitTime = TestTimeouts::action_max_timeout();
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

bool SyncTest::WaitForTestServerToStart(base::TimeDelta wait, int intervals) {
  for (int i = 0; i < intervals; ++i) {
    if (IsTestServerRunning())
      return true;
    base::PlatformThread::Sleep(wait / intervals);
  }
  return false;
}

bool SyncTest::IsTestServerRunning() {
  CommandLine* cl = CommandLine::ForCurrentProcess();
  std::string sync_url = cl->GetSwitchValueASCII(switches::kSyncServiceURL);
  GURL sync_url_status(sync_url.append("/healthz"));
  SyncServerStatusChecker delegate;
  scoped_ptr<net::URLFetcher> fetcher(net::URLFetcher::Create(
    sync_url_status, net::URLFetcher::GET, &delegate));
  fetcher->SetLoadFlags(net::LOAD_DISABLE_CACHE |
                        net::LOAD_DO_NOT_SEND_COOKIES |
                        net::LOAD_DO_NOT_SAVE_COOKIES);
  fetcher->SetRequestContext(g_browser_process->system_request_context());
  fetcher->Start();
  content::RunMessageLoop();
  return delegate.running();
}

void SyncTest::EnableNetwork(Profile* profile) {
  SetProxyConfig(profile->GetRequestContext(),
                 net::ProxyConfig::CreateDirect());
  if (notifications_enabled_) {
    EnableNotificationsImpl();
  }
  // TODO(rsimha): Remove this line once http://crbug.com/53857 is fixed.
  net::NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
}

void SyncTest::DisableNetwork(Profile* profile) {
  DisableNotificationsImpl();
  // Set the current proxy configuration to a nonexistent proxy to effectively
  // disable networking.
  net::ProxyConfig config;
  config.proxy_rules().ParseFromString("http=127.0.0.1:0");
  SetProxyConfig(profile->GetRequestContext(), config);
  // TODO(rsimha): Remove this line once http://crbug.com/53857 is fixed.
  net::NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
}

bool SyncTest::EnableEncryption(int index, syncer::ModelType type) {
  return GetClient(index)->EnableEncryptionForType(type);
}

bool SyncTest::IsEncrypted(int index, syncer::ModelType type) {
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

void SyncTest::DisableNotificationsImpl() {
  ASSERT_TRUE(ServerSupportsNotificationControl());
  std::string path = "chromiumsync/disablenotifications";
  ui_test_utils::NavigateToURL(browser(), sync_server_.GetURL(path));
  ASSERT_EQ("Notifications disabled",
            UTF16ToASCII(browser()->tab_strip_model()->GetActiveWebContents()->
                GetTitle()));
}

void SyncTest::DisableNotifications() {
  DisableNotificationsImpl();
  notifications_enabled_ = false;
}

void SyncTest::EnableNotificationsImpl() {
  ASSERT_TRUE(ServerSupportsNotificationControl());
  std::string path = "chromiumsync/enablenotifications";
  ui_test_utils::NavigateToURL(browser(), sync_server_.GetURL(path));
  ASSERT_EQ("Notifications enabled",
            UTF16ToASCII(browser()->tab_strip_model()->GetActiveWebContents()->
                GetTitle()));
}

void SyncTest::EnableNotifications() {
  EnableNotificationsImpl();
  notifications_enabled_ = true;
}

void SyncTest::TriggerNotification(syncer::ModelTypeSet changed_types) {
  ASSERT_TRUE(ServerSupportsNotificationControl());
  const std::string& data =
      syncer::P2PNotificationData(
          "from_server",
          syncer::NOTIFY_ALL,
          syncer::ObjectIdSetToInvalidationMap(
              syncer::ModelTypeSetToObjectIdSet(changed_types), std::string())
          ).ToString();
  const std::string& path =
      std::string("chromiumsync/sendnotification?channel=") +
      syncer::kSyncP2PNotificationChannel + "&data=" + data;
  ui_test_utils::NavigateToURL(browser(), sync_server_.GetURL(path));
  ASSERT_EQ("Notification sent",
            UTF16ToASCII(browser()->tab_strip_model()->GetActiveWebContents()->
                GetTitle()));
}

bool SyncTest::ServerSupportsErrorTriggering() const {
  EXPECT_NE(SERVER_TYPE_UNDECIDED, server_type_);

  // Supported only if we're using the python testserver.
  return server_type_ == LOCAL_PYTHON_SERVER;
}

void SyncTest::TriggerMigrationDoneError(syncer::ModelTypeSet model_types) {
  ASSERT_TRUE(ServerSupportsErrorTriggering());
  std::string path = "chromiumsync/migrate";
  char joiner = '?';
  for (syncer::ModelTypeSet::Iterator it = model_types.First();
       it.Good(); it.Inc()) {
    path.append(
        base::StringPrintf(
            "%ctype=%d", joiner,
            syncer::GetSpecificsFieldNumberFromModelType(it.Get())));
    joiner = '&';
  }
  ui_test_utils::NavigateToURL(browser(), sync_server_.GetURL(path));
  ASSERT_EQ("Migration: 200",
            UTF16ToASCII(browser()->tab_strip_model()->GetActiveWebContents()->
                GetTitle()));
}

void SyncTest::TriggerBirthdayError() {
  ASSERT_TRUE(ServerSupportsErrorTriggering());
  std::string path = "chromiumsync/birthdayerror";
  ui_test_utils::NavigateToURL(browser(), sync_server_.GetURL(path));
  ASSERT_EQ("Birthday error",
            UTF16ToASCII(browser()->tab_strip_model()->GetActiveWebContents()->
                GetTitle()));
}

void SyncTest::TriggerTransientError() {
  ASSERT_TRUE(ServerSupportsErrorTriggering());
  std::string path = "chromiumsync/transienterror";
  ui_test_utils::NavigateToURL(browser(), sync_server_.GetURL(path));
  ASSERT_EQ("Transient error",
            UTF16ToASCII(browser()->tab_strip_model()->GetActiveWebContents()->
                GetTitle()));
}

void SyncTest::TriggerAuthError() {
  ASSERT_TRUE(ServerSupportsErrorTriggering());
  std::string path = "chromiumsync/cred";
  ui_test_utils::NavigateToURL(browser(), sync_server_.GetURL(path));
}

void SyncTest::TriggerXmppAuthError() {
  ASSERT_TRUE(ServerSupportsErrorTriggering());
  std::string path = "chromiumsync/xmppcred";
  ui_test_utils::NavigateToURL(browser(), sync_server_.GetURL(path));
}

namespace {

sync_pb::SyncEnums::ErrorType
    GetClientToServerResponseErrorType(
        syncer::SyncProtocolErrorType error) {
  switch (error) {
    case syncer::SYNC_SUCCESS:
      return sync_pb::SyncEnums::SUCCESS;
    case syncer::NOT_MY_BIRTHDAY:
      return sync_pb::SyncEnums::NOT_MY_BIRTHDAY;
    case syncer::THROTTLED:
      return sync_pb::SyncEnums::THROTTLED;
    case syncer::CLEAR_PENDING:
      return sync_pb::SyncEnums::CLEAR_PENDING;
    case syncer::TRANSIENT_ERROR:
      return sync_pb::SyncEnums::TRANSIENT_ERROR;
    case syncer::MIGRATION_DONE:
      return sync_pb::SyncEnums::MIGRATION_DONE;
    case syncer::UNKNOWN_ERROR:
      return sync_pb::SyncEnums::UNKNOWN;
    default:
      NOTREACHED();
      return sync_pb::SyncEnums::UNKNOWN;
  }
}

sync_pb::SyncEnums::Action GetClientToServerResponseAction(
    const syncer::ClientAction& action) {
  switch (action) {
    case syncer::UPGRADE_CLIENT:
      return sync_pb::SyncEnums::UPGRADE_CLIENT;
    case syncer::CLEAR_USER_DATA_AND_RESYNC:
      return sync_pb::SyncEnums::CLEAR_USER_DATA_AND_RESYNC;
    case syncer::ENABLE_SYNC_ON_ACCOUNT:
      return sync_pb::SyncEnums::ENABLE_SYNC_ON_ACCOUNT;
    case syncer::STOP_AND_RESTART_SYNC:
      return sync_pb::SyncEnums::STOP_AND_RESTART_SYNC;
    case syncer::DISABLE_SYNC_ON_CLIENT:
      return sync_pb::SyncEnums::DISABLE_SYNC_ON_CLIENT;
    case syncer::UNKNOWN_ACTION:
      return sync_pb::SyncEnums::UNKNOWN_ACTION;
    default:
      NOTREACHED();
      return sync_pb::SyncEnums::UNKNOWN_ACTION;
  }
}

}  // namespace

void SyncTest::TriggerSyncError(const syncer::SyncProtocolError& error,
                                SyncErrorFrequency frequency) {
  ASSERT_TRUE(ServerSupportsErrorTriggering());
  std::string path = "chromiumsync/error";
  int error_type =
      static_cast<int>(GetClientToServerResponseErrorType(
          error.error_type));
  int action = static_cast<int>(GetClientToServerResponseAction(
      error.action));

  path.append(base::StringPrintf("?error=%d", error_type));
  path.append(base::StringPrintf("&action=%d", action));

  path.append(base::StringPrintf("&error_description=%s",
                                 error.error_description.c_str()));
  path.append(base::StringPrintf("&url=%s", error.url.c_str()));
  path.append(base::StringPrintf("&frequency=%d", frequency));

  ui_test_utils::NavigateToURL(browser(), sync_server_.GetURL(path));
  std::string output = UTF16ToASCII(
      browser()->tab_strip_model()->GetActiveWebContents()->GetTitle());
  ASSERT_TRUE(output.find("SetError: 200") != string16::npos);
}

void SyncTest::TriggerCreateSyncedBookmarks() {
  ASSERT_TRUE(ServerSupportsErrorTriggering());
  std::string path = "chromiumsync/createsyncedbookmarks";
  ui_test_utils::NavigateToURL(browser(), sync_server_.GetURL(path));
  ASSERT_EQ("Synced Bookmarks",
            UTF16ToASCII(browser()->tab_strip_model()->GetActiveWebContents()->
                GetTitle()));
}

int SyncTest::NumberOfDefaultSyncItems() const {
  return number_of_default_sync_items_;
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
