// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_LIVE_SYNC_LIVE_SYNC_TEST_H_
#define CHROME_TEST_LIVE_SYNC_LIVE_SYNC_TEST_H_
#pragma once

#include "chrome/test/in_process_browser_test.h"

#include "base/basictypes.h"
#include "base/file_util.h"
#include "base/process_util.h"
#include "base/scoped_ptr.h"
#include "base/scoped_vector.h"
#include "net/base/mock_host_resolver.h"
#include "net/test/test_server.h"

#include <string>
#include <vector>

class CommandLine;
class Profile;
class ProfileSyncServiceHarness;
class URLRequestContextGetter;
class FakeURLFetcherFactory;

namespace net {
class ProxyConfig;
class ScopedDefaultHostResolverProc;
}

// This is the base class for integration tests for all sync data types. Derived
// classes must be defined for each sync data type. Individual tests are defined
// using the IN_PROC_BROWSER_TEST_F macro.
class LiveSyncTest : public InProcessBrowserTest {
 public:
  // The different types of live sync tests that can be implemented.
  enum TestType {
    // Tests where only one client profile is synced with the server. Typically
    // sanity level tests.
    SINGLE_CLIENT,

    // Tests where two client profiles are synced with the server. Typically
    // functionality level tests.
    TWO_CLIENT,

    // Tests where three or more client profiles are synced with the server.
    // Typically, these tests create client side races and verify that sync
    // works.
    MULTIPLE_CLIENT,

    // Tests where several client profiles are synced with the server. Only used
    // by stress tests.
    MANY_CLIENT
  };

  // A LiveSyncTest must be associated with a particular test type.
  explicit LiveSyncTest(TestType test_type);

  virtual ~LiveSyncTest();

  // Validates command line parameters and creates a local python test server if
  // specified.
  virtual void SetUp();

  // Brings down local python test server if one was created.
  virtual void TearDown();

  // Sets up command line flags required for sync tests.
  virtual void SetUpCommandLine(CommandLine* cl);

  // Used to get the number of sync clients used by a test.
  int num_clients() WARN_UNUSED_RESULT { return num_clients_; }

  // Returns a pointer to a particular sync profile. Callee owns the object
  // and manages its lifetime.
  Profile* GetProfile(int index) WARN_UNUSED_RESULT;

  // Returns a pointer to a particular sync client. Callee owns the object
  // and manages its lifetime.
  ProfileSyncServiceHarness* GetClient(int index) WARN_UNUSED_RESULT;

  // Returns a reference to the collection of sync clients. Callee owns the
  // object and manages its lifetime.
  std::vector<ProfileSyncServiceHarness*>& clients() WARN_UNUSED_RESULT {
    return clients_.get();
  }

  // Returns a pointer to the sync profile that is used to verify changes to
  // individual sync profiles. Callee owns the object and manages its lifetime.
  Profile* verifier() WARN_UNUSED_RESULT;

  // Initializes sync clients and profiles but does not sync any of them.
  virtual bool SetupClients() WARN_UNUSED_RESULT;

  // Initializes sync clients and profiles if required and syncs each of them.
  virtual bool SetupSync() WARN_UNUSED_RESULT;

  // Enable outgoing network connections for the given profile.
  virtual void EnableNetwork(Profile* profile);

  // Disable outgoing network connections for the given profile.
  virtual void DisableNetwork(Profile* profile);

  // Blocks until all sync clients have completed their mutual sync cycles.
  // Returns true if a quiescent state was successfully reached.
  bool AwaitQuiescence();

 protected:
  // InProcessBrowserTest override. Destroys all the sync clients and sync
  // profiles created by a test.
  virtual void CleanUpOnMainThread();

  // InProcessBrowserTest override. Changes behavior of the default host
  // resolver to avoid DNS lookup errors.
  virtual void SetUpInProcessBrowserTestFixture();

  // InProcessBrowserTest override. Resets the host resolver its default
  // behavior.
  virtual void TearDownInProcessBrowserTestFixture();

  // GAIA account used by the test case.
  std::string username_;

  // GAIA password used by the test case.
  std::string password_;

  // Locally available plain text file in which GAIA credentials are stored.
  FilePath password_file_;

 private:
  // Helper to ProfileManager::CreateProfile that handles path creation.
  static Profile* MakeProfile(const FilePath::StringType name);

  // Helper method used to read GAIA credentials from a local password file
  // specified via the "--password-file-for-test" command line switch.
  // Note: The password file must be a plain text file with exactly two lines --
  // the username on the first line and the password on the second line.
  void ReadPasswordFile();

  // Helper method that starts up a sync test server if required.
  void SetUpTestServerIfRequired();

  // Helper method used to start up a local python sync test server. Returns
  // true if successful.
  bool SetUpLocalPythonTestServer();

  // Helper method used to start up a local sync test server. Returns true if
  // successful.
  bool SetUpLocalTestServer();

  // Helper method used to destroy the local python sync test server if one was
  // created. Returns true if successful.
  bool TearDownLocalPythonTestServer();

  // Helper method used to destroy the local sync test server if one was
  // created. Returns true if successful.
  bool TearDownLocalTestServer();

  // Helper method that waits for up to |time_ms| milliseconds for the test
  // server to start. Splits the time into |intervals| intervals, and polls the
  // server after each interval to see if it has started. Returns true if
  // successful.
  bool WaitForTestServerToStart(int time_ms, int intervals);

  // Helper method used to check if the test server is up and running.
  bool IsTestServerRunning();

  // Used to disable and enable network connectivity by providing and
  // clearing an invalid proxy configuration.
  void SetProxyConfig(URLRequestContextGetter* context,
                      const net::ProxyConfig& proxy_config);

  // Helper method used to set up fake responses for kClientLoginUrl,
  // kIssueAuthTokenUrl, kGetUserInfoUrl and kSearchDomainCheckUrl in order to
  // mock out calls to GAIA servers.
  void SetupMockGaiaResponses();

  // Test server of type sync, started on demand.
  net::TestServer sync_server_;

  // Helper class to whitelist the notification port.
  scoped_ptr<net::ScopedPortException> xmpp_port_;

  // Used to differentiate between single-client, two-client, multi-client and
  // many-client tests.
  TestType test_type_;

  // Number of sync clients that will be created by a test.
  int num_clients_;

  // Collection of sync profiles used by a test. A sync profile maintains sync
  // data contained within its own subdirectory under the chrome user data
  // directory.
  ScopedVector<Profile> profiles_;

  // Collection of sync clients used by a test. A sync client is associated with
  // a sync profile, and implements methods that sync the contents of the
  // profile with the server.
  ScopedVector<ProfileSyncServiceHarness> clients_;

  // Sync profile against which changes to individual profiles are verified. We
  // don't need a corresponding verifier sync client because the contents of the
  // verifier profile are strictly local, and are not meant to be synced.
  scoped_ptr<Profile> verifier_;

  // Sync integration tests need to make live DNS requests for access to
  // GAIA and sync server URLs under google.com. We use a scoped version
  // to override the default resolver while the test is active.
  scoped_ptr<net::ScopedDefaultHostResolverProc> mock_host_resolver_override_;

  // Used to start and stop the local test server.
  base::ProcessHandle test_server_handle_;

  // Fake URLFetcher factory used to mock out GAIA signin.
  scoped_ptr<FakeURLFetcherFactory> factory_;

  DISALLOW_COPY_AND_ASSIGN(LiveSyncTest);
};

#endif  // CHROME_TEST_LIVE_SYNC_LIVE_SYNC_TEST_H_
