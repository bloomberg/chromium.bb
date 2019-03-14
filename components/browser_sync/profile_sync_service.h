// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_SYNC_PROFILE_SYNC_SERVICE_H_
#define COMPONENTS_BROWSER_SYNC_PROFILE_SYNC_SERVICE_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/location.h"
#include "base/macros.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "components/invalidation/public/identity_provider.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/sync_prefs.h"
#include "components/sync/base/unrecoverable_error_handler.h"
#include "components/sync/driver/data_type_controller.h"
#include "components/sync/driver/data_type_manager.h"
#include "components/sync/driver/data_type_manager_observer.h"
#include "components/sync/driver/data_type_status_table.h"
#include "components/sync/driver/startup_controller.h"
#include "components/sync/driver/sync_client.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_service_crypto.h"
#include "components/sync/driver/sync_stopped_reporter.h"
#include "components/sync/driver/sync_user_settings_impl.h"
#include "components/sync/engine/configure_reason.h"
#include "components/sync/engine/events/protocol_event_observer.h"
#include "components/sync/engine/net/network_time_update_callback.h"
#include "components/sync/engine/shutdown_reason.h"
#include "components/sync/engine/sync_engine.h"
#include "components/sync/engine/sync_engine_host.h"
#include "components/sync/js/sync_js_controller.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "services/identity/public/cpp/identity_manager.h"
#include "url/gurl.h"

namespace network {
class NetworkConnectionTracker;
class SharedURLLoaderFactory;
}  // namespace network

namespace syncer {
class BackendMigrator;
class NetworkResources;
class SyncTypePreferenceProvider;
class TypeDebugInfoObserver;
struct CommitCounters;
struct StatusCounters;
struct UpdateCounters;
struct UserShare;
}  // namespace syncer

namespace browser_sync {

class SyncAuthManager;

// ProfileSyncService is the layer between browser subsystems like bookmarks,
// and the sync engine. Each subsystem is logically thought of as being a sync
// datatype. Individual datatypes can, at any point, be in a variety of stages
// of being "enabled". Here are some specific terms for concepts used in this
// class:
//
//   'Registered' (feature suppression for a datatype)
//
//      When a datatype is registered, the user has the option of syncing it.
//      The sync opt-in UI will show only registered types; a checkbox should
//      never be shown for an unregistered type, nor can it ever be synced.
//
//   'Preferred' (user preferences and opt-out for a datatype)
//
//      This means the user's opt-in or opt-out preference on a per-datatype
//      basis. The sync service will try to make active exactly these types.
//      If a user has opted out of syncing a particular datatype, it will
//      be registered, but not preferred. Also note that not all datatypes can
//      be directly chosen by the user: e.g. AUTOFILL_PROFILE is implied by
//      AUTOFILL but can't be selected separately. If AUTOFILL is chosen by the
//      user, then AUTOFILL_PROFILE will also be considered preferred. See
//      SyncPrefs::ResolvePrefGroups.
//
//      This state is controlled by SyncUserSettings::SetChosenDataTypes. They
//      are stored in the preferences system and persist; though if a datatype
//      is not registered, it cannot be a preferred datatype.
//
//   'Active' (run-time initialization of sync system for a datatype)
//
//      An active datatype is a preferred datatype that is actively being
//      synchronized: the syncer has been instructed to querying the server
//      for this datatype, first-time merges have finished, and there is an
//      actively installed ChangeProcessor that listens for changes to this
//      datatype, propagating such changes into and out of the sync engine
//      as necessary.
//
//      When a datatype is in the process of becoming active, it may be
//      in some intermediate state. Those finer-grained intermediate states
//      are differentiated by the DataTypeController state, but not exposed.
//
// Sync Configuration:
//
//   Sync configuration is accomplished via SyncUserSettings, in particular:
//    * SetChosenDataTypes(): Set the data types the user wants to sync.
//    * SetDecryptionPassphrase(): Attempt to decrypt the user's encrypted data
//        using the passed passphrase.
//    * SetEncryptionPassphrase(): Re-encrypt the user's data using the passed
//        passphrase.
//
// Initial sync setup:
//
//   For privacy reasons, it is usually desirable to avoid syncing any data
//   types until the user has finished setting up sync. There are two APIs
//   that control the initial sync download:
//
//    * SyncUserSettings::SetFirstSetupComplete()
//    * GetSetupInProgressHandle()
//
//   SetFirstSetupComplete() should be called once the user has finished setting
//   up sync at least once on their account. GetSetupInProgressHandle() should
//   be called while the user is actively configuring their account. The handle
//   should be deleted once configuration is complete.
//
//   Once first setup has completed and there are no outstanding
//   setup-in-progress handles, datatype configuration will begin.
class ProfileSyncService : public syncer::SyncService,
                           public syncer::SyncEngineHost,
                           public syncer::SyncPrefObserver,
                           public syncer::DataTypeManagerObserver,
                           public syncer::UnrecoverableErrorHandler,
                           public identity::IdentityManager::Observer {
 public:
  // If AUTO_START, sync will set IsFirstSetupComplete() automatically and sync
  // will begin syncing without the user needing to confirm sync settings.
  enum StartBehavior {
    AUTO_START,
    MANUAL_START,
  };

  // Bundles the arguments for ProfileSyncService construction. This is a
  // movable struct. Because of the non-POD data members, it needs out-of-line
  // constructors, so in particular the move constructor needs to be
  // explicitly defined.
  struct InitParams {
    InitParams();
    InitParams(InitParams&& other);
    ~InitParams();

    std::unique_ptr<syncer::SyncClient> sync_client;
    identity::IdentityManager* identity_manager = nullptr;
    std::vector<invalidation::IdentityProvider*>
        invalidations_identity_providers;
    StartBehavior start_behavior = MANUAL_START;
    syncer::NetworkTimeUpdateCallback network_time_update_callback;
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory;
    network::NetworkConnectionTracker* network_connection_tracker = nullptr;
    std::string debug_identifier;

   private:
    DISALLOW_COPY_AND_ASSIGN(InitParams);
  };

  explicit ProfileSyncService(InitParams init_params);

  ~ProfileSyncService() override;

  // Initializes the object. This must be called at most once, and
  // immediately after an object of this class is constructed.
  // TODO(mastiz): Rename this to Start().
  void Initialize();

  // syncer::SyncService implementation
  syncer::SyncUserSettings* GetUserSettings() override;
  const syncer::SyncUserSettings* GetUserSettings() const override;
  int GetDisableReasons() const override;
  TransportState GetTransportState() const override;
  bool IsLocalSyncEnabled() const override;
  CoreAccountInfo GetAuthenticatedAccountInfo() const override;
  bool IsAuthenticatedAccountPrimary() const override;
  GoogleServiceAuthError GetAuthError() const override;
  std::unique_ptr<syncer::SyncSetupInProgressHandle> GetSetupInProgressHandle()
      override;
  bool IsSetupInProgress() const override;
  syncer::ModelTypeSet GetRegisteredDataTypes() const override;
  syncer::ModelTypeSet GetForcedDataTypes() const override;
  syncer::ModelTypeSet GetPreferredDataTypes() const override;
  syncer::ModelTypeSet GetActiveDataTypes() const override;
  void StopAndClear() override;
  void OnDataTypeRequestsSyncStartup(syncer::ModelType type) override;
  void TriggerRefresh(const syncer::ModelTypeSet& types) override;
  void ReenableDatatype(syncer::ModelType type) override;
  void ReadyForStartChanged(syncer::ModelType type) override;
  void SetInvalidationsForSessionsEnabled(bool enabled) override;
  void AddObserver(syncer::SyncServiceObserver* observer) override;
  void RemoveObserver(syncer::SyncServiceObserver* observer) override;
  bool HasObserver(const syncer::SyncServiceObserver* observer) const override;
  void AddPreferenceProvider(
      syncer::SyncTypePreferenceProvider* provider) override;
  void RemovePreferenceProvider(
      syncer::SyncTypePreferenceProvider* provider) override;
  bool HasPreferenceProvider(
      syncer::SyncTypePreferenceProvider* provider) const override;
  syncer::UserShare* GetUserShare() const override;
  syncer::SyncTokenStatus GetSyncTokenStatus() const override;
  bool QueryDetailedSyncStatus(syncer::SyncStatus* result) const override;
  base::Time GetLastSyncedTime() const override;
  syncer::SyncCycleSnapshot GetLastCycleSnapshot() const override;
  std::unique_ptr<base::Value> GetTypeStatusMap() override;
  const GURL& sync_service_url() const override;
  std::string unrecoverable_error_message() const override;
  base::Location unrecoverable_error_location() const override;
  void AddProtocolEventObserver(
      syncer::ProtocolEventObserver* observer) override;
  void RemoveProtocolEventObserver(
      syncer::ProtocolEventObserver* observer) override;
  void AddTypeDebugInfoObserver(
      syncer::TypeDebugInfoObserver* observer) override;
  void RemoveTypeDebugInfoObserver(
      syncer::TypeDebugInfoObserver* observer) override;
  base::WeakPtr<syncer::JsController> GetJsController() override;
  void GetAllNodes(const base::Callback<void(std::unique_ptr<base::ListValue>)>&
                       callback) override;

  // SyncEngineHost implementation.
  void OnEngineInitialized(
      syncer::ModelTypeSet initial_types,
      const syncer::WeakHandle<syncer::JsBackend>& js_backend,
      const syncer::WeakHandle<syncer::DataTypeDebugInfoListener>&
          debug_info_listener,
      const std::string& cache_guid,
      const std::string& session_name,
      const std::string& birthday,
      const std::string& bag_of_chips,
      bool success) override;
  void OnSyncCycleCompleted(const syncer::SyncCycleSnapshot& snapshot) override;
  void OnProtocolEvent(const syncer::ProtocolEvent& event) override;
  void OnDirectoryTypeCommitCounterUpdated(
      syncer::ModelType type,
      const syncer::CommitCounters& counters) override;
  void OnDirectoryTypeUpdateCounterUpdated(
      syncer::ModelType type,
      const syncer::UpdateCounters& counters) override;
  void OnDatatypeStatusCounterUpdated(
      syncer::ModelType type,
      const syncer::StatusCounters& counters) override;
  void OnConnectionStatusChange(syncer::ConnectionStatus status) override;
  void OnMigrationNeededForTypes(syncer::ModelTypeSet types) override;
  void OnActionableError(const syncer::SyncProtocolError& error) override;

  // DataTypeManagerObserver implementation.
  void OnConfigureDone(
      const syncer::DataTypeManager::ConfigureResult& result) override;
  void OnConfigureStart() override;

  // TODO(crbug.com/884159): Remove these; they should be queried via
  // SyncUserSettings instead.
  bool IsPassphraseRequired() const;
  syncer::ModelTypeSet GetEncryptedDataTypes() const;

  // IdentityManager::Observer implementation.
  void OnAccountsInCookieUpdated(
      const identity::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
      const GoogleServiceAuthError& error) override;

  // Similar to above but with a callback that will be invoked on completion.
  void OnAccountsInCookieUpdatedWithCallback(
      const std::vector<gaia::ListedAccount>& signed_in_accounts,
      const base::Closure& callback);

  // Returns true if currently signed in account is not present in the list of
  // accounts from cookie jar.
  bool HasCookieJarMismatch(
      const std::vector<gaia::ListedAccount>& cookie_jar_accounts);

  syncer::PassphraseRequiredReason passphrase_required_reason_for_test() const {
    return crypto_.passphrase_required_reason();
  }

  // syncer::UnrecoverableErrorHandler implementation.
  void OnUnrecoverableError(const base::Location& from_here,
                            const std::string& message) override;

  // Returns whether or not the underlying sync engine has made any
  // local changes to items that have not yet been synced with the
  // server.
  void HasUnsyncedItemsForTest(base::OnceCallback<void(bool)> cb) const;

  // Used by MigrationWatcher.  May return null.
  syncer::BackendMigrator* GetBackendMigratorForTest();

  // Used by tests to inspect interaction with OAuth2TokenService.
  bool IsRetryingAccessTokenFetchForTest() const;

  // Used by tests to inspect the OAuth2 access tokens used by PSS.
  std::string GetAccessTokenForTest() const;

  // SyncPrefObserver implementation.
  void OnSyncManagedPrefChange(bool is_sync_managed) override;
  void OnFirstSetupCompletePrefChange(bool is_first_setup_complete) override;
  void OnSyncRequestedPrefChange(bool is_sync_requested) override;
  void OnPreferredDataTypesPrefChange() override;

  // Returns true if the syncer is waiting for new datatypes to be encrypted.
  bool encryption_pending() const;

  // KeyedService implementation.  This must be called exactly
  // once (before this object is destroyed).
  void Shutdown() override;

  // Overrides the NetworkResources used for Sync connections.
  // TODO(treib): Inject this in the ctor instead. As it is, it's possible that
  // the real NetworkResources were already used before the test had a chance
  // to call this.
  void OverrideNetworkResourcesForTest(
      std::unique_ptr<syncer::NetworkResources> network_resources);

  // Virtual for testing.
  virtual bool IsDataTypeControllerRunning(syncer::ModelType type) const;

  // This triggers a Directory::SaveChanges() call on the sync thread.
  // It should be used to persist data to disk when the process might be
  // killed in the near future.
  void FlushDirectory() const;

  bool IsPassphrasePrompted() const;
  void SetPassphrasePrompted(bool prompted);

  void SyncAllowedByPlatformChanged(bool allowed);

  // Sometimes we need to wait for tasks on the sync thread in tests.
  scoped_refptr<base::SingleThreadTaskRunner> GetSyncThreadTaskRunnerForTest()
      const;

  // Some tests rely on injecting calls to the encryption observer.
  syncer::SyncEncryptionHandler::Observer* GetEncryptionObserverForTest();

  syncer::SyncClient* GetSyncClientForTest();

 private:
  // Passed as an argument to StopImpl to control whether or not the sync
  // engine should clear its data directory when it shuts down. See StopImpl
  // for more information.
  enum SyncStopDataFate {
    KEEP_DATA,
    CLEAR_DATA,
  };

  // Shorthand for user_settings_.IsFirstSetupComplete().
  bool IsFirstSetupComplete() const;

  // Virtual for testing.
  virtual syncer::WeakHandle<syncer::JsEventHandler> GetJsEventHandler();

  syncer::SyncEngine::HttpPostProviderFactoryGetter
  MakeHttpPostProviderFactoryGetter();

  syncer::WeakHandle<syncer::UnrecoverableErrorHandler>
  GetUnrecoverableErrorHandler();

  // Callbacks for SyncAuthManager.
  void AccountStateChanged();
  void CredentialsChanged();

  bool IsEngineAllowedToStart() const;

  enum UnrecoverableErrorReason {
    ERROR_REASON_UNSET,
    ERROR_REASON_SYNCER,
    ERROR_REASON_ENGINE_INIT_FAILURE,
    ERROR_REASON_CONFIGURATION_RETRY,
    ERROR_REASON_CONFIGURATION_FAILURE,
    ERROR_REASON_ACTIONABLE_ERROR,
    ERROR_REASON_LIMIT
  };

  friend class TestProfileSyncService;

  // Reconfigures the data type manager with the latest enabled types.
  // Note: Does not initialize the engine if it is not already initialized.
  // If a Sync setup is currently in progress (i.e. a settings UI is open), then
  // the reconfiguration will only happen if |bypass_setup_in_progress_check| is
  // set to true.
  void ReconfigureDatatypeManager(bool bypass_setup_in_progress_check);

  // Helper to install and configure a data type manager.
  void ConfigureDataTypeManager(syncer::ConfigureReason reason);

  // Shuts down the engine sync components.
  // |reason| dictates if syncing is being disabled or not, and whether
  // to claim ownership of sync thread from engine.
  void ShutdownImpl(syncer::ShutdownReason reason);

  // Helper method for managing encryption UI.
  bool IsEncryptedDatatypeEnabled() const;

  // Helper for OnUnrecoverableError.
  void OnUnrecoverableErrorImpl(const base::Location& from_here,
                                const std::string& message,
                                UnrecoverableErrorReason reason);

  // Stops the sync engine. Does NOT set IsSyncRequested to false. Use
  // RequestStop for that. |data_fate| controls whether the local sync data is
  // deleted or kept when the engine shuts down.
  void StopImpl(SyncStopDataFate data_fate);

  // Puts the engine's sync scheduler into NORMAL mode.
  // Called when configuration is complete.
  void StartSyncingWithServer();

  // Sets the last synced time to the current time.
  void UpdateLastSyncedTime();

  // Notify all observers that a change has occurred.
  void NotifyObservers();

  void NotifySyncCycleCompleted();
  void NotifyShutdown();

  void ClearUnrecoverableError();

  // Kicks off asynchronous initialization of the SyncEngine.
  // Virtual for testing.
  virtual void StartUpSlowEngineComponents();

  // Update UMA for syncing engine.
  void UpdateEngineInitUMA(bool success) const;

  // Whether sync has been authenticated with an account ID.
  bool IsSignedIn() const;

  // Tell the sync server that this client has disabled sync.
  void RemoveClientFromServer() const;

  // Called when the system is under memory pressure.
  void OnMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level);

  // Check if previous shutdown is shutdown cleanly.
  void ReportPreviousSessionMemoryWarningCount();

  // Estimates and records memory usage histograms per type.
  void RecordMemoryUsageHistograms();

  // True if setup has been completed at least once and is not in progress.
  bool CanConfigureDataTypes(bool bypass_setup_in_progress_check) const;

  // Called when a SetupInProgressHandle issued by this instance is destroyed.
  virtual void OnSetupInProgressHandleDestroyed();

  // Called by SyncServiceCrypto when a passphrase is required or accepted.
  void ReconfigureDueToPassphrase(syncer::ConfigureReason reason);

  // This profile's SyncClient, which abstracts away non-Sync dependencies and
  // the Sync API component factory.
  const std::unique_ptr<syncer::SyncClient> sync_client_;

  // The class that handles getting, setting, and persisting sync preferences.
  syncer::SyncPrefs sync_prefs_;

  // Encapsulates user signin - used to set/get the user's authenticated
  // email address and sign-out upon error.
  identity::IdentityManager* const identity_manager_;

  std::unique_ptr<syncer::SyncUserSettingsImpl> user_settings_;

  // Handles tracking of the authenticated account and acquiring access tokens.
  // Only null after Shutdown().
  std::unique_ptr<SyncAuthManager> auth_manager_;

  // An identifier representing this instance for debugging purposes.
  const std::string debug_identifier_;

  // This specifies where to find the sync server.
  const GURL sync_service_url_;

  // A utility object containing logic and state relating to encryption.
  syncer::SyncServiceCrypto crypto_;

  // The thread where all the sync operations happen. This thread is kept alive
  // until browser shutdown and reused if sync is turned off and on again. It is
  // joined during the shutdown process, but there is an abort mechanism in
  // place to prevent slow HTTP requests from blocking browser shutdown.
  std::unique_ptr<base::Thread> sync_thread_;

  // Our asynchronous engine to communicate with sync components living on
  // other threads.
  std::unique_ptr<syncer::SyncEngine> engine_;

  // Used to ensure that certain operations are performed on the sequence that
  // this object was created on.
  SEQUENCE_CHECKER(sequence_checker_);

  // Cache of the last SyncCycleSnapshot received from the sync engine.
  syncer::SyncCycleSnapshot last_snapshot_;

  // The time that OnConfigureStart is called. This member is zero if
  // OnConfigureStart has not yet been called, and is reset to zero once
  // OnConfigureDone is called.
  base::Time sync_configure_start_time_;

  // Callback to update the network time; used for initializing the engine.
  syncer::NetworkTimeUpdateCallback network_time_update_callback_;

  // The URL loader factory for the sync.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // The global NetworkConnectionTracker instance.
  network::NetworkConnectionTracker* network_connection_tracker_;

  // Indicates if this is the first time sync is being configured.
  // This is set to true if last synced time is not set at the time of
  // OnEngineInitialized().
  bool is_first_time_sync_configure_;

  // Number of UIs currently configuring the Sync service. When this number
  // is decremented back to zero, Sync setup is marked no longer in progress.
  int outstanding_setup_in_progress_handles_ = 0;

  // Whether the SyncEngine has been initialized.
  bool engine_initialized_;

  // Set when sync receives STOP_SYNC_FOR_DISABLED_ACCOUNT error from server.
  // Prevents ProfileSyncService from starting engine till browser restarted
  // or user signed out.
  bool sync_disabled_by_admin_;

  // Information describing an unrecoverable error.
  UnrecoverableErrorReason unrecoverable_error_reason_;
  std::string unrecoverable_error_message_;
  base::Location unrecoverable_error_location_;

  // Manages the start and stop of the data types.
  std::unique_ptr<syncer::DataTypeManager> data_type_manager_;

  base::ObserverList<syncer::SyncServiceObserver>::Unchecked observers_;
  base::ObserverList<syncer::ProtocolEventObserver>::Unchecked
      protocol_event_observers_;
  base::ObserverList<syncer::TypeDebugInfoObserver>::Unchecked
      type_debug_info_observers_;

  std::set<syncer::SyncTypePreferenceProvider*> preference_providers_;

  syncer::SyncJsController sync_js_controller_;

  // This allows us to gracefully handle an ABORTED return code from the
  // DataTypeManager in the event that the server informed us to cease and
  // desist syncing immediately.
  bool expect_sync_configuration_aborted_;

  std::unique_ptr<syncer::BackendMigrator> migrator_;

  // This is the last |SyncProtocolError| we received from the server that had
  // an action set on it.
  syncer::SyncProtocolError last_actionable_error_;

  // Tracks the set of failed data types (those that encounter an error
  // or must delay loading for some reason).
  syncer::DataTypeStatusTable::TypeErrorMap data_type_error_map_;

  // This providers tells the invalidations code which identity to register for.
  // The account that it registers for should be the same as the currently
  // syncing account, so we'll need to update this whenever the account changes.
  std::vector<invalidation::IdentityProvider*> const
      invalidations_identity_providers_;

  // List of available data type controllers.
  syncer::DataTypeController::TypeMap data_type_controllers_;

  std::unique_ptr<syncer::NetworkResources> network_resources_;

  const StartBehavior start_behavior_;
  std::unique_ptr<syncer::StartupController> startup_controller_;

  std::unique_ptr<syncer::SyncStoppedReporter> sync_stopped_reporter_;

  // Listens for the system being under memory pressure.
  std::unique_ptr<base::MemoryPressureListener> memory_pressure_listener_;

  // Whether the major version has changed since the last time Chrome ran,
  // and therefore a passphrase required state should result in prompting
  // the user. This logic is only enabled on platforms that consume the
  // IsPassphrasePrompted sync preference.
  bool passphrase_prompt_triggered_by_version_;

  // Used by StopAndClear() to remember that clearing of data is needed (as
  // sync is stopped after a callback from |user_settings_|).
  bool is_stopping_and_clearing_;

  // This weak factory invalidates its issued pointers when Sync is disabled.
  base::WeakPtrFactory<ProfileSyncService> sync_enabled_weak_factory_;

  base::WeakPtrFactory<ProfileSyncService> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ProfileSyncService);
};

}  // namespace browser_sync

#endif  // COMPONENTS_BROWSER_SYNC_PROFILE_SYNC_SERVICE_H_
