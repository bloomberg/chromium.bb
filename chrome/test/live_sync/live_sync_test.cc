// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/live_sync/live_sync_test.h"

#include <vector>

#include "base/string_util.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/profile_manager.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"

namespace switches {
const std::string kPasswordFileForTest = "password-file-for-test";
const std::string kSyncUserForTest = "sync-user-for-test";
const std::string kSyncPasswordForTest = "sync-password-for-test";
}

void LiveSyncTest::SetUp() {
  // At this point, the browser hasn't been launched, and no services are
  // available.  But we can verify our command line parameters and fail
  // early.
  const CommandLine* cl = CommandLine::ForCurrentProcess();
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

  // Unless a sync server was explicitly provided, run a test one locally.
  // TODO(ncarter): It might be better to allow the user to specify a choice
  // of sync server "providers" -- a script that could locate (or allocate)
  // a sync server instance, possibly on some remote host.  The provider
  // would be invoked before each test.
  if (!cl->HasSwitch(switches::kSyncServiceURL))
    SetUpLocalTestServer();

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
    if (GetProfile(i) == NULL)
      return false;
    clients_.push_back(new ProfileSyncServiceTestHarness(
        GetProfile(i), username_, password_));
    if (GetClient(i) == NULL)
      return false;
  }

  // Create the verifier profile.
  verifier_.reset(MakeProfile(FILE_PATH_LITERAL("Verifier")));
  return (verifier_.get() != NULL);
}

bool LiveSyncTest::SetupSync() {
  // Create sync profiles and clients if they haven't already been created.
  if (profiles_.empty()) {
    if (!SetupClients())
      return false;
  }

  // Sync each of the profiles.
  for (int i = 0; i < num_clients_; ++i) {
    if (!GetClient(i)->SetupSync())
      return false;
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
  // Allow direct lookup of thawte.com.  On Linux, we use Chromium's nss
  // implementation which uses ocsp.thawte.com for certificate verification.
  // Without this, running the test case on Linux causes an error as we make an
  // external DNS lookup of "ocsp.thawte.com".
  resolver->AllowDirectLookup("*.thawte.com");
  mock_host_resolver_override_.reset(
      new net::ScopedDefaultHostResolverProc(resolver));
}

void LiveSyncTest::TearDownInProcessBrowserTestFixture() {
  mock_host_resolver_override_.reset();
}

void LiveSyncTest::SetUpLocalTestServer() {
  bool success = server_.Start(net::TestServerLauncher::ProtoHTTP,
      server_.kHostName, server_.kOKHTTPSPort,
      FilePath(), FilePath(), std::wstring());
  ASSERT_TRUE(success);

  started_local_test_server_ = true;

  CommandLine* cl = CommandLine::ForCurrentProcess();
  cl->AppendSwitchWithValue(switches::kSyncServiceURL,
      StringPrintf("http://%s:%d/chromiumsync", server_.kHostName,
          server_.kOKHTTPSPort));
}

void LiveSyncTest::TearDownLocalTestServer() {
  bool success = server_.Stop();
  ASSERT_TRUE(success);
}
