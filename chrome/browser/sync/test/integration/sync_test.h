// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_SYNC_TEST_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_SYNC_TEST_H_

#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "base/process/process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/sync/base/model_type.h"
#include "components/sync/protocol/sync_protocol_error.h"
#include "components/sync/test/fake_server/fake_server.h"
#include "components/sync/test/local_sync_test_server.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_request_status.h"

// The E2E tests are designed to run against real backend servers. To identify
// those tests we use *E2ETest* test name filter and run disabled tests.
//
// The following macros define how a test is run:
// - E2E_ONLY: Marks a test to run only as an E2E test against backend servers.
//             These tests DO NOT run on regular Chromium waterfalls.
// - E2E_ENABLED: Marks a test to run as an E2E test in addition to Chromium
//                waterfalls.
//
// To disable a test from running on Chromium waterfalls, you would still use
// the default DISABLED_test_name macro. To disable it from running as an E2E
// test outside Chromium waterfalls you would need to remove the E2E* macro.
#define MACRO_CONCAT(prefix, test_name) prefix ## _ ## test_name
#define E2E_ONLY(test_name) MACRO_CONCAT(DISABLED_E2ETest, test_name)
#define E2E_ENABLED(test_name) MACRO_CONCAT(test_name, E2ETest)

class ProfileSyncServiceHarness;
class P2PInvalidationForwarder;
class P2PSyncRefresher;

namespace base {
class CommandLine;
class ScopedTempDir;
}  // namespace base

namespace browser_sync {
class ProfileSyncService;
}  // namespace browser_sync

namespace fake_server {
class FakeServer;
class FakeServerInvalidationService;
}  // namespace fake_server

namespace net {
class FakeURLFetcherFactory;
class ScopedDefaultHostResolverProc;
class URLFetcherImplFactory;
}  // namespace net

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
    TWO_CLIENT_LEGACY
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
    IN_PROCESS_FAKE_SERVER,  // The fake Sync server (FakeServer) running
                             // in-process (bypassing HTTP calls). This
                             // ServerType will eventually replace
                             // LOCAL_PYTHON_SERVER.
  };

  // A SyncTest must be associated with a particular test type.
  explicit SyncTest(TestType test_type);

  ~SyncTest() override;

  // Validates command line parameters and creates a local python test server if
  // specified.
  void SetUp() override;

  // Brings down local python test server if one was created.
  void TearDown() override;

  // Sets up command line flags required for sync tests.
  void SetUpCommandLine(base::CommandLine* cl) override;

  // Used to get the number of sync clients used by a test.
  int num_clients() WARN_UNUSED_RESULT { return num_clients_; }

  // Returns a pointer to a particular sync profile. Callee owns the object
  // and manages its lifetime.
  Profile* GetProfile(int index) WARN_UNUSED_RESULT;

  // Returns a list of all profiles including the verifier if available. Callee
  // owns the objects and manages its lifetime.
  std::vector<Profile*> GetAllProfiles();

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
  browser_sync::ProfileSyncService* GetSyncService(int index);

  // Returns the set of ProfileSyncServices.
  std::vector<browser_sync::ProfileSyncService*> GetSyncServices();

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

  // Returns true if we are running tests against external servers.
  bool UsingExternalServers();

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

  // Triggers an XMPP auth error on the server.  Note the server will
  // stay in this state until shut down.
  void TriggerXmppAuthError();

  // Triggers the creation the Synced Bookmarks folder on the server.
  void TriggerCreateSyncedBookmarks();

  // Returns the FakeServer being used for the test or null if FakeServer is
  // not being used.
  fake_server::FakeServer* GetFakeServer() const;

  // Triggers a sync for the given |model_types| for the Profile at |index|.
  void TriggerSyncForModelTypes(int index, syncer::ModelTypeSet model_types);

 protected:
  // Add custom switches needed for running the test.
  virtual void AddTestSwitches(base::CommandLine* cl);

  // Append the command line switches to enable experimental types that aren't
  // on by default yet.
  virtual void AddOptionalTypesToCommandLine(base::CommandLine* cl);

  // BrowserTestBase override. Destroys all the sync clients and sync
  // profiles created by a test.
  void TearDownOnMainThread() override;

  // InProcessBrowserTest override. Changes behavior of the default host
  // resolver to avoid DNS lookup errors.
  void SetUpInProcessBrowserTestFixture() override;

  // InProcessBrowserTest override. Resets the host resolver its default
  // behavior.
  void TearDownInProcessBrowserTestFixture() override;

  // Implementations of the EnableNotifications() and DisableNotifications()
  // functions defined above.
  void DisableNotificationsImpl();
  void EnableNotificationsImpl();

  // If non-empty, |contents| will be written to a profile's Preferences file
  // before the Profile object is created.
  void SetPreexistingPreferencesFileContents(const std::string& contents);

  // Helper to ProfileManager::CreateProfileAsync that creates a new profile
  // used for UI Signin. Blocks until profile is created.
  static Profile* MakeProfileForUISignin(base::FilePath profile_path);

  // GAIA account used by the test case.
  std::string username_;

  // GAIA password used by the test case.
  std::string password_;

  // Locally available plain text file in which GAIA credentials are stored.
  base::FilePath password_file_;

  // The FakeServer used in tests with server type IN_PROCESS_FAKE_SERVER.
  std::unique_ptr<fake_server::FakeServer> fake_server_;

 private:
  // Handles Profile creation for given index. Profile's path and type is
  // determined at runtime based on server type.
  void CreateProfile(int index);

  // Callback for MakeProfileForUISignin() method. It runs the quit_closure once
  // profile is created successfully.
  static void CreateProfileCallback(const base::Closure& quit_closure,
                                    Profile* profile,
                                    Profile::CreateStatus status);

  // Helper to Profile::CreateProfile that handles path creation. It creates
  // a profile then registers it as a testing profile.
  Profile* MakeTestProfile(base::FilePath profile_path, int index);

  // Helper method used to create a Gaia account at runtime.
  // This function should only be called when running against external servers
  // which support this functionality.
  // Returns true if account creation was successful, false otherwise.
  bool CreateGaiaAccount(const std::string& username,
                         const std::string& password);

  // Helper to block the current thread while the data models sync depends on
  // finish loading.
  void WaitForDataModels(Profile* profile);

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

  // Initializes any custom services needed for the |profile| at |index|.
  void InitializeProfile(int index, Profile* profile);

  // Sets up the client-side invalidations infrastructure depending on the
  // value of |server_type_|.
  void InitializeInvalidations(int index);

  // Python sync test server, started on demand.
  syncer::LocalSyncTestServer sync_server_;

  // Helper class to whitelist the notification port.
  std::unique_ptr<net::ScopedPortException> xmpp_port_;

  // Used to differentiate between single-client and two-client tests as well
  // as wher the in-process FakeServer is used.
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

  // Collection of profile delegates. Only used for test profiles, which require
  // a custom profile delegate to ensure initialization happens at the right
  // time.
  std::vector<std::unique_ptr<Profile::Delegate>> profile_delegates_;

  // Collection of profile paths used by a test. Each profile has a unique path
  // which should be cleaned up at test shutdown. Profile paths inside the
  // default user data dir gets deleted at InProcessBrowserTest teardown.
  std::vector<base::ScopedTempDir*> tmp_profile_paths_;

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

  // A set of objects to listen for commit activity and broadcast refresh
  // notifications of this activity to its peer sync clients.
  ScopedVector<P2PSyncRefresher> sync_refreshers_;

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

  // Indicates the need to create Gaia user account at runtime. This can only be
  // set if tests are run against external servers with support for user
  // creation via http requests.
  bool create_gaia_account_at_runtime_;

  // Sync integration tests need to make live DNS requests for access to
  // GAIA and sync server URLs under google.com. We use a scoped version
  // to override the default resolver while the test is active.
  std::unique_ptr<net::ScopedDefaultHostResolverProc>
      mock_host_resolver_override_;

  // Used to start and stop the local test server.
  base::Process test_server_;

  // Fake URLFetcher factory used to mock out GAIA signin.
  std::unique_ptr<net::FakeURLFetcherFactory> fake_factory_;

  // The URLFetcherImplFactory instance used to instantiate |fake_factory_|.
  std::unique_ptr<net::URLFetcherImplFactory> factory_;

  // The contents to be written to a profile's Preferences file before the
  // Profile object is created. If empty, no preexisting file will be written.
  std::string preexisting_preferences_file_contents_;

  DISALLOW_COPY_AND_ASSIGN(SyncTest);
};

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_SYNC_TEST_H_
