// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_PROFILE_SYNC_SERVICE_HARNESS_H_
#define CHROME_BROWSER_SYNC_PROFILE_SYNC_SERVICE_HARNESS_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/sync/backend_migrator.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_observer.h"
#include "chrome/browser/sync/retry_verifier.h"
#include "sync/internal_api/public/base/model_type.h"

class Profile;

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
    : public ProfileSyncServiceObserver,
      public browser_sync::MigrationObserver {
 public:
  ProfileSyncServiceHarness(Profile* profile,
                            const std::string& username,
                            const std::string& password);

  virtual ~ProfileSyncServiceHarness();

  // Creates a ProfileSyncServiceHarness object and attaches it to |profile|, a
  // profile that is assumed to have been signed into sync in the past. Caller
  // takes ownership.
  static ProfileSyncServiceHarness* CreateAndAttach(Profile* profile);

  // Sets the GAIA credentials with which to sign in to sync.
  void SetCredentials(const std::string& username, const std::string& password);

  // Returns true if sync has been enabled on |profile_|.
  bool IsSyncAlreadySetup();

  // Creates a ProfileSyncService for the profile passed at construction and
  // enables sync for all available datatypes. Returns true only after sync has
  // been fully initialized and authenticated, and we are ready to process
  // changes.
  bool SetupSync();

  // Same as the above method, but enables sync only for the datatypes contained
  // in |synced_datatypes|.
  bool SetupSync(syncer::ModelTypeSet synced_datatypes);

  // Prepare for setting up to sync, but without clearing the existing items.
  bool InitializeSync();

  // Returns true if the sync client has no unsynced items.
  bool IsDataSynced();

  // ProfileSyncServiceObserver implementation.
  virtual void OnStateChanged() OVERRIDE;

  // MigrationObserver implementation.
  virtual void OnMigrationStateChange() OVERRIDE;

  // Blocks the caller until the sync backend host associated with this harness
  // has been initialized.  Returns true if the wait was successful.
  bool AwaitBackendInitialized();

  // Blocks the caller until the datatype manager is configured and sync has
  // been initialized (for example, after a browser restart).  Returns true if
  // the wait was successful.
  bool AwaitSyncRestart();

  // Blocks the caller until this harness has completed a single sync cycle
  // since the previous one.  Returns true if a sync cycle has completed.
  bool AwaitDataSyncCompletion(const std::string& reason);

  // Blocks the caller until this harness has completed as many sync cycles as
  // are required to ensure its progress marker matches the latest available on
  // the server.
  //
  // Note: When other clients are committing changes this will not be reliable.
  // If your test involves changes to multiple clients, you should use one of
  // the other Await* functions, such as AwaitMutualSyncCycleComplete.  Refer to
  // the documentation of those functions for more details.
  bool AwaitFullSyncCompletion(const std::string& reason);

  // Blocks the caller until sync has been disabled for this client. Returns
  // true if sync is disabled.
  bool AwaitSyncDisabled(const std::string& reason);

  // Blocks the caller until exponential backoff has been verified to happen.
  bool AwaitExponentialBackoffVerification();

  // Blocks the caller until the syncer receives an actionable error.
  // Returns true if the sync client received an actionable error.
  bool AwaitActionableError();

  // Blocks until the given set of data types are migrated.
  bool AwaitMigration(syncer::ModelTypeSet expected_migrated_types);

  // Blocks the caller until this harness has observed that the sync engine
  // has downloaded all the changes seen by the |partner| harness's client.
  bool WaitUntilProgressMarkersMatch(
      ProfileSyncServiceHarness* partner, const std::string& reason);

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
  ProfileSyncService* service() { return service_; }

  // Returns the status of the ProfileSyncService member of the sync client.
  ProfileSyncService::Status GetStatus();

  // See ProfileSyncService::ShouldPushChanges().
  bool ServiceIsPushingChanges() { return service_->ShouldPushChanges(); }

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

  // Encrypt the datatype |type|. This method will block while the sync backend
  // host performs the encryption or a timeout is reached.
  // PostCondition:
  //   returns: True if |type| was encrypted and we are fully synced.
  //            False if we timed out.
  bool EnableEncryptionForType(syncer::ModelType type);

  // Wait until |type| is encrypted or we time out.
  // PostCondition:
  //   returns: True if |type| is currently encrypted and we are fully synced.
  //            False if we timed out.
  bool WaitForTypeEncryption(syncer::ModelType type);

  // Check if |type| is encrypted.
  bool IsTypeEncrypted(syncer::ModelType type);

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

 private:
  friend class StateChangeTimeoutEvent;

  enum WaitState {
    // The sync client has just been initialized.
    INITIAL_WAIT_STATE = 0,

    // The sync client awaits the OnBackendInitialized() callback.
    WAITING_FOR_ON_BACKEND_INITIALIZED,

    // The sync client is waiting for the first sync cycle to complete.
    WAITING_FOR_INITIAL_SYNC,

    // The sync client is waiting for data to be synced.
    WAITING_FOR_DATA_SYNC,

    // The sync client is waiting for data and progress markers to be synced.
    WAITING_FOR_FULL_SYNC,

    // The sync client anticipates incoming updates leading to a new sync cycle.
    WAITING_FOR_UPDATES,

    // The sync client is waiting for a passphrase to be required by the
    // cryptographer.
    WAITING_FOR_PASSPHRASE_REQUIRED,

    // The sync client is waiting for its passphrase to be accepted by the
    // cryptographer.
    WAITING_FOR_PASSPHRASE_ACCEPTED,

    // The sync client anticipates encryption of new datatypes.
    WAITING_FOR_ENCRYPTION,

    // The sync client is waiting for the datatype manager to be configured and
    // for sync to be fully initialized. Used after a browser restart, where a
    // full sync cycle is not expected to occur.
    WAITING_FOR_SYNC_CONFIGURATION,

    // The sync client is waiting for sync to be disabled for this client.
    WAITING_FOR_SYNC_DISABLED,

    // The sync client is in the exponential backoff mode. Verify that
    // backoffs are triggered correctly.
    WAITING_FOR_EXPONENTIAL_BACKOFF_VERIFICATION,

    // The sync client is waiting for migration to start.
    WAITING_FOR_MIGRATION_TO_START,

    // The sync client is waiting for migration to finish.
    WAITING_FOR_MIGRATION_TO_FINISH,

    // The sync client is waiting for an actionable error from the server.
    WAITING_FOR_ACTIONABLE_ERROR,

    // The client verification is complete. We don't care about the state of
    // the syncer any more.
    WAITING_FOR_NOTHING,

    // The sync client needs a passphrase in order to decrypt data.
    SET_PASSPHRASE_FAILED,

    // The sync client's credentials were rejected.
    CREDENTIALS_REJECTED,

    // The sync client cannot reach the server.
    SERVER_UNREACHABLE,

    // The sync client is fully synced and there are no pending updates.
    FULLY_SYNCED,

    // Syncing is disabled for the client.
    SYNC_DISABLED,

    NUMBER_OF_STATES,
  };

  // Listen to migration events if the migrator has been initialized
  // and we're not already listening.  Returns true if we started
  // listening.
  bool TryListeningToMigrationEvents();

  // Called from the observer when the current wait state has been completed.
  void SignalStateCompleteWithNextState(WaitState next_state);

  // Indicates that the operation being waited on is complete.
  void SignalStateComplete();

  // Finite state machine for controlling state.  Returns true only if a state
  // change has taken place.
  bool RunStateChangeMachine();

  // Returns true if a status change took place, false on timeout.
  bool AwaitStatusChangeWithTimeout(int timeout_milliseconds,
                                    const std::string& reason);

  // A helper for implementing IsDataSynced() and IsFullySynced().
  bool IsDataSyncedImpl(
      const syncer::sessions::SyncSessionSnapshot& snapshot);

  // Returns true if the sync client has no unsynced items and its progress
  // markers are believed to be up to date.
  //
  // Although we can't detect when commits from other clients invalidate our
  // local progress markers, we do know when our own commits have invalidated
  // our timestmaps.  This check returns true when this client has, to the best
  // of its knowledge, downloaded the latest progress markers.
  bool IsFullySynced();

  // Returns true if there is a backend migration in progress.
  bool HasPendingBackendMigration();

  // Returns true if this client has downloaded all the items that the
  // other client has.
  bool MatchesOtherClient(ProfileSyncServiceHarness* partner);

  // Returns a string with relevant info about client's sync state (if
  // available), annotated with |message|. Useful for logging.
  std::string GetClientInfoString(const std::string& message);

  // Gets the current progress marker of the current sync session for a
  // particular datatype. Returns an empty string if the progress marker isn't
  // found.
  std::string GetSerializedProgressMarker(syncer::ModelType model_type) const;

  // Gets detailed status from |service_| in pretty-printable form.
  std::string GetServiceStatus();

  // When in WAITING_FOR_ENCRYPTION state, we check to see if this type is now
  // encrypted to determine if we're done.
  syncer::ModelType waiting_for_encryption_type_;

  // The WaitState in which the sync client currently is. Helps determine what
  // action to take when RunStateChangeMachine() is called.
  WaitState wait_state_;

  // Sync profile associated with this sync client.
  Profile* profile_;

  // ProfileSyncService object associated with |profile_|.
  ProfileSyncService* service_;

  // The harness of the client whose update progress marker we're expecting
  // eventually match.
  ProfileSyncServiceHarness* progress_marker_partner_;

  // Credentials used for GAIA authentication.
  std::string username_;
  std::string password_;

  // The current set of data types pending migration.  Used by
  // AwaitMigration().
  syncer::ModelTypeSet pending_migration_types_;

  // The set of data types that have undergone migration.  Used by
  // AwaitMigration().
  syncer::ModelTypeSet migrated_types_;

  // Used for logging.
  const std::string profile_debug_name_;

  // Keeps track of the number of attempts at exponential backoff and its
  // related bookkeeping information for verification.
  browser_sync::RetryVerifier retry_verifier_;

  // Flag set to true when we're waiting for a status change to happen. Used to
  // avoid triggering internal state machine logic on unexpected sync observer
  // callbacks.
  bool waiting_for_status_change_;

  DISALLOW_COPY_AND_ASSIGN(ProfileSyncServiceHarness);
};

#endif  // CHROME_BROWSER_SYNC_PROFILE_SYNC_SERVICE_HARNESS_H_
