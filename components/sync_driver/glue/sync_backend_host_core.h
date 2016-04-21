// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_GLUE_SYNC_BACKEND_HOST_CORE_H_
#define COMPONENTS_SYNC_DRIVER_GLUE_SYNC_BACKEND_HOST_CORE_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/timer/timer.h"
#include "components/invalidation/public/invalidation.h"
#include "components/sync_driver/glue/sync_backend_host_impl.h"
#include "components/sync_driver/system_encryptor.h"
#include "sync/internal_api/public/base/cancelation_signal.h"
#include "sync/internal_api/public/sessions/type_debug_info_observer.h"
#include "sync/internal_api/public/shutdown_reason.h"
#include "sync/internal_api/public/sync_encryption_handler.h"
#include "url/gurl.h"

namespace browser_sync {

class SyncBackendHostImpl;

// Utility struct for holding initialization options.
struct DoInitializeOptions {
  DoInitializeOptions(
      base::MessageLoop* sync_loop,
      SyncBackendRegistrar* registrar,
      const syncer::ModelSafeRoutingInfo& routing_info,
      const std::vector<scoped_refptr<syncer::ModelSafeWorker>>& workers,
      const scoped_refptr<syncer::ExtensionsActivity>& extensions_activity,
      const syncer::WeakHandle<syncer::JsEventHandler>& event_handler,
      const GURL& service_url,
      const std::string& sync_user_agent,
      std::unique_ptr<syncer::HttpPostProviderFactory> http_bridge_factory,
      const syncer::SyncCredentials& credentials,
      const std::string& invalidator_client_id,
      std::unique_ptr<syncer::SyncManagerFactory> sync_manager_factory,
      bool delete_sync_data_folder,
      const std::string& restored_key_for_bootstrapping,
      const std::string& restored_keystore_key_for_bootstrapping,
      std::unique_ptr<syncer::InternalComponentsFactory>
          internal_components_factory,
      const syncer::WeakHandle<syncer::UnrecoverableErrorHandler>&
          unrecoverable_error_handler,
      const base::Closure& report_unrecoverable_error_function,
      std::unique_ptr<syncer::SyncEncryptionHandler::NigoriState>
          saved_nigori_state,
      syncer::PassphraseTransitionClearDataOption clear_data_option,
      const std::map<syncer::ModelType, int64_t>& invalidation_versions);
  ~DoInitializeOptions();

  base::MessageLoop* sync_loop;
  SyncBackendRegistrar* registrar;
  syncer::ModelSafeRoutingInfo routing_info;
  std::vector<scoped_refptr<syncer::ModelSafeWorker> > workers;
  scoped_refptr<syncer::ExtensionsActivity> extensions_activity;
  syncer::WeakHandle<syncer::JsEventHandler> event_handler;
  GURL service_url;
  std::string sync_user_agent;
  // Overridden by tests.
  std::unique_ptr<syncer::HttpPostProviderFactory> http_bridge_factory;
  syncer::SyncCredentials credentials;
  const std::string invalidator_client_id;
  std::unique_ptr<syncer::SyncManagerFactory> sync_manager_factory;
  std::string lsid;
  bool delete_sync_data_folder;
  std::string restored_key_for_bootstrapping;
  std::string restored_keystore_key_for_bootstrapping;
  std::unique_ptr<syncer::InternalComponentsFactory>
      internal_components_factory;
  const syncer::WeakHandle<syncer::UnrecoverableErrorHandler>
      unrecoverable_error_handler;
  base::Closure report_unrecoverable_error_function;
  std::unique_ptr<syncer::SyncEncryptionHandler::NigoriState>
      saved_nigori_state;
  const syncer::PassphraseTransitionClearDataOption clear_data_option;
  const std::map<syncer::ModelType, int64_t> invalidation_versions;
};

// Helper struct to handle currying params to
// SyncBackendHost::Core::DoConfigureSyncer.
struct DoConfigureSyncerTypes {
  DoConfigureSyncerTypes();
  DoConfigureSyncerTypes(const DoConfigureSyncerTypes& other);
  ~DoConfigureSyncerTypes();
  syncer::ModelTypeSet to_download;
  syncer::ModelTypeSet to_purge;
  syncer::ModelTypeSet to_journal;
  syncer::ModelTypeSet to_unapply;
};

class SyncBackendHostCore
    : public base::RefCountedThreadSafe<SyncBackendHostCore>,
      public syncer::SyncEncryptionHandler::Observer,
      public syncer::SyncManager::Observer,
      public syncer::TypeDebugInfoObserver {
 public:
  SyncBackendHostCore(const std::string& name,
       const base::FilePath& sync_data_folder_path,
       bool has_sync_setup_completed,
       const base::WeakPtr<SyncBackendHostImpl>& backend);

  // SyncManager::Observer implementation.  The Core just acts like an air
  // traffic controller here, forwarding incoming messages to appropriate
  // landing threads.
  void OnSyncCycleCompleted(
      const syncer::sessions::SyncSessionSnapshot& snapshot) override;
  void OnInitializationComplete(
      const syncer::WeakHandle<syncer::JsBackend>& js_backend,
      const syncer::WeakHandle<syncer::DataTypeDebugInfoListener>&
          debug_info_listener,
      bool success,
      syncer::ModelTypeSet restored_types) override;
  void OnConnectionStatusChange(syncer::ConnectionStatus status) override;
  void OnActionableError(const syncer::SyncProtocolError& sync_error) override;
  void OnMigrationRequested(syncer::ModelTypeSet types) override;
  void OnProtocolEvent(const syncer::ProtocolEvent& event) override;

  // SyncEncryptionHandler::Observer implementation.
  void OnPassphraseRequired(
      syncer::PassphraseRequiredReason reason,
      const sync_pb::EncryptedData& pending_keys) override;
  void OnPassphraseAccepted() override;
  void OnBootstrapTokenUpdated(const std::string& bootstrap_token,
                               syncer::BootstrapTokenType type) override;
  void OnEncryptedTypesChanged(syncer::ModelTypeSet encrypted_types,
                               bool encrypt_everything) override;
  void OnEncryptionComplete() override;
  void OnCryptographerStateChanged(
      syncer::Cryptographer* cryptographer) override;
  void OnPassphraseTypeChanged(syncer::PassphraseType type,
                               base::Time passphrase_time) override;
  void OnLocalSetPassphraseEncryption(
      const syncer::SyncEncryptionHandler::NigoriState& nigori_state) override;

  // TypeDebugInfoObserver implementation
  void OnCommitCountersUpdated(syncer::ModelType type,
                               const syncer::CommitCounters& counters) override;
  void OnUpdateCountersUpdated(syncer::ModelType type,
                               const syncer::UpdateCounters& counters) override;
  void OnStatusCountersUpdated(syncer::ModelType type,
                               const syncer::StatusCounters& counters) override;

  // Forwards an invalidation state change to the sync manager.
  void DoOnInvalidatorStateChange(syncer::InvalidatorState state);

  // Forwards an invalidation to the sync manager.
  void DoOnIncomingInvalidation(
      const syncer::ObjectIdInvalidationMap& invalidation_map);

  // Note:
  //
  // The Do* methods are the various entry points from our
  // SyncBackendHost.  They are all called on the sync thread to
  // actually perform synchronous (and potentially blocking) syncapi
  // operations.
  //
  // Called to perform initialization of the syncapi on behalf of
  // SyncBackendHost::Initialize.
  void DoInitialize(std::unique_ptr<DoInitializeOptions> options);

  // Called to perform credential update on behalf of
  // SyncBackendHost::UpdateCredentials.
  void DoUpdateCredentials(const syncer::SyncCredentials& credentials);

  // Called to tell the syncapi to start syncing (generally after
  // initialization and authentication).
  void DoStartSyncing(const syncer::ModelSafeRoutingInfo& routing_info,
                      base::Time last_poll_time);

  // Called to set the passphrase for encryption.
  void DoSetEncryptionPassphrase(const std::string& passphrase,
                                 bool is_explicit);

  // Called to decrypt the pending keys.
  void DoSetDecryptionPassphrase(const std::string& passphrase);

  // Called to turn on encryption of all sync data as well as
  // reencrypt everything.
  void DoEnableEncryptEverything();

  // Ask the syncer to check for updates for the specified types.
  void DoRefreshTypes(syncer::ModelTypeSet types);

  // Invoked if we failed to download the necessary control types at startup.
  // Invokes SyncBackendHost::HandleControlTypesDownloadRetry.
  void OnControlTypesDownloadRetry();

  // Called to perform tasks which require the control data to be downloaded.
  // This includes refreshing encryption, etc.
  void DoInitialProcessControlTypes();

  // The shutdown order is a bit complicated:
  // 1) Call ShutdownOnUIThread() from |frontend_loop_| to request sync manager
  //    to stop as soon as possible.
  // 2) Post DoShutdown() to sync loop to clean up backend state, save
  //    directory and destroy sync manager.
  void ShutdownOnUIThread();
  void DoShutdown(syncer::ShutdownReason reason);
  void DoDestroySyncManager(syncer::ShutdownReason reason);

  // Configuration methods that must execute on sync loop.
  void DoConfigureSyncer(
      syncer::ConfigureReason reason,
      const DoConfigureSyncerTypes& config_types,
      const syncer::ModelSafeRoutingInfo routing_info,
      const base::Callback<void(syncer::ModelTypeSet,
                                syncer::ModelTypeSet)>& ready_task,
      const base::Closure& retry_callback);
  void DoFinishConfigureDataTypes(
      syncer::ModelTypeSet types_to_config,
      const base::Callback<void(syncer::ModelTypeSet,
                                syncer::ModelTypeSet)>& ready_task);
  void DoRetryConfiguration(
      const base::Closure& retry_callback);

  // Set the base request context to use when making HTTP calls.
  // This method will add a reference to the context to persist it
  // on the IO thread. Must be removed from IO thread.

  syncer::SyncManager* sync_manager() { return sync_manager_.get(); }

  void SendBufferedProtocolEventsAndEnableForwarding();
  void DisableProtocolEventForwarding();

  // Enables the forwarding of directory type debug counters to the
  // SyncBackendHost.  Also requests that updates to all counters be
  // emitted right away to initialize any new listeners' states.
  void EnableDirectoryTypeDebugInfoForwarding();

  // Disables forwarding of directory type debug counters.
  void DisableDirectoryTypeDebugInfoForwarding();

  // Delete the sync data folder to cleanup backend data.  Happens the first
  // time sync is enabled for a user (to prevent accidentally reusing old
  // sync databases), as well as shutdown when you're no longer syncing.
  void DeleteSyncDataFolder();

  // We expose this member because it's required in the construction of the
  // HttpBridgeFactory.
  syncer::CancelationSignal* GetRequestContextCancelationSignal() {
    return &release_request_context_signal_;
  }

  void GetAllNodesForTypes(
      syncer::ModelTypeSet types,
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      base::Callback<void(const std::vector<syncer::ModelType>& type,
                          ScopedVector<base::ListValue>) > callback);

  // Tell the sync manager to persist its state by writing to disk.
  // Called on the sync thread, both by a timer and, on Android, when the
  // application is backgrounded.
  void SaveChanges();

  void DoClearServerData(
      const syncer::SyncManager::ClearServerDataCallback& frontend_callback);

  // Notify the syncer that the cookie jar has changed.
  void DoOnCookieJarChanged(bool account_mismatch);

 private:
  friend class base::RefCountedThreadSafe<SyncBackendHostCore>;
  friend class SyncBackendHostForProfileSyncTest;

  ~SyncBackendHostCore() override;

  // Invoked when initialization of syncapi is complete and we can start
  // our timer.
  // This must be called from the thread on which SaveChanges is intended to
  // be run on; the host's |registrar_->sync_thread()|.
  void StartSavingChanges();

  void ClearServerDataDone(const base::Closure& frontend_callback);

  // Name used for debugging.
  const std::string name_;

  // Path of the folder that stores the sync data files.
  const base::FilePath sync_data_folder_path_;

  // Our parent SyncBackendHost.
  syncer::WeakHandle<SyncBackendHostImpl> host_;

  // The loop where all the sync backend operations happen.
  // Non-NULL only between calls to DoInitialize() and ~Core().
  base::MessageLoop* sync_loop_;

  // Our parent's registrar (not owned).  Non-NULL only between
  // calls to DoInitialize() and DoShutdown().
  SyncBackendRegistrar* registrar_;

  // The timer used to periodically call SaveChanges.
  std::unique_ptr<base::RepeatingTimer> save_changes_timer_;

  // Our encryptor, which uses Chrome's encryption functions.
  sync_driver::SystemEncryptor encryptor_;

  // The top-level syncapi entry point.  Lives on the sync thread.
  std::unique_ptr<syncer::SyncManager> sync_manager_;

  // Temporary holder of sync manager's initialization results. Set by
  // OnInitializeComplete, and consumed when we pass it via OnBackendInitialized
  // in the final state of HandleInitializationSuccessOnFrontendLoop.
  syncer::WeakHandle<syncer::JsBackend> js_backend_;
  syncer::WeakHandle<syncer::DataTypeDebugInfoListener> debug_info_listener_;

  // These signals allow us to send requests to shut down the HttpBridgeFactory
  // and ServerConnectionManager without having to wait for those classes to
  // finish initializing first.
  //
  // See comments in SyncBackendHostCore::ShutdownOnUIThread() for more details.
  syncer::CancelationSignal release_request_context_signal_;
  syncer::CancelationSignal stop_syncing_signal_;

  // Matches the value of SyncPref's IsFirstSetupComplete() flag at init time.
  // Should not be used for anything except for UMAs and logging.
  const bool has_sync_setup_completed_;

  // Set when we've been asked to forward sync protocol events to the frontend.
  bool forward_protocol_events_;

  // Set when the forwarding of per-type debug counters is enabled.
  bool forward_type_info_;

  // A map of data type -> invalidation version to track the most recently
  // received invalidation version for each type.
  // This allows dropping any invalidations with versions older than those
  // most recently received for that data type.
  std::map<syncer::ModelType, int64_t> last_invalidation_versions_;

  base::WeakPtrFactory<SyncBackendHostCore> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SyncBackendHostCore);
};

}  // namespace browser_sync

#endif  // COMPONENTS_SYNC_DRIVER_GLUE_SYNC_BACKEND_HOST_CORE_H_
