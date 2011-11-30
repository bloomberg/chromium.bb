// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_SYNC_BACKEND_HOST_H_
#define CHROME_BROWSER_SYNC_GLUE_SYNC_BACKEND_HOST_H_
#pragma once

#include <map>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread.h"
#include "base/timer.h"
#include "chrome/browser/sync/engine/model_safe_worker.h"
#include "chrome/browser/sync/internal_api/configure_reason.h"
#include "chrome/browser/sync/internal_api/sync_manager.h"
#include "chrome/browser/sync/notifier/sync_notifier_factory.h"
#include "chrome/browser/sync/protocol/sync_protocol_error.h"
#include "chrome/browser/sync/syncable/model_type.h"
#include "chrome/browser/sync/util/weak_handle.h"
#include "chrome/common/net/gaia/google_service_auth_error.h"
#include "googleurl/src/gurl.h"
#include "net/url_request/url_request_context_getter.h"

class MessageLoop;
class Profile;

namespace browser_sync {

namespace sessions {
struct SyncSessionSnapshot;
}

class ChangeProcessor;
class JsBackend;
class JsEventHandler;
class SyncBackendRegistrar;
class SyncPrefs;

// SyncFrontend is the interface used by SyncBackendHost to communicate with
// the entity that created it and, presumably, is interested in sync-related
// activity.
// NOTE: All methods will be invoked by a SyncBackendHost on the same thread
// used to create that SyncBackendHost.
class SyncFrontend {
 public:
  SyncFrontend() {}

  // The backend has completed initialization and it is now ready to
  // accept and process changes.  If success is false, initialization
  // wasn't able to be completed and should be retried.
  //
  // |js_backend| is what about:sync interacts with; it's different
  // from the 'Backend' in 'OnBackendInitialized' (unfortunately).  It
  // is initialized only if |success| is true.
  virtual void OnBackendInitialized(
      const WeakHandle<JsBackend>& js_backend, bool success) = 0;

  // The backend queried the server recently and received some updates.
  virtual void OnSyncCycleCompleted() = 0;

  // The backend encountered an authentication problem and requests new
  // credentials to be provided. See SyncBackendHost::Authenticate for details.
  virtual void OnAuthError() = 0;

  // We are no longer permitted to communicate with the server. Sync should
  // be disabled and state cleaned up at once.
  virtual void OnStopSyncingPermanently() = 0;

  // Called to handle success/failure of clearing server data
  virtual void OnClearServerDataSucceeded() = 0;
  virtual void OnClearServerDataFailed() = 0;

  // The syncer requires a passphrase to decrypt sensitive updates. This is
  // called when the first sensitive data type is setup by the user and anytime
  // the passphrase is changed by another synced client. |reason| denotes why
  // the passphrase was required.
  virtual void OnPassphraseRequired(
      sync_api::PassphraseRequiredReason reason) = 0;

  // Called when the passphrase provided by the user is
  // accepted. After this is called, updates to sensitive nodes are
  // encrypted using the accepted passphrase.
  virtual void OnPassphraseAccepted() = 0;

  // Called when the set of encrypted types or the encrypt everything
  // flag has been changed.  Note that encryption isn't complete until
  // the OnEncryptionComplete() notification has been sent (see
  // below).
  //
  // |encrypted_types| will always be a superset of
  // Cryptographer::SensitiveTypes().  If |encrypt_everything| is
  // true, |encrypted_types| will be the set of all known types.
  //
  // Until this function is called, observers can assume that the set
  // of encrypted types is Cryptographer::SensitiveTypes() and that
  // the encrypt everything flag is false.
  virtual void OnEncryptedTypesChanged(
      const syncable::ModelTypeSet& encrypted_types,
      bool encrypt_everything) = 0;

  // Called after we finish encrypting the current set of encrypted
  // types.
  virtual void OnEncryptionComplete() = 0;

  // Called to perform migration of |types|.
  virtual void OnMigrationNeededForTypes(
      const syncable::ModelTypeSet& types) = 0;

  // Inform the Frontend that new datatypes are available for registration.
  virtual void OnDataTypesChanged(const syncable::ModelTypeSet& to_add) = 0;

  // Called when the sync cycle returns there is an user actionable error.
  virtual void OnActionableError(
      const browser_sync::SyncProtocolError& error) = 0;

 protected:
  // Don't delete through SyncFrontend interface.
  virtual ~SyncFrontend() {
  }
 private:
  DISALLOW_COPY_AND_ASSIGN(SyncFrontend);
};

// A UI-thread safe API into the sync backend that "hosts" the top-level
// syncapi element, the SyncManager, on its own thread. This class handles
// dispatch of potentially blocking calls to appropriate threads and ensures
// that the SyncFrontend is only accessed on the UI loop.
class SyncBackendHost {
 public:
  typedef sync_api::SyncManager::Status::Summary StatusSummary;
  typedef sync_api::SyncManager::Status Status;

  // Create a SyncBackendHost with a reference to the |frontend| that
  // it serves and communicates to via the SyncFrontend interface (on
  // the same thread it used to call the constructor).  Must outlive
  // |sync_prefs|.
  SyncBackendHost(const std::string& name,
                  Profile* profile,
                  const base::WeakPtr<SyncPrefs>& sync_prefs);
  // For testing.
  // TODO(skrul): Extract an interface so this is not needed.
  SyncBackendHost();
  virtual ~SyncBackendHost();

  // Called on |frontend_loop_| to kick off asynchronous initialization.
  // As a fallback when no cached auth information is available, try to
  // bootstrap authentication using |lsid|, if it isn't empty.
  // Optionally delete the Sync Data folder (if it's corrupt).
  void Initialize(SyncFrontend* frontend,
                  const WeakHandle<JsEventHandler>& event_handler,
                  const GURL& service_url,
                  const syncable::ModelTypeSet& initial_types,
                  const sync_api::SyncCredentials& credentials,
                  bool delete_sync_data_folder);

  // Called from |frontend_loop| to update SyncCredentials.
  void UpdateCredentials(const sync_api::SyncCredentials& credentials);

  // This starts the SyncerThread running a Syncer object to communicate with
  // sync servers.  Until this is called, no changes will leave or enter this
  // browser from the cloud / sync servers.
  // Called on |frontend_loop_|.
  virtual void StartSyncingWithServer();

  // Called on |frontend_loop_| to asynchronously set the passphrase.
  // |is_explicit| is true if the call is in response to the user explicitly
  // setting a passphrase as opposed to implicitly (from the users' perspective)
  // using their Google Account password.  An implicit SetPassphrase will *not*
  // *not* override an explicit passphrase set previously.
  void SetPassphrase(const std::string& passphrase, bool is_explicit);

  // Called on |frontend_loop_| to kick off shutdown procedure. After this,
  // no further sync activity will occur with the sync server and no further
  // change applications will occur from changes already downloaded.
  virtual void StopSyncingForShutdown();

  // Called on |frontend_loop_| to kick off shutdown.
  // |sync_disabled| indicates if syncing is being disabled or not.
  // See the implementation and Core::DoShutdown for details.
  // Must be called *after* StopSyncingForShutdown.
  void Shutdown(bool sync_disabled);

  // Changes the set of data types that are currently being synced.
  // The ready_task will be run when configuration is done with the
  // set of all types that failed configuration (i.e., if its argument
  // is non-empty, then an error was encountered).
  virtual void ConfigureDataTypes(
      const syncable::ModelTypeSet& types_to_add,
      const syncable::ModelTypeSet& types_to_remove,
      sync_api::ConfigureReason reason,
      base::Callback<void(const syncable::ModelTypeSet&)> ready_task,
      bool enable_nigori);

  // Makes an asynchronous call to syncer to switch to config mode. When done
  // syncer will call us back on FinishConfigureDataTypes.
  virtual void StartConfiguration(const base::Closure& callback);

  // Turns on encryption of all present and future sync data.
  virtual void EnableEncryptEverything();

  // Activates change processing for the given data type.  This must
  // be called synchronously with the data type's model association so
  // no changes are dropped between model association and change
  // processor activation.
  void ActivateDataType(
      syncable::ModelType type, ModelSafeGroup group,
      ChangeProcessor* change_processor);

  // Deactivates change processing for the given data type.
  void DeactivateDataType(syncable::ModelType type);

  // Asks the server to clear all data associated with ChromeSync.
  virtual bool RequestClearServerData();

  // Called on |frontend_loop_| to obtain a handle to the UserShare needed
  // for creating transactions.
  sync_api::UserShare* GetUserShare() const;

  // Called from any thread to obtain current status information in detailed or
  // summarized form.
  Status GetDetailedStatus();
  StatusSummary GetStatusSummary();
  const GoogleServiceAuthError& GetAuthError() const;
  const sessions::SyncSessionSnapshot* GetLastSessionSnapshot() const;

  const FilePath& sync_data_folder_path() const {
    return sync_data_folder_path_;
  }

  // Returns the authenticated username of the sync user, or empty if none
  // exists. It will only exist if the authentication service provider (e.g
  // GAIA) has confirmed the username is authentic.
  string16 GetAuthenticatedUsername() const;

  // Determines if the underlying sync engine has made any local changes to
  // items that have not yet been synced with the server.
  // ONLY CALL THIS IF OnInitializationComplete was called!
  bool HasUnsyncedItems() const;

  // Whether or not we are syncing encryption keys.
  bool IsNigoriEnabled() const;

  // Whether or not the Nigori node is encrypted using an explicit passphrase.
  bool IsUsingExplicitPassphrase();

  // True if the cryptographer has any keys available to attempt decryption.
  // Could mean we've downloaded and loaded Nigori objects, or we bootstrapped
  // using a token previously received.
  bool IsCryptographerReady(const sync_api::BaseTransaction* trans) const;

  void GetModelSafeRoutingInfo(ModelSafeRoutingInfo* out) const;

 protected:
  // An enum representing the steps to initializing the SyncBackendHost.
  enum InitializationState {
    NOT_ATTEMPTED,
    CREATING_SYNC_MANAGER,  // We're waiting for the first callback from the
                            // sync thread to inform us that the sync manager
                            // has been created.
    NOT_INITIALIZED,        // Initialization hasn't completed, but we've
                            // constructed a SyncManager.
    DOWNLOADING_NIGORI,     // The SyncManager is initialized, but
                            // we're fetching encryption information.
    REFRESHING_ENCRYPTION,  // The SyncManager is initialized, and we
                            // have the encryption information, but we
                            // still need to refresh encryption.
    INITIALIZED,            // Initialization is complete.
  };

  // The real guts of SyncBackendHost, to keep the public client API clean.
  class Core : public base::RefCountedThreadSafe<SyncBackendHost::Core>,
               public sync_api::SyncManager::Observer {
   public:
    Core(const std::string& name, SyncBackendHost* backend);

    // SyncManager::Observer implementation.  The Core just acts like an air
    // traffic controller here, forwarding incoming messages to appropriate
    // landing threads.
    virtual void OnSyncCycleCompleted(
        const sessions::SyncSessionSnapshot* snapshot) OVERRIDE;
    virtual void OnInitializationComplete(
        const WeakHandle<JsBackend>& js_backend,
        bool success) OVERRIDE;
    virtual void OnAuthError(
        const GoogleServiceAuthError& auth_error) OVERRIDE;
    virtual void OnPassphraseRequired(
        sync_api::PassphraseRequiredReason reason) OVERRIDE;
    virtual void OnPassphraseAccepted(
        const std::string& bootstrap_token) OVERRIDE;
    virtual void OnStopSyncingPermanently() OVERRIDE;
    virtual void OnUpdatedToken(const std::string& token) OVERRIDE;
    virtual void OnClearServerDataFailed() OVERRIDE;
    virtual void OnClearServerDataSucceeded() OVERRIDE;
    virtual void OnEncryptedTypesChanged(
        const syncable::ModelTypeSet& encrypted_types,
        bool encrypt_everything) OVERRIDE;
    virtual void OnEncryptionComplete() OVERRIDE;
    virtual void OnActionableError(
        const browser_sync::SyncProtocolError& sync_error) OVERRIDE;

    struct DoInitializeOptions {
      DoInitializeOptions(
          MessageLoop* sync_loop,
          SyncBackendRegistrar* registrar,
          const WeakHandle<JsEventHandler>& event_handler,
          const GURL& service_url,
          const scoped_refptr<net::URLRequestContextGetter>&
              request_context_getter,
          const sync_api::SyncCredentials& credentials,
          bool delete_sync_data_folder,
          const std::string& restored_key_for_bootstrapping,
          bool setup_for_test_mode);
      ~DoInitializeOptions();

      MessageLoop* sync_loop;
      SyncBackendRegistrar* registrar;
      WeakHandle<JsEventHandler> event_handler;
      GURL service_url;
      scoped_refptr<net::URLRequestContextGetter> request_context_getter;
      sync_api::SyncCredentials credentials;
      std::string lsid;
      bool delete_sync_data_folder;
      std::string restored_key_for_bootstrapping;
      bool setup_for_test_mode;
    };

    // Note:
    //
    // The Do* methods are the various entry points from our
    // SyncBackendHost.  They are all called on the sync thread to
    // actually perform synchronous (and potentially blocking) syncapi
    // operations.
    //
    // Called to perform initialization of the syncapi on behalf of
    // SyncBackendHost::Initialize.
    void DoInitialize(const DoInitializeOptions& options);

    // Called to check server reachability after initialization is
    // fully completed.
    void DoCheckServerReachable();

    // Called to perform credential update on behalf of
    // SyncBackendHost::UpdateCredentials
    void DoUpdateCredentials(const sync_api::SyncCredentials& credentials);

    // Called when the user disables or enables a sync type.
    void DoUpdateEnabledTypes();

    // Called to tell the syncapi to start syncing (generally after
    // initialization and authentication).
    void DoStartSyncing();

    // Called to clear server data.
    void DoRequestClearServerData();

    // Called to cleanup disabled types.
    void DoRequestCleanupDisabledTypes();

    // Called to set the passphrase on behalf of
    // SyncBackendHost::SupplyPassphrase.
    void DoSetPassphrase(const std::string& passphrase, bool is_explicit);

    // Called to turn on encryption of all sync data as well as
    // reencrypt everything.
    void DoEnableEncryptEverything();

    // Called to refresh encryption with the most recent passphrase
    // and set of encrypted types.  |done_callback| is called on the
    // sync thread.
    void DoRefreshEncryption(const base::Closure& done_callback);

    // The shutdown order is a bit complicated:
    // 1) From |sync_thread_|, invoke the syncapi Shutdown call to do
    //    a final SaveChanges, and close sqlite handles.
    // 2) Then, from |frontend_loop_|, halt the sync_thread_ (which is
    //    a blocking call). This causes syncapi thread-exit handlers
    //    to run and make use of cached pointers to various components
    //    owned implicitly by us.
    // 3) Destroy this Core. That will delete syncapi components in a
    //    safe order because the thread that was using them has exited
    //    (in step 2).
    void DoStopSyncManagerForShutdown(const base::Closure& closure);
    void DoShutdown(bool stopping_sync);

    virtual void DoRequestConfig(
        const syncable::ModelTypeBitSet& types_to_config,
        sync_api::ConfigureReason reason);

    // Start the configuration mode.  |callback| is called on the sync
    // thread.
    virtual void DoStartConfiguration(const base::Closure& callback);

    // Set the base request context to use when making HTTP calls.
    // This method will add a reference to the context to persist it
    // on the IO thread. Must be removed from IO thread.

    sync_api::SyncManager* sync_manager() { return sync_manager_.get(); }

    // Delete the sync data folder to cleanup backend data.  Happens the first
    // time sync is enabled for a user (to prevent accidentally reusing old
    // sync databases), as well as shutdown when you're no longer syncing.
    void DeleteSyncDataFolder();

    // A callback from the SyncerThread when it is safe to continue config.
    void FinishConfigureDataTypes();

    // Called to handle updating frontend thread components whenever we may
    // need to alert the frontend that the backend is intialized.
    void HandleInitializationCompletedOnFrontendLoop(
        const WeakHandle<JsBackend>& js_backend,
        bool success);

    // Called when configuration of the Nigori node has completed as
    // part of the initialization process.
    void HandleNigoriConfigurationCompletedOnFrontendLoop(
        const WeakHandle<JsBackend>& js_backend,
        const syncable::ModelTypeSet& failed_configuration_types);

   private:
    friend class base::RefCountedThreadSafe<SyncBackendHost::Core>;
    friend class SyncBackendHostForProfileSyncTest;

    virtual ~Core();

    // Invoked when initialization of syncapi is complete and we can start
    // our timer.
    // This must be called from the thread on which SaveChanges is intended to
    // be run on; the host's |sync_thread_|.
    void StartSavingChanges();

    // Invoked periodically to tell the syncapi to persist its state
    // by writing to disk.
    // This is called from the thread we were created on (which is the
    // SyncBackendHost |sync_thread_|), using a repeating timer that is kicked
    // off as soon as the SyncManager tells us it completed
    // initialization.
    void SaveChanges();

    // Let the front end handle the actionable error event.
    void HandleActionableErrorEventOnFrontendLoop(
        const browser_sync::SyncProtocolError& sync_error);

    // Dispatched to from OnAuthError to handle updating frontend UI
    // components.
    void HandleAuthErrorEventOnFrontendLoop(
        const GoogleServiceAuthError& new_auth_error);

    // Invoked when a passphrase is required to decrypt a set of Nigori keys,
    // or for encrypting. |reason| denotes why the passhrase was required.
    void NotifyPassphraseRequired(sync_api::PassphraseRequiredReason reason);

    // Invoked when the passphrase provided by the user has been accepted.
    void NotifyPassphraseAccepted(const std::string& bootstrap_token);

    // Invoked when an updated token is available from the sync server.
    void NotifyUpdatedToken(const std::string& token);

    // Invoked when the set of encrypted types or the encrypt
    // everything flag changes.
    void NotifyEncryptedTypesChanged(
        const syncable::ModelTypeSet& encrypted_types,
        bool encrypt_everything);

    // Invoked when sync finishes encrypting new datatypes.
    void NotifyEncryptionComplete();

    // Called from Core::OnSyncCycleCompleted to handle updating frontend
    // thread components.
    void HandleSyncCycleCompletedOnFrontendLoop(
        sessions::SyncSessionSnapshot* snapshot);

    void HandleStopSyncingPermanentlyOnFrontendLoop();

    // Called to handle success/failure of clearing server data
    void HandleClearServerDataSucceededOnFrontendLoop();
    void HandleClearServerDataFailedOnFrontendLoop();

    void FinishConfigureDataTypesOnFrontendLoop();

    // Name used for debugging.
    const std::string name_;

    // Our parent SyncBackendHost
    SyncBackendHost* host_;

    // The loop where all the sync backend operations happen.
    // Non-NULL only between calls to DoInitialize() and DoShutdown().
    MessageLoop* sync_loop_;

    // Our parent's registrar (not owned).  Non-NULL only between
    // calls to DoInitialize() and DoShutdown().
    SyncBackendRegistrar* registrar_;

    // The timer used to periodically call SaveChanges.
    base::RepeatingTimer<Core> save_changes_timer_;

    // The top-level syncapi entry point.  Lives on the sync thread.
    scoped_ptr<sync_api::SyncManager> sync_manager_;

    DISALLOW_COPY_AND_ASSIGN(Core);
  };

  // Checks if we have received a notice to turn on experimental datatypes
  // (via the nigori node) and informs the frontend if that is the case.
  // Note: it is illegal to call this before the backend is initialized.
  void AddExperimentalTypes();

  // InitializationComplete passes through the SyncBackendHost to forward
  // on to |frontend_|, and so that tests can intercept here if they need to
  // set up initial conditions.
  virtual void HandleInitializationCompletedOnFrontendLoop(
      const WeakHandle<JsBackend>& js_backend,
      bool success);

  // Called to finish the job of ConfigureDataTypes once the syncer is in
  // configuration mode.
  void FinishConfigureDataTypesOnFrontendLoop();

  // Allows tests to perform alternate core initialization work.
  virtual void InitCore(const Core::DoInitializeOptions& options);

  // Factory method for HttpPostProviderFactories.  Should be
  // thread-safe.
  virtual sync_api::HttpPostProviderFactory* MakeHttpBridgeFactory(
      const scoped_refptr<net::URLRequestContextGetter>& getter);

  // Helpers to persist a token that can be used to bootstrap sync encryption
  // across browser restart to avoid requiring the user to re-enter their
  // passphrase.  |token| must be valid UTF-8 as we use the PrefService for
  // storage.
  void PersistEncryptionBootstrapToken(const std::string& token);

  // Our core, which communicates directly to the syncapi.
  scoped_refptr<Core> core_;

  InitializationState initialization_state_;

 private:
  struct PendingConfigureDataTypesState {
    PendingConfigureDataTypesState();
    ~PendingConfigureDataTypesState();

    // The ready_task will be run when configuration is done with the
    // set of all types that failed configuration (i.e., if its
    // argument is non-empty, then an error was encountered).
    base::Callback<void(const syncable::ModelTypeSet&)> ready_task;

    // The set of types that we are waiting to be initially synced in a
    // configuration cycle.
    syncable::ModelTypeSet types_to_add;

    // Additional details about which types were added.
    syncable::ModelTypeSet added_types;
    sync_api::ConfigureReason reason;
  };

  // For convenience, checks if initialization state is INITIALIZED.
  bool initialized() const { return initialization_state_ == INITIALIZED; }

  // Must be called on |frontend_loop_|.  |done_callback| is called on
  // |frontend_loop_|.
  void RefreshEncryption(const base::Closure& done_callback);

  // Handles stopping the core's SyncManager, accounting for whether
  // initialization is done yet.
  void StopSyncManagerForShutdown(const base::Closure& closure);

  // A thread where all the sync operations happen.
  base::Thread sync_thread_;

  // A reference to the MessageLoop used to construct |this|, so we know how
  // to safely talk back to the SyncFrontend.
  MessageLoop* const frontend_loop_;

  Profile* const profile_;

  const base::WeakPtr<SyncPrefs> sync_prefs_;

  // Name used for debugging (set from profile_->GetDebugName()).
  const std::string name_;

  sync_notifier::SyncNotifierFactory sync_notifier_factory_;

  scoped_ptr<SyncBackendRegistrar> registrar_;

  // The frontend which we serve (and are owned by).
  SyncFrontend* frontend_;

  // Path of the folder that stores the sync data files.
  FilePath sync_data_folder_path_;

  scoped_ptr<PendingConfigureDataTypesState> pending_download_state_;
  scoped_ptr<PendingConfigureDataTypesState> pending_config_mode_state_;

  // UI-thread cache of the last AuthErrorState received from syncapi.
  GoogleServiceAuthError last_auth_error_;

  // UI-thread cache of the last SyncSessionSnapshot received from syncapi.
  scoped_ptr<sessions::SyncSessionSnapshot> last_snapshot_;

  DISALLOW_COPY_AND_ASSIGN(SyncBackendHost);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_SYNC_BACKEND_HOST_H_
