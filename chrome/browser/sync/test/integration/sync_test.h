// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_SYNC_TEST_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_SYNC_TEST_H_
#pragma once

#include "chrome/test/base/in_process_browser_test.h"

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/process_util.h"
#include "base/task.h"
#include "chrome/browser/sync/protocol/sync_protocol_error.h"
#include "chrome/browser/sync/syncable/model_type.h"
#include "net/base/mock_host_resolver.h"
#include "net/test/test_server.h"

class CommandLine;
class Profile;
class ProfileSyncServiceHarness;
class FakeURLFetcherFactory;
class URLFetcherImplFactory;

namespace net {
class ProxyConfig;
class ScopedDefaultHostResolverProc;
class URLRequestContextGetter;
}

// This is the base class for integration tests for all sync data types. Derived
// classes must be defined for each sync data type. Individual tests are defined
// using the IN_PROC_BROWSER_TEST_F macro.
class SyncTest : public InProcessBrowserTest {
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
    MULTIPLE_CLIENT
  };

  // The type of server we're running against.
  enum ServerType {
    SERVER_TYPE_UNDECIDED,
    LOCAL_PYTHON_SERVER,   // The mock python server that runs locally and is
                           // part of the Chromium checkout.
    LOCAL_LIVE_SERVER,     // Some other server (maybe the real binary used by
                           // Google's sync service) that can be started on
                           // a per-test basis by running a command
    EXTERNAL_LIVE_SERVER,  // A remote server that the test code has no control
                           // over whatsoever; cross your fingers that the
                           // account state is initially clean.
  };

  // A SyncTest must be associated with a particular test type.
  explicit SyncTest(TestType test_type);

  virtual ~SyncTest();

  // Validates command line parameters and creates a local python test server if
  // specified.
  virtual void SetUp() OVERRIDE;

  // Brings down local python test server if one was created.
  virtual void TearDown() OVERRIDE;

  // Sets up command line flags required for sync tests.
  virtual void SetUpCommandLine(CommandLine* cl) OVERRIDE;

  // Used to get the number of sync clients used by a test.
  int num_clients() WARN_UNUSED_RESULT { return num_clients_; }

  // Returns a pointer to a particular sync profile. Callee owns the object
  // and manages its lifetime.
  Profile* GetProfile(int index) WARN_UNUSED_RESULT;

  // Returns a pointer to a particular browser. Callee owns the object
  // and manages its lifetime.
  Browser* GetBrowser(int index) WARN_UNUSED_RESULT;

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

  // Used to determine whether the verifier profile should be updated or not.
  bool use_verifier() WARN_UNUSED_RESULT { return use_verifier_; }

  // After calling this method, changes made to a profile will no longer be
  // reflected in the verifier profile. Note: Not all datatypes use this.
  // TODO(rsimha): Hook up all datatypes to this mechanism.
  void DisableVerifier();

  // Initializes sync clients and profiles but does not sync any of them.
  virtual bool SetupClients() WARN_UNUSED_RESULT;

  // Initializes sync clients and profiles if required and syncs each of them.
  virtual bool SetupSync() WARN_UNUSED_RESULT;

  // Enable outgoing network connections for the given profile.
  virtual void EnableNetwork(Profile* profile);

  // Disable outgoing network connections for the given profile.
  virtual void DisableNetwork(Profile* profile);

  // Encrypts the datatype |type| for profile |index|.
  bool EnableEncryption(int index, syncable::ModelType type);

  // Checks if the datatype |type| is encrypted for profile |index|.
  bool IsEncrypted(int index, syncable::ModelType type);

  // Blocks until all sync clients have completed their mutual sync cycles.
  // Returns true if a quiescent state was successfully reached.
  bool AwaitQuiescence();

  // Returns true if the server being used supports controlling
  // notifications.
  bool ServerSupportsNotificationControl() const;

  // Disable notifications on the server.  This operation is available
  // only if ServerSupportsNotificationControl() returned true.
  void DisableNotifications();

  // Enable notifications on the server.  This operation is available
  // only if ServerSupportsNotificationControl() returned true.
  void EnableNotifications();

  // Trigger a notification to be sent to all clients.  This operation
  // is available only if ServerSupportsNotificationControl() returned
  // true.
  void TriggerNotification(const syncable::ModelTypeSet& changed_types);

  // Returns true if the server being used supports injecting errors.
  bool ServerSupportsErrorTriggering() const;

  // Triggers a migration for one or more datatypes, and waits
  // for the server to complete it.  This operation is available
  // only if ServerSupportsErrorTriggering() returned true.
  void TriggerMigrationDoneError(const syncable::ModelTypeSet& model_types);

  // Triggers the server to set its birthday to a random value thereby
  // the server would return a birthday error on next sync.
  void TriggerBirthdayError();

  // Triggers a transient error on the server. Note the server will stay in
  // this state until shut down.
  void TriggerTransientError();

  // Triggers a sync error on the server.
  void TriggerSyncError(const browser_sync::SyncProtocolError& error);

  // Triggers setting the sync_tabs field of the nigori node.
  void TriggerSetSyncTabs();

  // Returns the number of default items that every client syncs.
  int NumberOfDefaultSyncItems() const;

 protected:
  // Add custom switches needed for running the test.
  virtual void AddTestSwitches(CommandLine* cl);

  // Append the command line switches to enable experimental types that aren't
  // on by default yet.
  virtual void AddOptionalTypesToCommandLine(CommandLine* cl);

  // InProcessBrowserTest override. Destroys all the sync clients and sync
  // profiles created by a test.
  virtual void CleanUpOnMainThread() OVERRIDE;

  // InProcessBrowserTest override. Changes behavior of the default host
  // resolver to avoid DNS lookup errors.
  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE;

  // InProcessBrowserTest override. Resets the host resolver its default
  // behavior.
  virtual void TearDownInProcessBrowserTestFixture() OVERRIDE;

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

  // Helper method used to start up a local python test server. Note: We set up
  // an XMPP-only python server if |server_type_| is LOCAL_LIVE_SERVER and mock
  // gaia credentials are in use. Returns true if successful.
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
  void SetProxyConfig(net::URLRequestContextGetter* context,
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

  // Tells us what kind of server we're using (some tests run only on certain
  // server types).
  ServerType server_type_;

  // Number of sync clients that will be created by a test.
  int num_clients_;

  // Collection of sync profiles used by a test. A sync profile maintains sync
  // data contained within its own subdirectory under the chrome user data
  // directory. Profiles are owned by the ProfileManager.
  std::vector<Profile*> profiles_;

  // Collection of pointers to the browser objects used by a test. One browser
  // instance is created for each sync profile. Browser object lifetime is
  // managed by BrowserList, so we don't use a ScopedVector here.
  std::vector<Browser*> browsers_;

  // Collection of sync clients used by a test. A sync client is associated with
  // a sync profile, and implements methods that sync the contents of the
  // profile with the server.
  ScopedVector<ProfileSyncServiceHarness> clients_;

  // Sync profile against which changes to individual profiles are verified. We
  // don't need a corresponding verifier sync client because the contents of the
  // verifier profile are strictly local, and are not meant to be synced.
  Profile* verifier_;

  // Indicates whether changes to a profile should also change the verifier
  // profile or not.
  bool use_verifier_;

  // Sync integration tests need to make live DNS requests for access to
  // GAIA and sync server URLs under google.com. We use a scoped version
  // to override the default resolver while the test is active.
  scoped_ptr<net::ScopedDefaultHostResolverProc> mock_host_resolver_override_;

  // Used to start and stop the local test server.
  base::ProcessHandle test_server_handle_;

  // Fake URLFetcher factory used to mock out GAIA signin.
  scoped_ptr<FakeURLFetcherFactory> fake_factory_;

  // The URLFetcherImplFactory instance used to instantiate |fake_factory_|.
  scoped_ptr<URLFetcherImplFactory> factory_;

  DISALLOW_COPY_AND_ASSIGN(SyncTest);
};

DISABLE_RUNNABLE_METHOD_REFCOUNT(SyncTest);

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_SYNC_TEST_H_
