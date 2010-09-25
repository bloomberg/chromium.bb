// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/live_sync/live_sync_test.h"

#include <vector>

#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/task.h"
#include "base/waitable_event.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/password_manager/encryptor.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/profile_manager.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/net/url_fetcher.h"
#include "chrome/common/net/url_request_context_getter.h"
#include "chrome/test/testing_browser_process.h"
#include "googleurl/src/gurl.h"
#include "net/base/escape.h"
#include "net/base/network_change_notifier.h"
#include "net/proxy/proxy_config.h"
#include "net/proxy/proxy_config_service_fixed.h"
#include "net/proxy/proxy_service.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_status.h"

namespace switches {
const std::string kPasswordFileForTest = "password-file-for-test";
const std::string kSyncUserForTest = "sync-user-for-test";
const std::string kSyncPasswordForTest = "sync-password-for-test";
}

class ConfigureURLFectcherDelegate : public URLFetcher::Delegate {
 public:
  ConfigureURLFectcherDelegate() : success_(false) {}

  virtual void OnURLFetchComplete(const URLFetcher* source,
                                  const GURL& url,
                                  const URLRequestStatus& status,
                                  int response_code,
                                  const ResponseCookies& cookies,
                                  const std::string& data) {
    if (status.status() == URLRequestStatus::SUCCESS && response_code == 200)
      success_ = true;
    MessageLoop::current()->Quit();
  }

  bool success() const { return success_; }

 private:
  bool success_;
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

void LiveSyncTest::SetUp() {
  // At this point, the browser hasn't been launched, and no services are
  // available.  But we can verify our command line parameters and fail
  // early.
  CommandLine* cl = CommandLine::ForCurrentProcess();
  if (cl->HasSwitch(switches::kPasswordFileForTest)) {
    // Read GAIA credentials from a local password file if specified via the
    // "--password-file-for-test" command line switch. Note: The password file
    // must be a plain text file with exactly two lines -- the username on the
    // first line and the password on the second line.
    password_file_ = cl->GetSwitchValuePath(switches::kPasswordFileForTest);
    ASSERT_FALSE(password_file_.empty()) << "Can't run live server test "
        << "without specifying --" << switches::kPasswordFileForTest
        << "=<filename>";
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
  } else {
    // Read GAIA credentials from the "--sync-XXX-for-test" command line
    // parameters.
    username_ = cl->GetSwitchValueASCII(switches::kSyncUserForTest);
    password_ = cl->GetSwitchValueASCII(switches::kSyncPasswordForTest);
    ASSERT_FALSE(username_.empty()) << "Can't run live server test "
        << "without specifying --" << switches::kSyncUserForTest;
    ASSERT_FALSE(password_.empty()) << "Can't run live server test "
        << "without specifying --" << switches::kSyncPasswordForTest;
  }

  // TODO(rsimha): Until we implement a fake Tango server against which tests
  // can run, we need to set the --sync-notification-method to "transitional".
  if (!cl->HasSwitch(switches::kSyncNotificationMethod)) {
    cl->AppendSwitchASCII(switches::kSyncNotificationMethod,
                          "transitional");
  }

  // TODO(akalin): Delete this block of code once a local python notification
  // server is implemented.
  // The chrome sync builders are behind a firewall that blocks port 5222, the
  // default port for XMPP notifications. This causes the tests to spend up to a
  // minute waiting for a connection on port 5222 before they fail over to port
  // 443, the default SSL/TCP port. This switch causes the tests to use port 443
  // by default, without having to try port 5222.
  if (!cl->HasSwitch(switches::kSyncUseSslTcp)) {
    cl->AppendSwitch(switches::kSyncUseSslTcp);
  }

  // Unless a sync server was explicitly provided, run a test one locally.
  // TODO(ncarter): It might be better to allow the user to specify a choice
  // of sync server "providers" -- a script that could locate (or allocate)
  // a sync server instance, possibly on some remote host.  The provider
  // would be invoked before each test.
  if (!cl->HasSwitch(switches::kSyncServiceURL))
    SetUpLocalTestServer();

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

  // Stop the local test sync server if one was used.
  if (started_local_test_server_)
    TearDownLocalTestServer();
}

// static
Profile* LiveSyncTest::MakeProfile(const FilePath::StringType name) {
  FilePath path;
  PathService::Get(chrome::DIR_USER_DATA, &path);
  return ProfileManager::CreateProfile(path.Append(name));
}

Profile* LiveSyncTest::GetProfile(int index) {
  EXPECT_FALSE(profiles_.empty()) << "SetupSync() has not yet been called.";
  EXPECT_TRUE((index >= 0) && (index < static_cast<int>(profiles_.size())))
      << "GetProfile(): Index is out of bounds.";
  return profiles_[index];
}

ProfileSyncServiceTestHarness* LiveSyncTest::GetClient(int index) {
  EXPECT_FALSE(clients_.empty()) << "SetupClients() has not yet been called.";
  EXPECT_TRUE((index >= 0) && (index < static_cast<int>(clients_.size())))
      << "GetClient(): Index is out of bounds.";
  return clients_[index];
}

Profile* LiveSyncTest::verifier() {
  EXPECT_TRUE(verifier_.get() != NULL)
      << "SetupClients() has not yet been called.";
  return verifier_.get();
}

bool LiveSyncTest::SetupClients() {
  EXPECT_GT(num_clients_, 0) << "num_clients_ incorrectly initialized.";
  EXPECT_TRUE(profiles_.empty()) << "SetupClients() has already been called.";
  EXPECT_TRUE(clients_.empty()) << "SetupClients() has already been called.";

  // Create the required number of sync profiles and clients.
  for (int i = 0; i < num_clients_; ++i) {
    profiles_.push_back(MakeProfile(
        StringPrintf(FILE_PATH_LITERAL("Profile%d"), i)));
    EXPECT_FALSE(GetProfile(i) == NULL) << "GetProfile(" << i << ") failed.";
    clients_.push_back(new ProfileSyncServiceTestHarness(
        GetProfile(i), username_, password_, i));
    EXPECT_FALSE(GetClient(i) == NULL) << "GetClient(" << i << ") failed.";
  }

  // Create the verifier profile.
  verifier_.reset(MakeProfile(FILE_PATH_LITERAL("Verifier")));
  return (verifier_.get() != NULL);
}

bool LiveSyncTest::SetupSync() {
  // Create sync profiles and clients if they haven't already been created.
  if (profiles_.empty()) {
    EXPECT_TRUE(SetupClients()) << "SetupClients() failed.";
  }

  // Sync each of the profiles.
  for (int i = 0; i < num_clients_; ++i) {
    EXPECT_TRUE(GetClient(i)->SetupSync()) << "SetupSync() failed.";
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
}

void LiveSyncTest::SetUpLocalTestServer() {
  ASSERT_TRUE(test_server_.Start());

  started_local_test_server_ = true;

  CommandLine* cl = CommandLine::ForCurrentProcess();
  cl->AppendSwitchASCII(switches::kSyncServiceURL,
      StringPrintf("http://%s:%d/chromiumsync",
                   test_server_.host_port_pair().host().c_str(),
                   test_server_.host_port_pair().port()));

  // TODO(akalin): Set the kSyncNotificationHost switch here once a local python
  // notification server is implemented.
}

void LiveSyncTest::TearDownLocalTestServer() {
  ASSERT_TRUE(test_server_.Stop());
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

void LiveSyncTest::SetProxyConfig(URLRequestContextGetter* context_getter,
                                  const net::ProxyConfig& proxy_config) {
  base::WaitableEvent done(false, false);
  ChromeThread::PostTask(
      ChromeThread::IO,
      FROM_HERE,
      new SetProxyConfigTask(&done,
                             context_getter,
                             proxy_config));
  done.Wait();
}
