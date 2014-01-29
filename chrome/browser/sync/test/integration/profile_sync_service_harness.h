// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_PROFILE_SYNC_SERVICE_HARNESS_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_PROFILE_SYNC_SERVICE_HARNESS_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_observer.h"
#include "sync/internal_api/public/base/model_type.h"

class Profile;
class StatusChangeChecker;

namespace invalidation {
class P2PInvalidationService;
}

namespace browser_sync {
namespace sessions {
class SyncSessionSnapshot;
}
}

// An instance of this class is basically our notion of a "sync client" for
// automation purposes. It harnesses the ProfileSyncService member of the
// profile passed to it on construction and automates certain things like setup
// and authentication. It provides ways to "wait" adequate periods of time for
// several clients to get to the same state.
class ProfileSyncServiceHarness
    : public ProfileSyncServiceObserver {
 public:
  static ProfileSyncServiceHarness* Create(
      Profile* profile,
      const std::string& username,
      const std::string& password);

  static ProfileSyncServiceHarness* CreateForIntegrationTest(
      Profile* profile,
      const std::string& username,
      const std::string& password,
      invalidation::P2PInvalidationService* invalidation_service);

  virtual ~ProfileSyncServiceHarness();

  // Sets the GAIA credentials with which to sign in to sync.
  void SetCredentials(const std::string& username, const std::string& password);

  // Returns true if sync is disabled for this client.
  bool IsSyncDisabled() const;

  // Returns true if an auth error has been encountered.
  bool HasAuthError() const;

  // Creates a ProfileSyncService for the profile passed at construction and
  // enables sync for all available datatypes. Returns true only after sync has
  // been fully initialized and authenticated, and we are ready to process
  // changes.
  bool SetupSync();

  // Same as the above method, but enables sync only for the datatypes contained
  // in |synced_datatypes|.
  bool SetupSync(syncer::ModelTypeSet synced_datatypes);

  // ProfileSyncServiceObserver implementation.
  virtual void OnStateChanged() OVERRIDE;
  virtual void OnSyncCycleCompleted() OVERRIDE;

  // Blocks the caller until the sync backend host associated with this harness
  // has been initialized.  Returns true if the wait was successful.
  bool AwaitBackendInitialized();

  // Blocks the caller until the client has nothing left to commit and its
  // progress markers are up to date. Returns true if successful.
  bool AwaitCommitActivityCompletion();

  // Blocks the caller until sync has been disabled for this client. Returns
  // true if sync is disabled.
  bool AwaitSyncDisabled();

  // Blocks the caller until sync setup is complete for this client. Returns
  // true if sync setup is complete.
  bool AwaitSyncSetupCompletion();

  // Blocks the caller until this harness has observed that the sync engine
  // has downloaded all the changes seen by the |partner| harness's client.
  bool WaitUntilProgressMarkersMatch(ProfileSyncServiceHarness* partner);

  // Calling this acts as a barrier and blocks the caller until |this| and
  // |partner| have both completed a sync cycle.  When calling this method,
  // the |partner| should be the passive responder who responds to the actions
  // of |this|.  This method relies upon the synchronization of callbacks
  // from the message queue. Returns true if two sync cycles have completed.
  // Note: Use this method when exactly one client makes local change(s), and
  // exactly one client is waiting to receive those changes.
  bool AwaitMutualSyncCycleCompletion(ProfileSyncServiceHarness* partner);

  // Blocks the caller until |this| completes its ongoing sync cycle and every
  // other client in |partners| have achieved identical download progresses.
  // Note: Use this method when exactly one client makes local change(s),
  // and more than one client is waiting to receive those changes.
  bool AwaitGroupSyncCycleCompletion(
      std::vector<ProfileSyncServiceHarness*>& partners);

  // Blocks the caller until every client in |clients| completes its ongoing
  // sync cycle and all the clients' progress markers match.  Note: Use this
  // method when more than one client makes local change(s), and more than one
  // client is waiting to receive those changes.
  static bool AwaitQuiescence(
      std::vector<ProfileSyncServiceHarness*>& clients);

  // Blocks the caller until |service_| indicates that a passphrase is required.
  bool AwaitPassphraseRequired();

  // Blocks the caller until |service_| indicates that the passphrase set by
  // calling SetDecryptionPassphrase has been accepted.
  bool AwaitPassphraseAccepted();

  // Returns the ProfileSyncService member of the sync client.
  ProfileSyncService* service() const { return service_; }

  // Returns the debug name for this profile. Used for logging.
  const std::string& profile_debug_name() const { return profile_debug_name_; }

  // Returns the status of the ProfileSyncService member of the sync client.
  ProfileSyncService::Status GetStatus() const;

  // See ProfileSyncService::ShouldPushChanges().
  bool ServiceIsPushingChanges() const { return service_->ShouldPushChanges(); }

  // Enables sync for a particular sync datatype. Returns true on success.
  bool EnableSyncForDatatype(syncer::ModelType datatype);

  // Disables sync for a particular sync datatype. Returns true on success.
  bool DisableSyncForDatatype(syncer::ModelType datatype);

  // Enables sync for all sync datatypes. Returns true on success.
  bool EnableSyncForAllDatatypes();

  // Disables sync for all sync datatypes. Returns true on success.
  bool DisableSyncForAllDatatypes();

  // Returns a snapshot of the current sync session.
  syncer::sessions::SyncSessionSnapshot GetLastSessionSnapshot() const;

  // Encrypts all datatypes. This method will block while the sync backend host
  // performs the encryption, or a timeout is reached. Returns true if
  // encryption is complete and we are fully synced, and false if we timed out.
  bool EnableEncryption();

  // Waits until encryption is complete for all datatypes. Returns true if
  // encryption is complete and we are fully synced, and false if we timed out.
  bool WaitForEncryption();

  // Returns true if encryption is complete for all datatypes, and false
  // otherwise.
  bool IsEncryptionComplete() const;

  // Check if |type| is registered and the controller is running.
  bool IsTypeRunning(syncer::ModelType type);

  // Check if |type| is being synced.
  bool IsTypePreferred(syncer::ModelType type);

  // Get the number of sync entries this client has. This includes all top
  // level or permanent items, and can include recently deleted entries.
  size_t GetNumEntries() const;

  // Get the number of sync datatypes registered (ignoring whatever state
  // they're in).
  size_t GetNumDatatypes() const;

  // Gets the |auto_start_enabled_| variable from the |service_|.
  bool AutoStartEnabled();

  // Runs the UI message loop and waits until the Run() method of |checker|
  // returns true, indicating that the status change we are waiting for has
  // taken place. Caller retains ownership of |checker|, which must outlive this
  // method. Returns true if the status change was observed. In case of a
  // timeout, we log the |source| of the call to this method, and return false.
  bool AwaitStatusChange(StatusChangeChecker* checker,
                         const std::string& source);

  // Returns a string that can be used as the value of an oauth2 refresh token.
  // This function guarantees that a different string is returned each time
  // it is called.
  std::string GenerateFakeOAuth2RefreshTokenString();

  // Returns a string with relevant info about client's sync state (if
  // available), annotated with |message|. Useful for logging.
  std::string GetClientInfoString(const std::string& message) const;

  // Returns true if this client has downloaded all the items that the
  // other client has.
  bool MatchesPartnerClient() const;

 private:
  ProfileSyncServiceHarness(
      Profile* profile,
      const std::string& username,
      const std::string& password,
      invalidation::P2PInvalidationService* invalidation_service);

  // Quits the current message loop. Called when the status change being waited
  // on has occurred, or in the event of a timeout.
  void QuitMessageLoop();

  // Signals that sync setup is complete, and that PSS may begin syncing.
  void FinishSyncSetup();

  // Gets the current progress marker of the current sync session for a
  // particular datatype. Returns an empty string if the progress marker isn't
  // found.
  std::string GetSerializedProgressMarker(syncer::ModelType model_type) const;

  // Returns true if a client has nothing left to commit and its progress
  // markers are up to date.
  bool HasLatestProgressMarkers() const;

  // Gets detailed status from |service_| in pretty-printable form.
  std::string GetServiceStatus();

  // Sync profile associated with this sync client.
  Profile* profile_;

  // ProfileSyncService object associated with |profile_|.
  ProfileSyncService* service_;

  // P2PInvalidationService associated with |profile_|.
  invalidation::P2PInvalidationService* p2p_invalidation_service_;

  // The harness of the client whose update progress marker we're expecting
  // eventually match.
  ProfileSyncServiceHarness* progress_marker_partner_;

  // Credentials used for GAIA authentication.
  std::string username_;
  std::string password_;

  // Number used by GenerateFakeOAuth2RefreshTokenString() to make sure that
  // all refresh tokens used in the tests are different.
  int oauth2_refesh_token_number_;

  // Used for logging.
  const std::string profile_debug_name_;

  // Keeps track of the state change on which we are waiting. PSSHarness can
  // wait on only one status change at a time.
  StatusChangeChecker* status_change_checker_;

  DISALLOW_COPY_AND_ASSIGN(ProfileSyncServiceHarness);
};

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_PROFILE_SYNC_SERVICE_HARNESS_H_
