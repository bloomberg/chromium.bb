// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_SYNC_TEST_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_SYNC_TEST_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_request_status.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/protocol/sync_protocol_error.h"
#include "sync/test/fake_server/fake_server.h"
#include "sync/test/local_sync_test_server.h"

class Profile;
class ProfileSyncService;
class ProfileSyncServiceHarness;
class P2PInvalidationForwarder;

namespace base {
class CommandLine;
}

namespace fake_server {
class FakeServer;
class FakeServerInvalidationService;
}

namespace net {
class FakeURLFetcherFactory;
class ProxyConfig;
class ScopedDefaultHostResolverProc;
class URLFetcherImplFactory;
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

    // Tests that use one client profile and are not compatible with
    // FakeServer.
    // TODO(pvalenzuela): Delete this value when all SINGLE_CLIENT_LEGACY tests
    // are compatible with FakeServer and switched to SINGLE_CLIENT. See
    // crbug.com/323265.
    SINGLE_CLIENT_LEGACY,

    // Tests where two client profiles are synced with the server. Typically
    // functionality level tests.
    TWO_CLIENT,

    // Tests that use two client profiles and are not compatible with
    // FakeServer.
    // TODO(pvalenzuela): Delete this value when all TWO_CLIENT_LEGACY tests are
    // compatible with FakeServer and switched to TWO_CLIENT. See
    // crbug.com/323265.
    TWO_CLIENT_LEGACY,

    // Tests where three or more client profiles are synced with the server.
    // Typically, these tests create client side races and verify that sync
    // works.
    MULTIPLE_CLIENT
  };

  // The type of server we're running against.
  enum ServerType {
    SERVER_TYPE_UNDECIDED,
    LOCAL_PYTHON_SERVER,    // The mock python server that runs locally and is
                            // part of the Chromium checkout.
    LOCAL_LIVE_SERVER,      // Some other server (maybe the real binary used by
                            // Google's sync service) that can be started on
                            // a per-test basis by running a command
    EXTERNAL_LIVE_SERVER,   // A remote server that the test code has no control
                            // over whatsoever; cross your fingers that the
                            // account state is initially clean.
    IN_PROCESS_FAKE_SERVER, // The fake Sync server (FakeServer) running
                            // in-process (bypassing HTTP calls). This
                            // ServerType will eventually replace
                            // LOCAL_PYTHON_SERVER.
  };

  // NOTE: IMPORTANT the enum here should match with
  // the enum defined on the chromiumsync.py test server impl.
  enum SyncErrorFrequency {
    // Uninitialized state.
    ERROR_FREQUENCY_NONE,

    // Server sends the error on all requests.
    ERROR_FREQUENCY_ALWAYS,

    // Server sends the error on two thirds of the request.
    // Note this is not random. The server would send the
    // error on the first 2 requests of every 3 requests.
    ERROR_FREQUENCY_TWO_THIRDS
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
  virtual void SetUpCommandLine(base::CommandLine* cl) OVERRIDE;

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

  // Returns a ProfileSyncService at the given index.
  ProfileSyncService* GetSyncService(int index);

  // Returns the set of ProfileSyncServices.
  std::vector<ProfileSyncService*> GetSyncServices();

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

  // Sets whether or not the sync clients in this test should respond to
  // notifications of their own commits.  Real sync clients do not do this, but
  // many test assertions require this behavior.
  //
  // Default is to return true.  Test should override this if they require
  // different behavior.
  virtual bool TestUsesSelfNotifications();

  // Kicks off encryption for profile |index|.
  bool EnableEncryption(int index);

  // Checks if encryption is complete for profile |index|.
  bool IsEncryptionComplete(int index);

  // Waits until IsEncryptionComplete returns true or a timeout is reached.
  bool AwaitEncryptionComplete(int index);

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

  // Sets the mock gaia response for when an OAuth2 token is requested.
  // Each call to this method will overwrite responses that were previously set.
  void SetOAuth2TokenResponse(const std::string& response_data,
                              net::HttpStatusCode response_code,
                              net::URLRequestStatus::Status status);

  // Trigger a notification to be sent to all clients.  This operation
  // is available only if ServerSupportsNotificationControl() returned
  // true.
  void TriggerNotification(syncer::ModelTypeSet changed_types);

  // Returns true if the server being used supports injecting errors.
  bool ServerSupportsErrorTriggering() const;

  // Triggers a migration for one or more datatypes, and waits
  // for the server to complete it.  This operation is available
  // only if ServerSupportsErrorTriggering() returned true.
  void TriggerMigrationDoneError(syncer::ModelTypeSet model_types);

  // Triggers a transient error on the server. Note the server will stay in
  // this state until shut down.
  void TriggerTransientError();

  // Triggers an XMPP auth error on the server.  Note the server will
  // stay in this state until shut down.
  void TriggerXmppAuthError();

  // Triggers a sync error on the server.
  //   error: The error the server is expected to return.
  //   frequency: Frequency with which the error is returned.
  void TriggerSyncError(const syncer::SyncProtocolError& error,
                        SyncErrorFrequency frequency);

  // Triggers the creation the Synced Bookmarks folder on the server.
  void TriggerCreateSyncedBookmarks();

  // Returns the FakeServer being used for the test or NULL if FakeServer is
  // not being used.
  fake_server::FakeServer* GetFakeServer() const;

 protected:
  // Add custom switches needed for running the test.
  virtual void AddTestSwitches(base::CommandLine* cl);

  // Append the command line switches to enable experimental types that aren't
  // on by default yet.
  virtual void AddOptionalTypesToCommandLine(base::CommandLine* cl);

  // BrowserTestBase override. Destroys all the sync clients and sync
  // profiles created by a test.
  virtual void TearDownOnMainThread() OVERRIDE;

  // InProcessBrowserTest override. Changes behavior of the default host
  // resolver to avoid DNS lookup errors.
  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE;

  // InProcessBrowserTest override. Resets the host resolver its default
  // behavior.
  virtual void TearDownInProcessBrowserTestFixture() OVERRIDE;

  // Creates Profile, Browser and ProfileSyncServiceHarness instances for
  // |index|. Used by SetupClients().
  virtual void InitializeInstance(int index);

  // Implementations of the EnableNotifications() and DisableNotifications()
  // functions defined above.
  void DisableNotificationsImpl();
  void EnableNotificationsImpl();

  // GAIA account used by the test case.
  std::string username_;

  // GAIA password used by the test case.
  std::string password_;

  // Locally available plain text file in which GAIA credentials are stored.
  base::FilePath password_file_;

  // The FakeServer used in tests with server type IN_PROCESS_FAKE_SERVER.
  scoped_ptr<fake_server::FakeServer> fake_server_;

 private:
  // Helper to ProfileManager::CreateProfile that handles path creation.
  static Profile* MakeProfile(const base::FilePath::StringType name);

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

  // Helper method that waits for up to |wait| for the test server
  // to start. Splits the time into |intervals| intervals, and polls the
  // server after each interval to see if it has started. Returns true if
  // successful.
  bool WaitForTestServerToStart(base::TimeDelta wait, int intervals);

  // Helper method used to check if the test server is up and running.
  bool IsTestServerRunning();

  // Used to disable and enable network connectivity by providing and
  // clearing an invalid proxy configuration.
  void SetProxyConfig(net::URLRequestContextGetter* context,
                      const net::ProxyConfig& proxy_config);

  void SetupNetwork(net::URLRequestContextGetter* context);

  // Helper method used to set up fake responses for kClientLoginUrl,
  // kIssueAuthTokenUrl, kGetUserInfoUrl and kSearchDomainCheckUrl in order to
  // mock out calls to GAIA servers.
  void SetupMockGaiaResponses();

  // Helper method used to clear any fake responses that might have been set for
  // various gaia URLs, cancel any outstanding URL requests, and return to using
  // the default URLFetcher creation mechanism.
  void ClearMockGaiaResponses();

  // Decide which sync server implementation to run against based on the type
  // of test being run and command line args passed in.
  void DecideServerType();

  // Sets up the client-side invalidations infrastructure depending on the
  // value of |server_type_|.
  void InitializeInvalidations(int index);

  // Python sync test server, started on demand.
  syncer::LocalSyncTestServer sync_server_;

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

  // A set of objects to listen for commit activity and broadcast notifications
  // of this activity to its peer sync clients.
  ScopedVector<P2PInvalidationForwarder> invalidation_forwarders_;

  // Collection of pointers to FakeServerInvalidation objects for each profile.
  std::vector<fake_server::FakeServerInvalidationService*>
      fake_server_invalidation_services_;

  // Sync profile against which changes to individual profiles are verified. We
  // don't need a corresponding verifier sync client because the contents of the
  // verifier profile are strictly local, and are not meant to be synced.
  Profile* verifier_;

  // Indicates whether changes to a profile should also change the verifier
  // profile or not.
  bool use_verifier_;

  // Indicates whether or not notifications were explicitly enabled/disabled.
  // Defaults to true.
  bool notifications_enabled_;

  // Sync integration tests need to make live DNS requests for access to
  // GAIA and sync server URLs under google.com. We use a scoped version
  // to override the default resolver while the test is active.
  scoped_ptr<net::ScopedDefaultHostResolverProc> mock_host_resolver_override_;

  // Used to start and stop the local test server.
  base::ProcessHandle test_server_handle_;

  // Fake URLFetcher factory used to mock out GAIA signin.
  scoped_ptr<net::FakeURLFetcherFactory> fake_factory_;

  // The URLFetcherImplFactory instance used to instantiate |fake_factory_|.
  scoped_ptr<net::URLFetcherImplFactory> factory_;

  DISALLOW_COPY_AND_ASSIGN(SyncTest);
};

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_SYNC_TEST_H_
