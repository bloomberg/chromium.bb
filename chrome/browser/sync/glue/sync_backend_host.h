// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_SYNC_BACKEND_HOST_H_
#define CHROME_BROWSER_SYNC_GLUE_SYNC_BACKEND_HOST_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread.h"
#include "chrome/browser/sync/glue/backend_data_type_configurer.h"
#include "chrome/browser/sync/glue/chrome_extensions_activity_monitor.h"
#include "chrome/browser/sync/glue/chrome_sync_notification_bridge.h"
#include "chrome/common/net/gaia/google_service_auth_error.h"
#include "googleurl/src/gurl.h"
#include "sync/internal_api/configure_reason.h"
#include "sync/internal_api/public/engine/model_safe_worker.h"
#include "sync/internal_api/public/sessions/sync_session_snapshot.h"
#include "sync/internal_api/public/syncable/model_type.h"
#include "sync/internal_api/sync_manager.h"
#include "sync/notifier/sync_notifier_factory.h"
#include "sync/protocol/encryption.pb.h"
#include "sync/protocol/sync_protocol_error.h"
#include "sync/util/report_unrecoverable_error_function.h"
#include "sync/util/unrecoverable_error_handler.h"
#include "sync/util/weak_handle.h"

class MessageLoop;
class Profile;

namespace browser_sync {

class ChangeProcessor;
struct Experiments;
class InvalidatorStorage;
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

  // Configure ran into some kind of error. But it is scheduled to be
  // retried.
  virtual void OnSyncConfigureRetry() = 0;

  // The status of the connection to the sync server has changed.
  virtual void OnConnectionStatusChange(
      sync_api::ConnectionStatus status) = 0;

  // We are no longer permitted to communicate with the server. Sync should
  // be disabled and state cleaned up at once.
  virtual void OnStopSyncingPermanently() = 0;

  // Called to handle success/failure of clearing server data
  virtual void OnClearServerDataSucceeded() = 0;
  virtual void OnClearServerDataFailed() = 0;

  // The syncer requires a passphrase to decrypt sensitive updates. This is
  // called when the first sensitive data type is setup by the user and anytime
  // the passphrase is changed by another synced client. |reason| denotes why
  // the passphrase was required. |pending_keys| is a copy of the
  // cryptographer's pending keys to be passed on to the frontend in order to
  // be cached.
  virtual void OnPassphraseRequired(
      sync_api::PassphraseRequiredReason reason,
      const sync_pb::EncryptedData& pending_keys) = 0;

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
      syncable::ModelTypeSet encrypted_types,
      bool encrypt_everything) = 0;

  // Called after we finish encrypting the current set of encrypted
  // types.
  virtual void OnEncryptionComplete() = 0;

  // Called to perform migration of |types|.
  virtual void OnMigrationNeededForTypes(
      syncable::ModelTypeSet types) = 0;

  // Inform the Frontend that new datatypes are available for registration.
  virtual void OnExperimentsChanged(
      const browser_sync::Experiments& experiments) = 0;

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
class SyncBackendHost : public BackendDataTypeConfigurer {
 public:
  typedef sync_api::SyncStatus Status;

  // Create a SyncBackendHost with a reference to the |frontend| that
  // it serves and communicates to via the SyncFrontend interface (on
  // the same thread it used to call the constructor).  Must outlive
  // |sync_prefs| and |invalidator_storage|.
  SyncBackendHost(
      const std::string& name,
      Profile* profile,
      const base::WeakPtr<SyncPrefs>& sync_prefs,
      // TODO(tim): Temporary, remove when bug 124137 finished.
      const base::WeakPtr<InvalidatorStorage>& invalidator_storage);

  // For testing.
  // TODO(skrul): Extract an interface so this is not needed.
  explicit SyncBackendHost(Profile* profile);
  virtual ~SyncBackendHost();

  // Called on |frontend_loop_| to kick off asynchronous initialization.
  // As a fallback when no cached auth information is available, try to
  // bootstrap authentication using |lsid|, if it isn't empty.
  // Optionally delete the Sync Data folder (if it's corrupt).
  // |report_unrecoverable_error_function| can be NULL.
  // Note: |unrecoverable_error_handler| may be invoked from any thread.
  void Initialize(SyncFrontend* frontend,
                  const WeakHandle<JsEventHandler>& event_handler,
                  const GURL& service_url,
                  syncable::ModelTypeSet initial_types,
                  const sync_api::SyncCredentials& credentials,
                  bool delete_sync_data_folder,
                  UnrecoverableErrorHandler* unrecoverable_error_handler,
                  ReportUnrecoverableErrorFunction
                      report_unrecoverable_error_function);

  // Called from |frontend_loop| to update SyncCredentials.
  void UpdateCredentials(const sync_api::SyncCredentials& credentials);

  // This starts the SyncerThread running a Syncer object to communicate with
  // sync servers.  Until this is called, no changes will leave or enter this
  // browser from the cloud / sync servers.
  // Called on |frontend_loop_|.
  virtual void StartSyncingWithServer();

  // Called on |frontend_loop_| to asynchronously set a new passphrase for
  // encryption. Note that it is an error to call SetEncryptionPassphrase under
  // the following circumstances:
  // - An explicit passphrase has already been set
  // - |is_explicit| is true and we have pending keys.
  // When |is_explicit| is false, a couple of things could happen:
  // - If there are pending keys, we try to decrypt them. If decryption works,
  //   this acts like a call to SetDecryptionPassphrase. If not, the GAIA
  //   passphrase passed in is cached so we can re-encrypt with it in future.
  // - If there are no pending keys, data is encrypted with |passphrase| (this
  //   is a no-op if data was already encrypted with |passphrase|.)
  void SetEncryptionPassphrase(const std::string& passphrase, bool is_explicit);

  // Called on |frontend_loop_| to use the provided passphrase to asynchronously
  // attempt decryption. Returns false immediately if the passphrase could not
  // be used to decrypt a locally cached copy of encrypted keys; returns true
  // otherwise. If new encrypted keys arrive during the asynchronous call,
  // OnPassphraseRequired may be triggered at a later time. It is an error to
  // call this when there are no pending keys.
  bool SetDecryptionPassphrase(const std::string& passphrase)
      WARN_UNUSED_RESULT;

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
      sync_api::ConfigureReason reason,
      syncable::ModelTypeSet types_to_add,
      syncable::ModelTypeSet types_to_remove,
      NigoriState nigori_state,
      base::Callback<void(syncable::ModelTypeSet)> ready_task,
      base::Callback<void()> retry_callback) OVERRIDE;

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
  sessions::SyncSessionSnapshot GetLastSessionSnapshot() const;

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
  // The types and functions below are protected so that test
  // subclasses can use them.
  //
  // TODO(akalin): Figure out a better way for tests to hook into
  // SyncBackendHost.

  typedef base::Callback<sync_api::HttpPostProviderFactory*(void)>
      MakeHttpBridgeFactoryFn;

  struct DoInitializeOptions {
    DoInitializeOptions(
        MessageLoop* sync_loop,
        SyncBackendRegistrar* registrar,
        const ModelSafeRoutingInfo& routing_info,
        const std::vector<ModelSafeWorker*>& workers,
        ExtensionsActivityMonitor* extensions_activity_monitor,
        const WeakHandle<JsEventHandler>& event_handler,
        const GURL& service_url,
        MakeHttpBridgeFactoryFn make_http_bridge_factory_fn,
        const sync_api::SyncCredentials& credentials,
        ChromeSyncNotificationBridge* chrome_sync_notification_bridge,
        sync_notifier::SyncNotifierFactory* sync_notifier_factory,
        bool delete_sync_data_folder,
        const std::string& restored_key_for_bootstrapping,
        sync_api::SyncManager::TestingMode testing_mode,
        UnrecoverableErrorHandler* unrecoverable_error_handler,
        ReportUnrecoverableErrorFunction report_unrecoverable_error_function);
    ~DoInitializeOptions();

    MessageLoop* sync_loop;
    SyncBackendRegistrar* registrar;
    ModelSafeRoutingInfo routing_info;
    std::vector<ModelSafeWorker*> workers;
    ExtensionsActivityMonitor* extensions_activity_monitor;
    WeakHandle<JsEventHandler> event_handler;
    GURL service_url;
    // Overridden by tests.
    MakeHttpBridgeFactoryFn make_http_bridge_factory_fn;
    sync_api::SyncCredentials credentials;
    ChromeSyncNotificationBridge* const chrome_sync_notification_bridge;
    sync_notifier::SyncNotifierFactory* const sync_notifier_factory;
    std::string lsid;
    bool delete_sync_data_folder;
    std::string restored_key_for_bootstrapping;
    sync_api::SyncManager::TestingMode testing_mode;
    UnrecoverableErrorHandler* unrecoverable_error_handler;
    ReportUnrecoverableErrorFunction report_unrecoverable_error_function;
  };

  // Allows tests to perform alternate core initialization work.
  virtual void InitCore(const DoInitializeOptions& options);

  // Called from Core::OnSyncCycleCompleted to handle updating frontend
  // thread components.
  void HandleSyncCycleCompletedOnFrontendLoop(
      const sessions::SyncSessionSnapshot& snapshot);

  // Called to finish the job of ConfigureDataTypes once the syncer is in
  // configuration mode.
  void FinishConfigureDataTypesOnFrontendLoop();

  bool IsDownloadingNigoriForTest() const;

 private:
  // The real guts of SyncBackendHost, to keep the public client API clean.
  class Core;

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
    REFRESHING_NIGORI,      // The SyncManager is initialized, and we
                            // have the encryption information, but we
                            // still need to refresh encryption. Also, we need
                            // to update the device information in the nigori.
    INITIALIZED,            // Initialization is complete.
  };

  struct PendingConfigureDataTypesState {
    PendingConfigureDataTypesState();
    ~PendingConfigureDataTypesState();

    // The ready_task will be run when configuration is done with the
    // set of all types that failed configuration (i.e., if its
    // argument is non-empty, then an error was encountered).
    base::Callback<void(syncable::ModelTypeSet)> ready_task;

    // The retry callback will be run when the download failed due to a
    // transient error. This is to notify DTM so it can apropriately inform
    // the UI. Note: The retry_callback will be run only once and after
    // that we will not notify DTM until the sync is successful or in a
    // permanent error state.
    base::Callback<void()> retry_callback;

    // The set of types that we are waiting to be initially synced in a
    // configuration cycle.
    syncable::ModelTypeSet types_to_add;

    // Additional details about which types were added.
    syncable::ModelTypeSet added_types;
    sync_api::ConfigureReason reason;
    bool retry_in_progress;
  };

  // Checks if we have received a notice to turn on experimental datatypes
  // (via the nigori node) and informs the frontend if that is the case.
  // Note: it is illegal to call this before the backend is initialized.
  void AddExperimentalTypes();

  // Downloading of nigori failed and will be retried.
  virtual void OnNigoriDownloadRetry();

  // InitializationComplete passes through the SyncBackendHost to forward
  // on to |frontend_|, and so that tests can intercept here if they need to
  // set up initial conditions.
  virtual void HandleInitializationCompletedOnFrontendLoop(
      const WeakHandle<JsBackend>& js_backend,
      bool success);

  // Helpers to persist a token that can be used to bootstrap sync encryption
  // across browser restart to avoid requiring the user to re-enter their
  // passphrase.  |token| must be valid UTF-8 as we use the PrefService for
  // storage.
  void PersistEncryptionBootstrapToken(const std::string& token);

  // For convenience, checks if initialization state is INITIALIZED.
  bool initialized() const { return initialization_state_ == INITIALIZED; }

  // Let the front end handle the actionable error event.
  void HandleActionableErrorEventOnFrontendLoop(
      const browser_sync::SyncProtocolError& sync_error);

  // Checks if |passphrase| can be used to decrypt the cryptographer's pending
  // keys that were cached during NotifyPassphraseRequired. Returns true if
  // decryption was successful. Returns false otherwise. Must be called with a
  // non-empty pending keys cache.
  bool CheckPassphraseAgainstCachedPendingKeys(
      const std::string& passphrase) const;

  // Invoked when a passphrase is required to decrypt a set of Nigori keys,
  // or for encrypting. |reason| denotes why the passphrase was required.
  // |pending_keys| is a copy of the cryptographer's pending keys, that are
  // cached by the frontend. If there are no pending keys, or if the passphrase
  // required reason is REASON_ENCRYPTION, an empty EncryptedData object is
  // passed.
  void NotifyPassphraseRequired(sync_api::PassphraseRequiredReason reason,
                                sync_pb::EncryptedData pending_keys);

  // Invoked when the passphrase provided by the user has been accepted.
  void NotifyPassphraseAccepted();

  // Invoked when an updated token is available from the sync server.
  void NotifyUpdatedToken(const std::string& token);

  // Invoked when the set of encrypted types or the encrypt
  // everything flag changes.
  void NotifyEncryptedTypesChanged(
      syncable::ModelTypeSet encrypted_types,
      bool encrypt_everything);

  // Invoked when sync finishes encrypting new datatypes.
  void NotifyEncryptionComplete();

  void HandleStopSyncingPermanentlyOnFrontendLoop();

  // Called to handle success/failure of clearing server data
  void HandleClearServerDataSucceededOnFrontendLoop();
  void HandleClearServerDataFailedOnFrontendLoop();

  // Dispatched to from OnConnectionStatusChange to handle updating
  // frontend UI components.
  void HandleConnectionStatusChangeOnFrontendLoop(
      sync_api::ConnectionStatus status);

  // Called when configuration of the Nigori node has completed as
  // part of the initialization process.
  void HandleNigoriConfigurationCompletedOnFrontendLoop(
      const WeakHandle<JsBackend>& js_backend,
      syncable::ModelTypeSet failed_configuration_types);

  // Must be called on |frontend_loop_|.  |done_callback| is called on
  // |frontend_loop_|.
  void RefreshNigori(const base::Closure& done_callback);

  // Handles stopping the core's SyncManager, accounting for whether
  // initialization is done yet.
  void StopSyncManagerForShutdown(const base::Closure& closure);

  base::WeakPtrFactory<SyncBackendHost> weak_ptr_factory_;

  // A thread where all the sync operations happen.
  base::Thread sync_thread_;

  // A reference to the MessageLoop used to construct |this|, so we know how
  // to safely talk back to the SyncFrontend.
  MessageLoop* const frontend_loop_;

  Profile* const profile_;

  // Name used for debugging (set from profile_->GetDebugName()).
  const std::string name_;

  // Our core, which communicates directly to the syncapi.
  scoped_refptr<Core> core_;

  InitializationState initialization_state_;

  const base::WeakPtr<SyncPrefs> sync_prefs_;

  // A thread-safe listener for handling notifications triggered by
  // chrome events.
  ChromeSyncNotificationBridge chrome_sync_notification_bridge_;

  sync_notifier::SyncNotifierFactory sync_notifier_factory_;

  ChromeExtensionsActivityMonitor extensions_activity_monitor_;

  scoped_ptr<SyncBackendRegistrar> registrar_;

  // The frontend which we serve (and are owned by).
  SyncFrontend* frontend_;

  scoped_ptr<PendingConfigureDataTypesState> pending_download_state_;
  scoped_ptr<PendingConfigureDataTypesState> pending_config_mode_state_;

  // We cache the cryptographer's pending keys whenever NotifyPassphraseRequired
  // is called. This way, before the UI calls SetDecryptionPassphrase on the
  // syncer, it can avoid the overhead of an asynchronous decryption call and
  // give the user immediate feedback about the passphrase entered by first
  // trying to decrypt the cached pending keys on the UI thread. Note that
  // SetDecryptionPassphrase can still fail after the cached pending keys are
  // successfully decrypted if the pending keys have changed since the time they
  // were cached.
  sync_pb::EncryptedData cached_pending_keys_;

  // UI-thread cache of the last SyncSessionSnapshot received from syncapi.
  sessions::SyncSessionSnapshot last_snapshot_;

  DISALLOW_COPY_AND_ASSIGN(SyncBackendHost);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_SYNC_BACKEND_HOST_H_
