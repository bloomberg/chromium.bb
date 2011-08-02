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
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread.h"
#include "base/timer.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/sync/engine/syncapi.h"
#include "chrome/browser/sync/engine/configure_reason.h"
#include "chrome/browser/sync/engine/model_safe_worker.h"
#include "chrome/browser/sync/js_backend.h"
#include "chrome/browser/sync/glue/data_type_controller.h"
#include "chrome/browser/sync/glue/ui_model_worker.h"
#include "chrome/browser/sync/notifier/sync_notifier_factory.h"
#include "chrome/browser/sync/syncable/model_type.h"
#include "chrome/browser/sync/weak_handle.h"
#include "chrome/common/net/gaia/google_service_auth_error.h"
#include "googleurl/src/gurl.h"
#include "net/url_request/url_request_context_getter.h"

class CancelableTask;
class Profile;

namespace net {
class URLRequestContextGetter;
}

namespace browser_sync {

namespace sessions {
struct SyncSessionSnapshot;
}

class ChangeProcessor;
class DataTypeController;
class JsEventHandler;

// SyncFrontend is the interface used by SyncBackendHost to communicate with
// the entity that created it and, presumably, is interested in sync-related
// activity.
// NOTE: All methods will be invoked by a SyncBackendHost on the same thread
// used to create that SyncBackendHost.
class SyncFrontend {
 public:
  SyncFrontend() {}

  // The backend has completed initialization and it is now ready to accept and
  // process changes.  If success is false, initialization wasn't able to be
  // completed and should be retried.
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

  virtual void OnEncryptionComplete(
      const syncable::ModelTypeSet& encrypted_types) = 0;

  // Called to perform migration of |types|.
  virtual void OnMigrationNeededForTypes(
      const syncable::ModelTypeSet& types) = 0;

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
class SyncBackendHost : public browser_sync::ModelSafeWorkerRegistrar {
 public:
  typedef sync_api::SyncManager::Status::Summary StatusSummary;
  typedef sync_api::SyncManager::Status Status;
  typedef std::map<ModelSafeGroup,
                   scoped_refptr<browser_sync::ModelSafeWorker> > WorkerMap;

  // Create a SyncBackendHost with a reference to the |frontend| that it serves
  // and communicates to via the SyncFrontend interface (on the same thread
  // it used to call the constructor).
  explicit SyncBackendHost(Profile* profile);
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
                  const syncable::ModelTypeSet& types,
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

  // Called on |frontend_loop_| to kick off shutdown.
  // |sync_disabled| indicates if syncing is being disabled or not.
  // See the implementation and Core::DoShutdown for details.
  void Shutdown(bool sync_disabled);

  // Changes the set of data types that are currently being synced.
  // The ready_task will be run when all of the requested data types
  // are up-to-date and ready for activation.  The task will cancelled
  // upon shutdown.  The method takes ownership of the task pointer.
  virtual void ConfigureDataTypes(
      const DataTypeController::TypeMap& data_type_controllers,
      const syncable::ModelTypeSet& types,
      sync_api::ConfigureReason reason,
      base::Callback<void(bool)> ready_task,
      bool enable_nigori);

  // Makes an asynchronous call to syncer to switch to config mode. When done
  // syncer will call us back on FinishConfigureDataTypes.
  virtual void StartConfiguration(Callback0::Type* callback);

  // Encrypts the specified datatypes and marks them as needing encryption on
  // other machines. This affects all machines synced to this account and all
  // data belonging to the specified types.
  // Note: actual work is done on sync_thread_'s message loop.
  virtual void EncryptDataTypes(
      const syncable::ModelTypeSet& encrypted_types);

  syncable::ModelTypeSet GetEncryptedDataTypes() const;

  // Activates change processing for the given data type.  This must
  // be called synchronously with the data type's model association so
  // no changes are dropped between model association and change
  // processor activation.
  void ActivateDataType(DataTypeController* data_type_controller,
                        ChangeProcessor* change_processor);

  // Deactivates change processing for the given data type.
  void DeactivateDataType(DataTypeController* data_type_controller,
                          ChangeProcessor* change_processor);

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

  // ModelSafeWorkerRegistrar implementation.
  virtual void GetWorkers(std::vector<browser_sync::ModelSafeWorker*>* out);
  virtual void GetModelSafeRoutingInfo(ModelSafeRoutingInfo* out);

  // Determines if the underlying sync engine has made any local changes to
  // items that have not yet been synced with the server.
  // ONLY CALL THIS IF OnInitializationComplete was called!
  bool HasUnsyncedItems() const;

  // Logs the unsynced items.
  void LogUnsyncedItems(int level) const;

  // Whether or not we are syncing encryption keys.
  bool IsNigoriEnabled() const;

  // Whether or not the Nigori node is encrypted using an explicit passphrase.
  bool IsUsingExplicitPassphrase();

  // True if the cryptographer has any keys available to attempt decryption.
  // Could mean we've downloaded and loaded Nigori objects, or we bootstrapped
  // using a token previously received.
  bool IsCryptographerReady(const sync_api::BaseTransaction* trans) const;

 protected:
  // An enum representing the steps to initializing the SyncBackendHost.
  enum InitializationState {
    NOT_INITIALIZED,     // Initialization hasn't completed.
    DOWNLOADING_NIGORI,  // The SyncManager is initialized, but we're fetching
                         // encryption information before alerting the
                         // frontend.
    INITIALIZED,         // Initialization is complete.
  };

  // The real guts of SyncBackendHost, to keep the public client API clean.
  class Core : public base::RefCountedThreadSafe<SyncBackendHost::Core>,
               public sync_api::SyncManager::Observer {
   public:
    Core(Profile* profile, SyncBackendHost* backend);

    // SyncManager::Observer implementation.  The Core just acts like an air
    // traffic controller here, forwarding incoming messages to appropriate
    // landing threads.
    virtual void OnChangesApplied(
        syncable::ModelType model_type,
        const sync_api::BaseTransaction* trans,
        const sync_api::SyncManager::ChangeRecord* changes,
        int change_count);
    virtual void OnChangesComplete(syncable::ModelType model_type);
    virtual void OnSyncCycleCompleted(
        const sessions::SyncSessionSnapshot* snapshot);
    virtual void OnInitializationComplete(
        const WeakHandle<JsBackend>& js_backend);
    virtual void OnAuthError(const GoogleServiceAuthError& auth_error);
    virtual void OnPassphraseRequired(
        sync_api::PassphraseRequiredReason reason);
    virtual void OnPassphraseAccepted(const std::string& bootstrap_token);
    virtual void OnStopSyncingPermanently();
    virtual void OnUpdatedToken(const std::string& token);
    virtual void OnClearServerDataFailed();
    virtual void OnClearServerDataSucceeded();
    virtual void OnEncryptionComplete(
        const syncable::ModelTypeSet& encrypted_types);

    struct DoInitializeOptions {
      DoInitializeOptions(
          const WeakHandle<JsEventHandler>& event_handler,
          const GURL& service_url,
          const scoped_refptr<net::URLRequestContextGetter>&
              request_context_getter,
          const sync_api::SyncCredentials& credentials,
          bool delete_sync_data_folder,
          const std::string& restored_key_for_bootstrapping,
          bool setup_for_test_mode);
      ~DoInitializeOptions();

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
    // The Do* methods are the various entry points from our SyncBackendHost.
    // It calls us on a dedicated thread to actually perform synchronous
    // (and potentially blocking) syncapi operations.
    //
    // Called on the SyncBackendHost sync_thread_ to perform initialization
    // of the syncapi on behalf of SyncBackendHost::Initialize.
    void DoInitialize(const DoInitializeOptions& options);

    // Called on our SyncBackendHost's sync_thread_ to perform credential
    // update on behalf of SyncBackendHost::UpdateCredentials
    void DoUpdateCredentials(const sync_api::SyncCredentials& credentials);

    // Called when the user disables or enables a sync type.
    void DoUpdateEnabledTypes();

    // Called on the SyncBackendHost sync_thread_ to tell the syncapi to start
    // syncing (generally after initialization and authentication).
    void DoStartSyncing();

    // Called on the SyncBackendHost sync_thread_ to clear server
    // data.
    void DoRequestClearServerData();

    // Called on the SyncBackendHost sync_thread_ to cleanup disabled
    // types.
    void DoRequestCleanupDisabledTypes();

    // Called on our SyncBackendHost's |sync_thread_| to set the passphrase
    // on behalf of SyncBackendHost::SupplyPassphrase.
    void DoSetPassphrase(const std::string& passphrase, bool is_explicit);

    // Getter/setter for whether we are waiting on SetPassphrase to process a
    // passphrase. Set by SetPassphrase, cleared by OnPassphraseRequired or
    // OnPassphraseAccepted.
    bool processing_passphrase() const;
    void set_processing_passphrase();

    // Called on SyncBackendHost's |sync_thread_| to set the datatypes we need
    // to encrypt as well as encrypt all local data of that type.
    void DoEncryptDataTypes(const syncable::ModelTypeSet& encrypted_types);

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
    void DoShutdown(bool stopping_sync);

    // Posts a config request on the sync thread.
    virtual void DoRequestConfig(const syncable::ModelTypeBitSet& added_types,
        sync_api::ConfigureReason reason);

    // Start the configuration mode.
    virtual void DoStartConfiguration(Callback0::Type* callback);

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

#if defined(UNIT_TEST)
    // Special form of initialization that does not try and authenticate the
    // last known user (since it will fail in test mode) and does some extra
    // setup to nudge the syncapi into a usable state.
    void DoInitializeForTest(
        const std::wstring& test_user,
        const scoped_refptr<net::URLRequestContextGetter>& getter,
        bool delete_sync_data_folder) {
      // Construct dummy credentials for test.
      sync_api::SyncCredentials credentials;
      credentials.email = WideToUTF8(test_user);
      credentials.sync_token = "token";
      DoInitialize(DoInitializeOptions(WeakHandle<JsEventHandler>(),
                                       GURL(), getter, credentials,
                                       delete_sync_data_folder,
                                       "", true));
    }
#endif

   private:
    friend class base::RefCountedThreadSafe<SyncBackendHost::Core>;
    friend class SyncBackendHostForProfileSyncTest;

    virtual ~Core();

    // Return change processor for a particular model (return NULL on failure).
    ChangeProcessor* GetProcessor(syncable::ModelType modeltype);

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

    // Invoked when sync finishes encrypting new datatypes or has become aware
    // of new datatypes requiring encryption.
    void NotifyEncryptionComplete(const syncable::ModelTypeSet&
                                      encrypted_types);

    // Called from Core::OnSyncCycleCompleted to handle updating frontend
    // thread components.
    void HandleSyncCycleCompletedOnFrontendLoop(
        sessions::SyncSessionSnapshot* snapshot);

    void HandleStopSyncingPermanentlyOnFrontendLoop();

    // Called to handle success/failure of clearing server data
    void HandleClearServerDataSucceededOnFrontendLoop();
    void HandleClearServerDataFailedOnFrontendLoop();

    void FinishConfigureDataTypesOnFrontendLoop();

    // Return true if a model lives on the current thread.
    bool IsCurrentThreadSafeForModel(syncable::ModelType model_type);

    Profile* profile_;

    // Our parent SyncBackendHost
    SyncBackendHost* host_;

    // The timer used to periodically call SaveChanges.
    base::RepeatingTimer<Core> save_changes_timer_;

    // The top-level syncapi entry point.  Lives on the sync thread.
    scoped_ptr<sync_api::SyncManager> sync_manager_;

    // Denotes if the core is currently attempting to set a passphrase. While
    // this is true, OnPassphraseRequired calls are dropped.
    // Note: after initialization, this variable should only ever be accessed or
    // modified from within the frontend_loop_ (UI thread).
    bool processing_passphrase_;

    DISALLOW_COPY_AND_ASSIGN(Core);
  };

  // InitializationComplete passes through the SyncBackendHost to forward
  // on to |frontend_|, and so that tests can intercept here if they need to
  // set up initial conditions.
  virtual void HandleInitializationCompletedOnFrontendLoop(
      const WeakHandle<JsBackend>& js_backend,
      bool success);

  // Called to finish the job of ConfigureDataTypes once the syncer is in
  // configuration mode.
  void FinishConfigureDataTypes();
  void FinishConfigureDataTypesOnFrontendLoop();

  // Allows tests to perform alternate core initialization work.
  virtual void InitCore(const Core::DoInitializeOptions& options);

  // Factory method for HttpPostProviderFactories.  Should be
  // thread-safe.
  virtual sync_api::HttpPostProviderFactory* MakeHttpBridgeFactory(
      const scoped_refptr<net::URLRequestContextGetter>& getter);

  MessageLoop* sync_loop() { return sync_thread_.message_loop(); }

  // Helpers to persist a token that can be used to bootstrap sync encryption
  // across browser restart to avoid requiring the user to re-enter their
  // passphrase.  |token| must be valid UTF-8 as we use the PrefService for
  // storage.
  void PersistEncryptionBootstrapToken(const std::string& token);
  std::string RestoreEncryptionBootstrapToken();

  // Our core, which communicates directly to the syncapi.
  scoped_refptr<Core> core_;

  InitializationState initialization_state_;

 private:
  FRIEND_TEST_ALL_PREFIXES(SyncBackendHostTest, GetPendingConfigModeState);

  struct PendingConfigureDataTypesState {
    PendingConfigureDataTypesState();
    ~PendingConfigureDataTypesState();

    // A task that should be called once data type configuration is
    // complete.
    base::Callback<void(bool)> ready_task;

    // The set of types that we are waiting to be initially synced in a
    // configuration cycle.
    syncable::ModelTypeSet initial_types;

    // Additional details about which types were added.
    syncable::ModelTypeBitSet added_types;
    sync_api::ConfigureReason reason;
  };

  UIModelWorker* ui_worker();

  // Helper function for ConfigureDataTypes() that fills in |state|
  // and |deleted_type|.  Does not take ownership of routing_info|.
  static void GetPendingConfigModeState(
      const DataTypeController::TypeMap& data_type_controllers,
      const syncable::ModelTypeSet& types,
      base::Callback<void(bool)> ready_task,
      ModelSafeRoutingInfo* routing_info,
      sync_api::ConfigureReason reason,
      bool nigori_enabled,
      PendingConfigureDataTypesState* state,
      bool* deleted_type);

  // For convenience, checks if initialization state is INITIALIZED.
  bool initialized() const { return initialization_state_ == INITIALIZED; }

  // A thread where all the sync operations happen.
  base::Thread sync_thread_;

  // A reference to the MessageLoop used to construct |this|, so we know how
  // to safely talk back to the SyncFrontend.
  MessageLoop* const frontend_loop_;

  Profile* profile_;

  sync_notifier::SyncNotifierFactory sync_notifier_factory_;

  // This is state required to implement ModelSafeWorkerRegistrar.
  struct {
    // We maintain ownership of all workers.  In some cases, we need to ensure
    // shutdown occurs in an expected sequence by Stop()ing certain workers.
    // They are guaranteed to be valid because we only destroy elements of
    // |workers_| after the syncapi has been destroyed.  Unless a worker is no
    // longer needed because all types that get routed to it have been disabled
    // (from syncing). In that case, we'll destroy on demand *after* routing
    // any dependent types to GROUP_PASSIVE, so that the syncapi doesn't call
    // into garbage.  If a key is present, it means at least one ModelType that
    // routes to that model safe group is being synced.
    WorkerMap workers;
    browser_sync::ModelSafeRoutingInfo routing_info;
  } registrar_;

  // The user can incur changes to registrar_ at any time from the UI thread.
  // The syncapi needs to periodically get a consistent snapshot of the state,
  // and it does so from a different thread.  Therefore, we protect creation,
  // destruction, and re-routing events by acquiring this lock.  Note that the
  // SyncBackendHost may read (on the UI thread or sync thread) from registrar_
  // without acquiring the lock (which is typically "read ModelSafeWorker
  // pointer value", and then invoke methods), because lifetimes are managed on
  // the UI thread.  Of course, this comment only applies to ModelSafeWorker
  // impls that are themselves thread-safe, such as UIModelWorker.
  mutable base::Lock registrar_lock_;

  // The frontend which we serve (and are owned by).
  SyncFrontend* frontend_;

  // The change processors that handle the different data types.
  std::map<syncable::ModelType, ChangeProcessor*> processors_;

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
