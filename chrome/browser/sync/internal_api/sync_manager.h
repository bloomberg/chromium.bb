// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_INTERNAL_API_SYNC_MANAGER_H_
#define CHROME_BROWSER_SYNC_INTERNAL_API_SYNC_MANAGER_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/threading/thread_checker.h"
#include "chrome/browser/sync/internal_api/change_record.h"
#include "chrome/browser/sync/internal_api/configure_reason.h"
#include "chrome/browser/sync/protocol/sync_protocol_error.h"
#include "chrome/browser/sync/syncable/model_type.h"
#include "chrome/browser/sync/util/weak_handle.h"
#include "chrome/common/net/gaia/google_service_auth_error.h"

class FilePath;

namespace browser_sync {
class JsBackend;
class JsEventHandler;
class ModelSafeWorkerRegistrar;

namespace sessions {
struct SyncSessionSnapshot;
}  // namespace sessions
}  // namespace browser_sync

namespace sync_notifier {
class SyncNotifier;
}  // namespace sync_notifier

namespace sync_api {

class BaseTransaction;
class HttpPostProviderFactory;
struct UserShare;

// Reasons due to which browser_sync::Cryptographer might require a passphrase.
enum PassphraseRequiredReason {
  REASON_PASSPHRASE_NOT_REQUIRED = 0,  // Initial value.
  REASON_ENCRYPTION = 1,               // The cryptographer requires a
                                       // passphrase for its first attempt at
                                       // encryption. Happens only during
                                       // migration or upgrade.
  REASON_DECRYPTION = 2,               // The cryptographer requires a
                                       // passphrase for its first attempt at
                                       // decryption.
  REASON_SET_PASSPHRASE_FAILED = 3,    // The cryptographer requires a new
                                       // passphrase because its attempt at
                                       // decryption with the cached passphrase
                                       // was unsuccessful.
};

// Contains everything needed to talk to and identify a user account.
struct SyncCredentials {
  std::string email;
  std::string sync_token;
};

// SyncManager encapsulates syncable::DirectoryManager and serves as
// the parent of all other objects in the sync API.  If multiple
// threads interact with the same local sync repository (i.e. the same
// sqlite database), they should share a single SyncManager instance.
// The caller should typically create one SyncManager for the lifetime
// of a user session.
//
// Unless stated otherwise, all methods of SyncManager should be
// called on the same thread.
class SyncManager {
 public:
  // SyncInternal contains the implementation of SyncManager, while abstracting
  // internal types from clients of the interface.
  class SyncInternal;

  // Status encapsulates detailed state about the internals of the SyncManager.
  struct Status {
    // Summary is a distilled set of important information that the end-user may
    // wish to be informed about (through UI, for example). Note that if a
    // summary state requires user interaction (such as auth failures), more
    // detailed information may be contained in additional status fields.
    enum Summary {
      // The internal instance is in an unrecognizable state. This should not
      // happen.
      INVALID = 0,
      // Can't connect to server, but there are no pending changes in
      // our local cache.
      OFFLINE,
      // Can't connect to server, and there are pending changes in our
      // local cache.
      OFFLINE_UNSYNCED,
      // Connected and syncing.
      SYNCING,
      // Connected, no pending changes.
      READY,
      // Internal sync error.
      CONFLICT,
      // Can't connect to server, and we haven't completed the initial
      // sync yet.  So there's nothing we can do but wait for the server.
      OFFLINE_UNUSABLE,

      SUMMARY_STATUS_COUNT,
    };

    Status();
    ~Status();

    Summary summary;
    bool authenticated;      // Successfully authenticated via GAIA.
    bool server_up;          // True if we have received at least one good
                             // reply from the server.
    bool server_reachable;   // True if we received any reply from the server.
    bool server_broken;      // True of the syncer is stopped because of server
                             // issues.
    bool notifications_enabled;  // True only if subscribed for notifications.

    // Notifications counters updated by the actions in synapi.
    int notifications_received;
    int notifiable_commits;

    // The max number of consecutive errors from any component.
    int max_consecutive_errors;

    browser_sync::SyncProtocolError sync_protocol_error;

    int unsynced_count;

    int conflicting_count;
    bool syncing;
    // True after a client has done a first sync.
    bool initial_sync_ended;
    // True if any syncer is stuck.
    bool syncer_stuck;

    // Total updates available.  If zero, nothing left to download.
    int64 updates_available;
    // Total updates received by the syncer since browser start.
    int updates_received;

    // Of updates_received, how many were tombstones.
    int tombstone_updates_received;
    bool disk_full;

    // Total number of overwrites due to conflict resolver since browser start.
    int num_local_overwrites_total;
    int num_server_overwrites_total;

    // Count of empty and non empty getupdates;
    int nonempty_get_updates;
    int empty_get_updates;

    // Count of useless and useful syncs we perform.
    int useless_sync_cycles;
    int useful_sync_cycles;

    // Encryption related.
    syncable::ModelTypeSet encrypted_types;
    bool cryptographer_ready;
    bool crypto_has_pending_keys;

    // The unique identifer for this client.
    std::string unique_id;
  };

  // An interface the embedding application implements to be notified
  // on change events.  Note that these methods may be called on *any*
  // thread.
  class ChangeDelegate {
   public:
    // Notify the delegate that changes have been applied to the sync model.
    //
    // This will be invoked on the same thread as on which ApplyChanges was
    // called. |changes| is an array of size |change_count|, and contains the
    // ID of each individual item that was changed. |changes| exists only for
    // the duration of the call. If items of multiple data types change at
    // the same time, this method is invoked once per data type and |changes|
    // is restricted to items of the ModelType indicated by |model_type|.
    // Because the observer is passed a |trans|, the observer can assume a
    // read lock on the sync model that will be released after the function
    // returns.
    //
    // The SyncManager constructs |changes| in the following guaranteed order:
    //
    // 1. Deletions, from leaves up to parents.
    // 2. Updates to existing items with synced parents & predecessors.
    // 3. New items with synced parents & predecessors.
    // 4. Items with parents & predecessors in |changes|.
    // 5. Repeat #4 until all items are in |changes|.
    //
    // Thus, an implementation of OnChangesApplied should be able to
    // process the change records in the order without having to worry about
    // forward dependencies.  But since deletions come before reparent
    // operations, a delete may temporarily orphan a node that is
    // updated later in the list.
    virtual void OnChangesApplied(
        syncable::ModelType model_type,
        const BaseTransaction* trans,
        const ImmutableChangeRecordList& changes) = 0;

    // OnChangesComplete gets called when the TransactionComplete event is
    // posted (after OnChangesApplied finishes), after the transaction lock
    // and the change channel mutex are released.
    //
    // The purpose of this function is to support processors that require
    // split-transactions changes. For example, if a model processor wants to
    // perform blocking I/O due to a change, it should calculate the changes
    // while holding the transaction lock (from within OnChangesApplied), buffer
    // those changes, let the transaction fall out of scope, and then commit
    // those changes from within OnChangesComplete (postponing the blocking
    // I/O to when it no longer holds any lock).
    virtual void OnChangesComplete(syncable::ModelType model_type) = 0;

   protected:
    virtual ~ChangeDelegate();
  };

  // Like ChangeDelegate, except called only on the sync thread and
  // not while a transaction is held.  For objects that want to know
  // when changes happen, but don't need to process them.
  class ChangeObserver {
   public:
    // Ids referred to in |changes| may or may not be in the write
    // transaction specified by |write_transaction_id|.  If they're
    // not, that means that the node didn't actually change, but we
    // marked them as changed for some other reason (e.g., siblings of
    // re-ordered nodes).
    //
    // TODO(sync, long-term): Ideally, ChangeDelegate/Observer would
    // be passed a transformed version of EntryKernelMutation instead
    // of a transaction that would have to be used to look up the
    // changed nodes.  That is, ChangeDelegate::OnChangesApplied()
    // would still be called under the transaction, but all the needed
    // data will be passed down.
    //
    // Even more ideally, we would have sync semantics such that we'd
    // be able to apply changes without being under a transaction.
    // But that's a ways off...
    virtual void OnChangesApplied(
        syncable::ModelType model_type,
        int64 write_transaction_id,
        const ImmutableChangeRecordList& changes) = 0;

    virtual void OnChangesComplete(syncable::ModelType model_type) = 0;

   protected:
    virtual ~ChangeObserver();
  };

  // An interface the embedding application implements to receive
  // notifications from the SyncManager.  Register an observer via
  // SyncManager::AddObserver.  All methods are called only on the
  // sync thread.
  class Observer {
   public:
    // A round-trip sync-cycle took place and the syncer has resolved any
    // conflicts that may have arisen.
    virtual void OnSyncCycleCompleted(
        const browser_sync::sessions::SyncSessionSnapshot* snapshot) = 0;

    // Called when user interaction may be required due to an auth problem.
    virtual void OnAuthError(const GoogleServiceAuthError& auth_error) = 0;

    // Called when a new auth token is provided by the sync server.
    virtual void OnUpdatedToken(const std::string& token) = 0;

    // Called when user interaction is required to obtain a valid passphrase.
    // - If the passphrase is required for encryption, |reason| will be
    //   REASON_ENCRYPTION.
    // - If the passphrase is required for the decryption of data that has
    //   already been encrypted, |reason| will be REASON_DECRYPTION.
    // - If the passphrase is required because decryption failed, and a new
    //   passphrase is required, |reason| will be REASON_SET_PASSPHRASE_FAILED.
    virtual void OnPassphraseRequired(PassphraseRequiredReason reason) = 0;

    // Called when the passphrase provided by the user has been accepted and is
    // now used to encrypt sync data.  |bootstrap_token| is an opaque base64
    // encoded representation of the key generated by the accepted passphrase,
    // and is provided to the observer for persistence purposes and use in a
    // future initialization of sync (e.g. after restart).
    virtual void OnPassphraseAccepted(const std::string& bootstrap_token) = 0;

    // Called when initialization is complete to the point that SyncManager can
    // process changes. This does not necessarily mean authentication succeeded
    // or that the SyncManager is online.
    // IMPORTANT: Creating any type of transaction before receiving this
    // notification is illegal!
    // WARNING: Calling methods on the SyncManager before receiving this
    // message, unless otherwise specified, produces undefined behavior.
    //
    // |js_backend| is what about:sync interacts with.  It can emit
    // the following events:

    /**
     * @param {{ enabled: boolean }} details A dictionary containing:
     *     - enabled: whether or not notifications are enabled.
     */
    // function onNotificationStateChange(details);

    /**
     * @param {{ changedTypes: Array.<string> }} details A dictionary
     *     containing:
     *     - changedTypes: a list of types (as strings) for which there
             are new updates.
     */
    // function onIncomingNotification(details);

    // Also, it responds to the following messages (all other messages
    // are ignored):

    /**
     * Gets the current notification state.
     *
     * @param {function(boolean)} callback Called with whether or not
     *     notifications are enabled.
     */
    // function getNotificationState(callback);

    /**
     * Gets details about the root node.
     *
     * @param {function(!Object)} callback Called with details about the
     *     root node.
     */
    // TODO(akalin): Change this to getRootNodeId or eliminate it
    // entirely.
    // function getRootNodeDetails(callback);

    /**
     * Gets summary information for a list of ids.
     *
     * @param {Array.<string>} idList List of 64-bit ids in decimal
     *     string form.
     * @param {Array.<{id: string, title: string, isFolder: boolean}>}
     * callback Called with summaries for the nodes in idList that
     *     exist.
     */
    // function getNodeSummariesById(idList, callback);

    /**
     * Gets detailed information for a list of ids.
     *
     * @param {Array.<string>} idList List of 64-bit ids in decimal
     *     string form.
     * @param {Array.<!Object>} callback Called with detailed
     *     information for the nodes in idList that exist.
     */
    // function getNodeDetailsById(idList, callback);

    /**
     * Gets child ids for a given id.
     *
     * @param {string} id 64-bit id in decimal string form of the parent
     *     node.
     * @param {Array.<string>} callback Called with the (possibly empty)
     *     list of child ids.
     */
    // function getChildNodeIds(id);

    virtual void OnInitializationComplete(
        const browser_sync::WeakHandle<browser_sync::JsBackend>&
            js_backend, bool success) = 0;

    // We are no longer permitted to communicate with the server. Sync should
    // be disabled and state cleaned up at once.  This can happen for a number
    // of reasons, e.g. swapping from a test instance to production, or a
    // global stop syncing operation has wiped the store.
    virtual void OnStopSyncingPermanently() = 0;

    // After a request to clear server data, these callbacks are invoked to
    // indicate success or failure.
    virtual void OnClearServerDataSucceeded() = 0;
    virtual void OnClearServerDataFailed() = 0;

    // Called when the set of encrypted types or the encrypt
    // everything flag has been changed.  Note that encryption isn't
    // complete until the OnEncryptionComplete() notification has been
    // sent (see below).
    //
    // |encrypted_types| will always be a superset of
    // Cryptographer::SensitiveTypes().  If |encrypt_everything| is
    // true, |encrypted_types| will be the set of all known types.
    //
    // Until this function is called, observers can assume that the
    // set of encrypted types is Cryptographer::SensitiveTypes() and
    // that the encrypt everything flag is false.
    //
    // Called from within a transaction.
    virtual void OnEncryptedTypesChanged(
        const syncable::ModelTypeSet& encrypted_types,
        bool encrypt_everything) = 0;

    // Called after we finish encrypting the current set of encrypted
    // types.
    //
    // Called from within a transaction.
    virtual void OnEncryptionComplete() = 0;

    virtual void OnActionableError(
        const browser_sync::SyncProtocolError& sync_protocol_error) = 0;

   protected:
    virtual ~Observer();
  };

  // Create an uninitialized SyncManager.  Callers must Init() before using.
  explicit SyncManager(const std::string& name);
  virtual ~SyncManager();

  // Initialize the sync manager.  |database_location| specifies the path of
  // the directory in which to locate a sqlite repository storing the syncer
  // backend state. Initialization will open the database, or create it if it
  // does not already exist. Returns false on failure.
  // |event_handler| is the JsEventHandler used to propagate events to
  // chrome://sync-internals.  |event_handler| may be uninitialized.
  // |sync_server_and_path| and |sync_server_port| represent the Chrome sync
  // server to use, and |use_ssl| specifies whether to communicate securely;
  // the default is false.
  // |post_factory| will be owned internally and used to create
  // instances of an HttpPostProvider.
  // |model_safe_worker| ownership is given to the SyncManager.
  // |user_agent| is a 7-bit ASCII string suitable for use as the User-Agent
  // HTTP header. Used internally when collecting stats to classify clients.
  // |sync_notifier| is owned and used to listen for notifications.
  bool Init(const FilePath& database_location,
            const browser_sync::WeakHandle<browser_sync::JsEventHandler>&
                event_handler,
            const std::string& sync_server_and_path,
            int sync_server_port,
            bool use_ssl,
            HttpPostProviderFactory* post_factory,
            browser_sync::ModelSafeWorkerRegistrar* registrar,
            ChangeDelegate* change_delegate,
            const std::string& user_agent,
            const SyncCredentials& credentials,
            sync_notifier::SyncNotifier* sync_notifier,
            const std::string& restored_key_for_bootstrapping,
            bool setup_for_test_mode);

  // Checks if the sync server is reachable.
  void CheckServerReachable();

  // Returns the username last used for a successful authentication.
  // Returns empty if there is no such username.  May be called on any
  // thread.
  const std::string& GetAuthenticatedUsername();

  // Check if the database has been populated with a full "initial" download of
  // sync items for each data type currently present in the routing info.
  // Prerequisite for calling this is that OnInitializationComplete has been
  // called.  May be called from any thread.
  bool InitialSyncEndedForAllEnabledTypes();

  // Update tokens that we're using in Sync. Email must stay the same.
  void UpdateCredentials(const SyncCredentials& credentials);

  // Called when the user disables or enables a sync type.
  void UpdateEnabledTypes();

  // Conditionally sets the flag in the Nigori node which instructs other
  // clients to start syncing tabs.
  void MaybeSetSyncTabsInNigoriNode(const syncable::ModelTypeSet enabled_types);

  // Put the syncer in normal mode ready to perform nudges and polls.
  void StartSyncingNormally();

  // Attempt to set the passphrase. If the passphrase is valid,
  // OnPassphraseAccepted will be fired to notify the ProfileSyncService and the
  // syncer will be nudged so that any update that was waiting for this
  // passphrase gets applied as soon as possible.
  // If the passphrase in invalid, OnPassphraseRequired will be fired.
  // Calling this metdod again is the appropriate course of action to "retry"
  // with a new passphrase.
  // |is_explicit| is true if the call is in response to the user explicitly
  // setting a passphrase as opposed to implicitly (from the users' perspective)
  // using their Google Account password.  An implicit SetPassphrase will *not*
  // *not* override an explicit passphrase set previously.
  void SetPassphrase(const std::string& passphrase, bool is_explicit);

  // Puts the SyncScheduler into a mode where no normal nudge or poll traffic
  // will occur, but calls to RequestConfig will be supported.  If |callback|
  // is provided, it will be invoked (from the internal SyncScheduler) when
  // the thread has changed to configuration mode.
  void StartConfigurationMode(const base::Closure& callback);

  // Switches the mode of operation to CONFIGURATION_MODE and
  // schedules a config task to fetch updates for |types|.
  void RequestConfig(const syncable::ModelTypeBitSet& types,
     sync_api::ConfigureReason reason);

  void RequestCleanupDisabledTypes();

  // Request a clearing of all data on the server
  void RequestClearServerData();

  // Adds a listener to be notified of sync events.
  // NOTE: It is OK (in fact, it's probably a good idea) to call this before
  // having received OnInitializationCompleted.
  void AddObserver(Observer* observer);

  // Remove the given observer.  Make sure to call this if the
  // Observer is being destroyed so the SyncManager doesn't
  // potentially dereference garbage.
  void RemoveObserver(Observer* observer);

  // Status-related getters. Typically GetStatusSummary will suffice, but
  // GetDetailedSyncStatus can be useful for gathering debug-level details of
  // the internals of the sync engine.  May be called on any thread.
  Status::Summary GetStatusSummary() const;
  Status GetDetailedStatus() const;

  // Whether or not the Nigori node is encrypted using an explicit passphrase.
  // May be called on any thread.
  bool IsUsingExplicitPassphrase();

  // Call periodically from a database-safe thread to persist recent changes
  // to the syncapi model.
  void SaveChanges();

  // Initiates shutdown of various components in the sync engine.  Must be
  // called from the main thread to allow preempting ongoing tasks on the sync
  // loop (that may be blocked on I/O).  The semantics of |callback| are the
  // same as with StartConfigurationMode. If provided and a scheduler / sync
  // loop exists, it will be invoked from the sync loop by the scheduler to
  // notify that all work has been flushed + cancelled, and it is idle.
  // If no scheduler exists, the callback is run immediately (from the loop
  // this was created on, which is the sync loop), as sync is effectively
  // stopped.
  void StopSyncingForShutdown(const base::Closure& callback);

  // Issue a final SaveChanges, and close sqlite handles.
  void ShutdownOnSyncThread();

  // May be called from any thread.
  UserShare* GetUserShare() const;

  // Inform the cryptographer of the most recent passphrase and set of
  // encrypted types (from nigori node), then ensure all data that
  // needs encryption is encrypted with the appropriate passphrase.
  //
  // May trigger OnPassphraseRequired().  Otherwise, it will trigger
  // OnEncryptedTypesChanged() if necessary (see comments for
  // OnEncryptedTypesChanged()), and then OnEncryptionComplete().
  //
  // Note: opens a transaction, so must only be called after syncapi
  // has been initialized.
  void RefreshEncryption();

  // Enable encryption of all sync data. Once enabled, it can never be
  // disabled without clearing the server data.
  //
  // This will trigger OnEncryptedTypesChanged() if necessary (see
  // comments for OnEncryptedTypesChanged()).  It then may trigger
  // OnPassphraseRequired(), but otherwise it will trigger
  // OnEncryptionComplete().
  void EnableEncryptEverything();

  // Returns true if we are currently encrypting all sync data.  May
  // be called on any thread.
  bool EncryptEverythingEnabledForTest() const;

  // Gets the set of encrypted types from the cryptographer
  // Note: opens a transaction.  May be called from any thread.
  syncable::ModelTypeSet GetEncryptedDataTypesForTest() const;

  // Reads the nigori node to determine if any experimental types should be
  // enabled.
  // Note: opens a transaction.  May be called on any thread.
  bool ReceivedExperimentalTypes(syncable::ModelTypeSet* to_add) const;

  // Uses a read-only transaction to determine if the directory being synced has
  // any remaining unsynced items.  May be called on any thread.
  bool HasUnsyncedItems() const;

  // Functions used for testing.

  void TriggerOnNotificationStateChangeForTest(
      bool notifications_enabled);

  void TriggerOnIncomingNotificationForTest(
      const syncable::ModelTypeBitSet& model_types);

 private:
  base::ThreadChecker thread_checker_;

  // An opaque pointer to the nested private class.
  SyncInternal* data_;

  DISALLOW_COPY_AND_ASSIGN(SyncManager);
};

bool InitialSyncEndedForTypes(syncable::ModelTypeSet types, UserShare* share);

syncable::ModelTypeSet GetTypesWithEmptyProgressMarkerToken(
    const syncable::ModelTypeSet types,
    sync_api::UserShare* share);

// Returns the string representation of a PassphraseRequiredReason value.
std::string PassphraseRequiredReasonToString(PassphraseRequiredReason reason);

}  // namespace sync_api

#endif  // CHROME_BROWSER_SYNC_INTERNAL_API_SYNC_MANAGER_H_
