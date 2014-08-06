// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_SYNC_BACKEND_HOST_CORE_H_
#define CHROME_BROWSER_SYNC_GLUE_SYNC_BACKEND_HOST_CORE_H_

#include "base/memory/ref_counted.h"

#include "base/timer/timer.h"
#include "chrome/browser/sync/glue/sync_backend_host_impl.h"
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
      const std::vector<scoped_refptr<syncer::ModelSafeWorker> >& workers,
      const scoped_refptr<syncer::ExtensionsActivity>& extensions_activity,
      const syncer::WeakHandle<syncer::JsEventHandler>& event_handler,
      const GURL& service_url,
      scoped_ptr<syncer::HttpPostProviderFactory> http_bridge_factory,
      const syncer::SyncCredentials& credentials,
      const std::string& invalidator_client_id,
      scoped_ptr<syncer::SyncManagerFactory> sync_manager_factory,
      bool delete_sync_data_folder,
      const std::string& restored_key_for_bootstrapping,
      const std::string& restored_keystore_key_for_bootstrapping,
      scoped_ptr<syncer::InternalComponentsFactory> internal_components_factory,
      scoped_ptr<syncer::UnrecoverableErrorHandler> unrecoverable_error_handler,
      syncer::ReportUnrecoverableErrorFunction
          report_unrecoverable_error_function,
      const std::string& signin_scoped_device_id);
  ~DoInitializeOptions();

  base::MessageLoop* sync_loop;
  SyncBackendRegistrar* registrar;
  syncer::ModelSafeRoutingInfo routing_info;
  std::vector<scoped_refptr<syncer::ModelSafeWorker> > workers;
  scoped_refptr<syncer::ExtensionsActivity> extensions_activity;
  syncer::WeakHandle<syncer::JsEventHandler> event_handler;
  GURL service_url;
  // Overridden by tests.
  scoped_ptr<syncer::HttpPostProviderFactory> http_bridge_factory;
  syncer::SyncCredentials credentials;
  const std::string invalidator_client_id;
  scoped_ptr<syncer::SyncManagerFactory> sync_manager_factory;
  std::string lsid;
  bool delete_sync_data_folder;
  std::string restored_key_for_bootstrapping;
  std::string restored_keystore_key_for_bootstrapping;
  scoped_ptr<syncer::InternalComponentsFactory> internal_components_factory;
  scoped_ptr<syncer::UnrecoverableErrorHandler> unrecoverable_error_handler;
  syncer::ReportUnrecoverableErrorFunction
      report_unrecoverable_error_function;
  std::string signin_scoped_device_id;
};

// Helper struct to handle currying params to
// SyncBackendHost::Core::DoConfigureSyncer.
struct DoConfigureSyncerTypes {
  DoConfigureSyncerTypes();
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
  virtual void OnSyncCycleCompleted(
      const syncer::sessions::SyncSessionSnapshot& snapshot) OVERRIDE;
  virtual void OnInitializationComplete(
      const syncer::WeakHandle<syncer::JsBackend>& js_backend,
      const syncer::WeakHandle<syncer::DataTypeDebugInfoListener>&
          debug_info_listener,
      bool success,
      syncer::ModelTypeSet restored_types) OVERRIDE;
  virtual void OnConnectionStatusChange(
      syncer::ConnectionStatus status) OVERRIDE;
  virtual void OnActionableError(
      const syncer::SyncProtocolError& sync_error) OVERRIDE;
  virtual void OnMigrationRequested(syncer::ModelTypeSet types) OVERRIDE;
  virtual void OnProtocolEvent(const syncer::ProtocolEvent& event) OVERRIDE;

  // SyncEncryptionHandler::Observer implementation.
  virtual void OnPassphraseRequired(
      syncer::PassphraseRequiredReason reason,
      const sync_pb::EncryptedData& pending_keys) OVERRIDE;
  virtual void OnPassphraseAccepted() OVERRIDE;
  virtual void OnBootstrapTokenUpdated(
      const std::string& bootstrap_token,
      syncer::BootstrapTokenType type) OVERRIDE;
  virtual void OnEncryptedTypesChanged(
      syncer::ModelTypeSet encrypted_types,
      bool encrypt_everything) OVERRIDE;
  virtual void OnEncryptionComplete() OVERRIDE;
  virtual void OnCryptographerStateChanged(
      syncer::Cryptographer* cryptographer) OVERRIDE;
  virtual void OnPassphraseTypeChanged(syncer::PassphraseType type,
                                       base::Time passphrase_time) OVERRIDE;

  // TypeDebugInfoObserver implementation
  virtual void OnCommitCountersUpdated(
      syncer::ModelType type,
      const syncer::CommitCounters& counters) OVERRIDE;
  virtual void OnUpdateCountersUpdated(
      syncer::ModelType type,
      const syncer::UpdateCounters& counters) OVERRIDE;
  virtual void OnStatusCountersUpdated(
      syncer::ModelType type,
      const syncer::StatusCounters& counters) OVERRIDE;

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
  void DoInitialize(scoped_ptr<DoInitializeOptions> options);

  // Called to perform credential update on behalf of
  // SyncBackendHost::UpdateCredentials.
  void DoUpdateCredentials(const syncer::SyncCredentials& credentials);

  // Called to tell the syncapi to start syncing (generally after
  // initialization and authentication).
  void DoStartSyncing(const syncer::ModelSafeRoutingInfo& routing_info);

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
  // This includes refreshing encryption, setting up the device info change
  // processor, etc.
  void DoInitialProcessControlTypes();

  // Some parts of DoInitialProcessControlTypes() may be executed on a different
  // thread.  This function asynchronously continues the work started in
  // DoInitialProcessControlTypes() once that other thread gets back to us.
  void DoFinishInitialProcessControlTypes();

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

  SyncedDeviceTracker* synced_device_tracker() {
    return synced_device_tracker_.get();
  }

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

 private:
  friend class base::RefCountedThreadSafe<SyncBackendHostCore>;
  friend class SyncBackendHostForProfileSyncTest;

  virtual ~SyncBackendHostCore();

  // Invoked when initialization of syncapi is complete and we can start
  // our timer.
  // This must be called from the thread on which SaveChanges is intended to
  // be run on; the host's |registrar_->sync_thread()|.
  void StartSavingChanges();

  // Invoked periodically to tell the syncapi to persist its state
  // by writing to disk.
  // This is called from the thread we were created on (which is sync thread),
  // using a repeating timer that is kicked off as soon as the SyncManager
  // tells us it completed initialization.
  void SaveChanges();

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
  scoped_ptr<base::RepeatingTimer<SyncBackendHostCore> > save_changes_timer_;

  // Our encryptor, which uses Chrome's encryption functions.
  sync_driver::SystemEncryptor encryptor_;

  // A special ChangeProcessor that tracks the DEVICE_INFO type for us.
  scoped_ptr<SyncedDeviceTracker> synced_device_tracker_;

  // The top-level syncapi entry point.  Lives on the sync thread.
  scoped_ptr<syncer::SyncManager> sync_manager_;

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

  // Matches the value of SyncPref's HasSyncSetupCompleted() flag at init time.
  // Should not be used for anything except for UMAs and logging.
  const bool has_sync_setup_completed_;

  // Set when we've been asked to forward sync protocol events to the frontend.
  bool forward_protocol_events_;

  // Set when the forwarding of per-type debug counters is enabled.
  bool forward_type_info_;

  // Obtained from SigninClient::GetSigninScopedDeviceId(). Stored here just to
  // pass from SyncBackendHostImpl to SyncedDeviceTracker.
  std::string signin_scoped_device_id_;

  base::WeakPtrFactory<SyncBackendHostCore> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SyncBackendHostCore);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_SYNC_BACKEND_HOST_CORE_H_
