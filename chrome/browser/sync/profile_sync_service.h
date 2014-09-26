// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_PROFILE_SYNC_SERVICE_H_
#define CHROME_BROWSER_SYNC_PROFILE_SYNC_SERVICE_H_

#include <set>
#include <string>
#include <utility>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/location.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/browsing_data/browsing_data_remover.h"
#include "chrome/browser/sync/backend_unrecoverable_error_handler.h"
#include "chrome/browser/sync/backup_rollback_controller.h"
#include "chrome/browser/sync/glue/sync_backend_host.h"
#include "chrome/browser/sync/profile_sync_service_base.h"
#include "chrome/browser/sync/profile_sync_service_observer.h"
#include "chrome/browser/sync/protocol_event_observer.h"
#include "chrome/browser/sync/sessions/sessions_sync_manager.h"
#include "chrome/browser/sync/startup_controller.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/core/browser/signin_manager_base.h"
#include "components/sync_driver/data_type_controller.h"
#include "components/sync_driver/data_type_encryption_handler.h"
#include "components/sync_driver/data_type_manager.h"
#include "components/sync_driver/data_type_manager_observer.h"
#include "components/sync_driver/data_type_status_table.h"
#include "components/sync_driver/device_info_sync_service.h"
#include "components/sync_driver/local_device_info_provider.h"
#include "components/sync_driver/non_blocking_data_type_manager.h"
#include "components/sync_driver/sync_frontend.h"
#include "components/sync_driver/sync_prefs.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_token_service.h"
#include "net/base/backoff_entry.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/engine/model_safe_worker.h"
#include "sync/internal_api/public/shutdown_reason.h"
#include "sync/internal_api/public/sync_manager_factory.h"
#include "sync/internal_api/public/util/experiments.h"
#include "sync/internal_api/public/util/unrecoverable_error_handler.h"
#include "sync/js/sync_js_controller.h"
#include "url/gurl.h"

class Profile;
class ProfileOAuth2TokenService;
class ProfileSyncComponentsFactory;
class SupervisedUserSigninManagerWrapper;
class SyncErrorController;
class SyncTypePreferenceProvider;

namespace base {
class CommandLine;
};

namespace browser_sync {
class BackendMigrator;
class FaviconCache;
class JsController;
class OpenTabsUIDelegate;

namespace sessions {
class SyncSessionSnapshot;
}  // namespace sessions
}  // namespace browser_sync

namespace sync_driver {
class ChangeProcessor;
class DataTypeManager;
class DeviceInfoSyncService;
class LocalDeviceInfoProvider;
}  // namespace sync_driver

namespace syncer {
class BaseTransaction;
class NetworkResources;
struct CommitCounters;
struct StatusCounters;
struct SyncCredentials;
struct UpdateCounters;
struct UserShare;
}  // namespace syncer

namespace sync_pb {
class EncryptedData;
}  // namespace sync_pb

// ProfileSyncService is the layer between browser subsystems like bookmarks,
// and the sync backend.  Each subsystem is logically thought of as being
// a sync datatype.
//
// Individual datatypes can, at any point, be in a variety of stages of being
// "enabled".  Here are some specific terms for concepts used in this class:
//
//   'Registered' (feature suppression for a datatype)
//
//      When a datatype is registered, the user has the option of syncing it.
//      The sync opt-in UI will show only registered types; a checkbox should
//      never be shown for an unregistered type, and nor should it ever be
//      synced.
//
//      A datatype is considered registered once RegisterDataTypeController
//      has been called with that datatype's DataTypeController.
//
//   'Preferred' (user preferences and opt-out for a datatype)
//
//      This means the user's opt-in or opt-out preference on a per-datatype
//      basis.  The sync service will try to make active exactly these types.
//      If a user has opted out of syncing a particular datatype, it will
//      be registered, but not preferred.
//
//      This state is controlled by the ConfigurePreferredDataTypes and
//      GetPreferredDataTypes.  They are stored in the preferences system,
//      and persist; though if a datatype is not registered, it cannot
//      be a preferred datatype.
//
//   'Active' (run-time initialization of sync system for a datatype)
//
//      An active datatype is a preferred datatype that is actively being
//      synchronized: the syncer has been instructed to querying the server
//      for this datatype, first-time merges have finished, and there is an
//      actively installed ChangeProcessor that listens for changes to this
//      datatype, propagating such changes into and out of the sync backend
//      as necessary.
//
//      When a datatype is in the process of becoming active, it may be
//      in some intermediate state.  Those finer-grained intermediate states
//      are differentiated by the DataTypeController state.
//
// Sync Configuration:
//
//   Sync configuration is accomplished via the following APIs:
//    * OnUserChoseDatatypes(): Set the data types the user wants to sync.
//    * SetDecryptionPassphrase(): Attempt to decrypt the user's encrypted data
//        using the passed passphrase.
//    * SetEncryptionPassphrase(): Re-encrypt the user's data using the passed
//        passphrase.
//
//   Additionally, the current sync configuration can be fetched by calling
//    * GetRegisteredDataTypes()
//    * GetPreferredDataTypes()
//    * GetActiveDataTypes()
//    * IsUsingSecondaryPassphrase()
//    * EncryptEverythingEnabled()
//    * IsPassphraseRequired()/IsPassphraseRequiredForDecryption()
//
//   The "sync everything" state cannot be read from ProfileSyncService, but
//   is instead pulled from SyncPrefs.HasKeepEverythingSynced().
//
// Initial sync setup:
//
//   For privacy reasons, it is usually desirable to avoid syncing any data
//   types until the user has finished setting up sync. There are two APIs
//   that control the initial sync download:
//
//    * SetSyncSetupCompleted()
//    * SetSetupInProgress()
//
//   SetSyncSetupCompleted() should be called once the user has finished setting
//   up sync at least once on their account. SetSetupInProgress(true) should be
//   called while the user is actively configuring their account, and then
//   SetSetupInProgress(false) should be called when configuration is complete.
//   When SetSyncSetupCompleted() == false, but SetSetupInProgress(true) has
//   been called, then the sync engine knows not to download any user data.
//
//   When initial sync is complete, the UI code should call
//   SetSyncSetupCompleted() followed by SetSetupInProgress(false) - this will
//   tell the sync engine that setup is completed and it can begin downloading
//   data from the sync server.
//
class ProfileSyncService : public ProfileSyncServiceBase,
                           public sync_driver::SyncFrontend,
                           public sync_driver::SyncPrefObserver,
                           public sync_driver::DataTypeManagerObserver,
                           public syncer::UnrecoverableErrorHandler,
                           public KeyedService,
                           public sync_driver::DataTypeEncryptionHandler,
                           public OAuth2TokenService::Consumer,
                           public OAuth2TokenService::Observer,
                           public SigninManagerBase::Observer {
 public:
  typedef browser_sync::SyncBackendHost::Status Status;

  // Status of sync server connection, sync token and token request.
  struct SyncTokenStatus {
    SyncTokenStatus();
    ~SyncTokenStatus();

    // Sync server connection status reported by sync backend.
    base::Time connection_status_update_time;
    syncer::ConnectionStatus connection_status;

    // Times when OAuth2 access token is requested and received.
    base::Time token_request_time;
    base::Time token_receive_time;

    // Error returned by OAuth2TokenService for token request and time when
    // next request is scheduled.
    GoogleServiceAuthError last_get_token_error;
    base::Time next_token_request_time;
  };

  enum SyncEventCodes  {
    MIN_SYNC_EVENT_CODE = 0,

    // Events starting the sync service.
    START_FROM_NTP = 1,      // Sync was started from the ad in NTP
    START_FROM_WRENCH = 2,   // Sync was started from the Wrench menu.
    START_FROM_OPTIONS = 3,  // Sync was started from Wrench->Options.
    START_FROM_BOOKMARK_MANAGER = 4,  // Sync was started from Bookmark manager.
    START_FROM_PROFILE_MENU = 5,  // Sync was started from multiprofile menu.
    START_FROM_URL = 6,  // Sync was started from a typed URL.

    // Events regarding cancellation of the signon process of sync.
    CANCEL_FROM_SIGNON_WITHOUT_AUTH = 10,   // Cancelled before submitting
                                            // username and password.
    CANCEL_DURING_SIGNON = 11,              // Cancelled after auth.
    CANCEL_DURING_CONFIGURE = 12,           // Cancelled before choosing data
                                            // types and clicking OK.
    // Events resulting in the stoppage of sync service.
    STOP_FROM_OPTIONS = 20,  // Sync was stopped from Wrench->Options.
    STOP_FROM_ADVANCED_DIALOG = 21,  // Sync was stopped via advanced settings.

    // Miscellaneous events caused by sync service.

    MAX_SYNC_EVENT_CODE
  };

  // Used to specify the kind of passphrase with which sync data is encrypted.
  enum PassphraseType {
    IMPLICIT,  // The user did not provide a custom passphrase for encryption.
               // We implicitly use the GAIA password in such cases.
    EXPLICIT,  // The user selected the "use custom passphrase" radio button
               // during sync setup and provided a passphrase.
  };

  enum SyncStatusSummary {
    UNRECOVERABLE_ERROR,
    NOT_ENABLED,
    SETUP_INCOMPLETE,
    DATATYPES_NOT_INITIALIZED,
    INITIALIZED,
    BACKUP_USER_DATA,
    ROLLBACK_USER_DATA,
    UNKNOWN_ERROR,
  };

  enum BackendMode {
    IDLE,       // No backend.
    SYNC,       // Backend for syncing.
    BACKUP,     // Backend for backup.
    ROLLBACK    // Backend for rollback.
  };

  // Default sync server URL.
  static const char* kSyncServerUrl;
  // Sync server URL for dev channel users
  static const char* kDevServerUrl;

  // Takes ownership of |factory| and |signin_wrapper|.
  ProfileSyncService(
      scoped_ptr<ProfileSyncComponentsFactory> factory,
      Profile* profile,
      scoped_ptr<SupervisedUserSigninManagerWrapper> signin_wrapper,
      ProfileOAuth2TokenService* oauth2_token_service,
      browser_sync::ProfileSyncServiceStartBehavior start_behavior);
  virtual ~ProfileSyncService();

  // Initializes the object. This must be called at most once, and
  // immediately after an object of this class is constructed.
  void Initialize();

  virtual void SetSyncSetupCompleted();

  // ProfileSyncServiceBase implementation.
  virtual bool HasSyncSetupCompleted() const OVERRIDE;
  virtual bool ShouldPushChanges() OVERRIDE;
  virtual syncer::ModelTypeSet GetActiveDataTypes() const OVERRIDE;
  virtual void AddObserver(ProfileSyncServiceBase::Observer* observer) OVERRIDE;
  virtual void RemoveObserver(
      ProfileSyncServiceBase::Observer* observer) OVERRIDE;
  virtual bool HasObserver(
      ProfileSyncServiceBase::Observer* observer) const OVERRIDE;


  void AddProtocolEventObserver(browser_sync::ProtocolEventObserver* observer);
  void RemoveProtocolEventObserver(
      browser_sync::ProtocolEventObserver* observer);

  void AddTypeDebugInfoObserver(syncer::TypeDebugInfoObserver* observer);
  void RemoveTypeDebugInfoObserver(syncer::TypeDebugInfoObserver* observer);

  // Add a sync type preference provider. Each provider may only be added once.
  void AddPreferenceProvider(SyncTypePreferenceProvider* provider);
  // Remove a sync type preference provider. May only be called for providers
  // that have been added. Providers must not remove themselves while being
  // called back.
  void RemovePreferenceProvider(SyncTypePreferenceProvider* provider);
  // Check whether a given sync type preference provider has been added.
  bool HasPreferenceProvider(SyncTypePreferenceProvider* provider) const;

  // Asynchronously fetches base::Value representations of all sync nodes and
  // returns them to the specified callback on this thread.
  //
  // These requests can live a long time and return when you least expect it.
  // For safety, the callback should be bound to some sort of WeakPtr<> or
  // scoped_refptr<>.
  void GetAllNodes(
      const base::Callback<void(scoped_ptr<base::ListValue>)>& callback);

  void RegisterAuthNotifications();
  void UnregisterAuthNotifications();

  // Returns true if sync is enabled/not suppressed and the user is logged in.
  // (being logged in does not mean that tokens are available - tokens may
  // be missing because they have not loaded yet, or because they were deleted
  // due to http://crbug.com/121755).
  // Virtual to enable mocking in tests.
  // TODO(tim): Remove this? Nothing in ProfileSyncService uses it, and outside
  // callers use a seemingly arbitrary / redundant / bug prone combination of
  // this method, IsSyncAccessible, and others.
  virtual bool IsSyncEnabledAndLoggedIn();

  // Return whether OAuth2 refresh token is loaded and available for the backend
  // to start up. Virtual to enable mocking in tests.
  virtual bool IsOAuthRefreshTokenAvailable();

  // Registers a data type controller with the sync service.  This
  // makes the data type controller available for use, it does not
  // enable or activate the synchronization of the data type (see
  // ActivateDataType).  Takes ownership of the pointer.
  void RegisterDataTypeController(
      sync_driver::DataTypeController* data_type_controller);

  // Registers a type whose sync storage will not be managed by the
  // ProfileSyncService.  It declares that this sync type may be activated at
  // some point in the future.  This function call does not enable or activate
  // the syncing of this type
  void RegisterNonBlockingType(syncer::ModelType type);

  // Called by a component that supports non-blocking sync when it is ready to
  // initialize its connection to the sync backend.
  //
  // If policy allows for syncing this type (ie. it is "preferred"), then this
  // should result in a message to enable syncing for this type when the sync
  // backend is available.  If the type is not to be synced, this should result
  // in a message that allows the component to delete its local sync state.
  void InitializeNonBlockingType(
      syncer::ModelType type,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner,
      const base::WeakPtr<syncer::ModelTypeSyncProxyImpl>& proxy);

  // Return the active OpenTabsUIDelegate. If sessions is not enabled or not
  // currently syncing, returns NULL.
  virtual browser_sync::OpenTabsUIDelegate* GetOpenTabsUIDelegate();

  // Returns the SyncedWindowDelegatesGetter from the embedded sessions manager.
  virtual browser_sync::SyncedWindowDelegatesGetter*
  GetSyncedWindowDelegatesGetter() const;

  // Returns the SyncableService for syncer::SESSIONS.
  virtual syncer::SyncableService* GetSessionsSyncableService();

  // Returns the SyncableService for syncer::DEVICE_INFO.
  virtual syncer::SyncableService* GetDeviceInfoSyncableService();

  // Returns DeviceInfo provider for the local device.
  virtual sync_driver::LocalDeviceInfoProvider* GetLocalDeviceInfoProvider();

  // Returns synced devices tracker. If DEVICE_INFO model type isn't yet
  // enabled or syncing, returns NULL.
  virtual sync_driver::DeviceInfoTracker* GetDeviceInfoTracker() const;

  // Fills state_map with a map of current data types that are possible to
  // sync, as well as their states.
  void GetDataTypeControllerStates(
    sync_driver::DataTypeController::StateMap* state_map) const;

  // Disables sync for user. Use ShowLoginDialog to enable.
  virtual void DisableForUser();

  // Disables sync for the user and prevents it from starting on next restart.
  virtual void StopSyncingPermanently();

  // SyncFrontend implementation.
  virtual void OnBackendInitialized(
      const syncer::WeakHandle<syncer::JsBackend>& js_backend,
      const syncer::WeakHandle<syncer::DataTypeDebugInfoListener>&
          debug_info_listener,
      const std::string& cache_guid,
      bool success) OVERRIDE;
  virtual void OnSyncCycleCompleted() OVERRIDE;
  virtual void OnProtocolEvent(const syncer::ProtocolEvent& event) OVERRIDE;
  virtual void OnDirectoryTypeCommitCounterUpdated(
      syncer::ModelType type,
      const syncer::CommitCounters& counters) OVERRIDE;
  virtual void OnDirectoryTypeUpdateCounterUpdated(
      syncer::ModelType type,
      const syncer::UpdateCounters& counters) OVERRIDE;
  virtual void OnDirectoryTypeStatusCounterUpdated(
      syncer::ModelType type,
      const syncer::StatusCounters& counters) OVERRIDE;
  virtual void OnSyncConfigureRetry() OVERRIDE;
  virtual void OnConnectionStatusChange(
      syncer::ConnectionStatus status) OVERRIDE;
  virtual void OnPassphraseRequired(
      syncer::PassphraseRequiredReason reason,
      const sync_pb::EncryptedData& pending_keys) OVERRIDE;
  virtual void OnPassphraseAccepted() OVERRIDE;
  virtual void OnEncryptedTypesChanged(
      syncer::ModelTypeSet encrypted_types,
      bool encrypt_everything) OVERRIDE;
  virtual void OnEncryptionComplete() OVERRIDE;
  virtual void OnMigrationNeededForTypes(
      syncer::ModelTypeSet types) OVERRIDE;
  virtual void OnExperimentsChanged(
      const syncer::Experiments& experiments) OVERRIDE;
  virtual void OnActionableError(
      const syncer::SyncProtocolError& error) OVERRIDE;

  // DataTypeManagerObserver implementation.
  virtual void OnConfigureDone(
      const sync_driver::DataTypeManager::ConfigureResult& result) OVERRIDE;
  virtual void OnConfigureRetry() OVERRIDE;
  virtual void OnConfigureStart() OVERRIDE;

  // DataTypeEncryptionHandler implementation.
  virtual bool IsPassphraseRequired() const OVERRIDE;
  virtual syncer::ModelTypeSet GetEncryptedDataTypes() const OVERRIDE;

  // SigninManagerBase::Observer implementation.
  virtual void GoogleSigninSucceeded(const std::string& account_id,
                                     const std::string& username,
                                     const std::string& password) OVERRIDE;
  virtual void GoogleSignedOut(const std::string& account_id,
                               const std::string& username) OVERRIDE;

  // Called when a user chooses which data types to sync as part of the sync
  // setup wizard.  |sync_everything| represents whether they chose the
  // "keep everything synced" option; if true, |chosen_types| will be ignored
  // and all data types will be synced.  |sync_everything| means "sync all
  // current and future data types."
  virtual void OnUserChoseDatatypes(bool sync_everything,
      syncer::ModelTypeSet chosen_types);

  // Get the sync status code.
  SyncStatusSummary QuerySyncStatusSummary();

  // Get a description of the sync status for displaying in the user interface.
  std::string QuerySyncStatusSummaryString();

  // Initializes a struct of status indicators with data from the backend.
  // Returns false if the backend was not available for querying; in that case
  // the struct will be filled with default data.
  virtual bool QueryDetailedSyncStatus(
      browser_sync::SyncBackendHost::Status* result);

  virtual const GoogleServiceAuthError& GetAuthError() const;

  // Returns true if initial sync setup is in progress (does not return true
  // if the user is customizing sync after already completing setup once).
  // ProfileSyncService uses this to determine if it's OK to start syncing, or
  // if the user is still setting up the initial sync configuration.
  virtual bool FirstSetupInProgress() const;

  // Called by the UI to notify the ProfileSyncService that UI is visible so it
  // will not start syncing. This tells sync whether it's safe to start
  // downloading data types yet (we don't start syncing until after sync setup
  // is complete). The UI calls this as soon as any part of the signin wizard is
  // displayed (even just the login UI).
  // If |setup_in_progress| is false, this also kicks the sync engine to ensure
  // that data download starts.
  virtual void SetSetupInProgress(bool setup_in_progress);

  // Returns true if the SyncBackendHost has told us it's ready to accept
  // changes.
  // [REMARK] - it is safe to call this function only from the ui thread.
  // because the variable is not thread safe and should only be accessed from
  // single thread. If we want multiple threads to access this(and there is
  // currently no need to do so) we need to protect this with a lock.
  // TODO(timsteele): What happens if the bookmark model is loaded, a change
  // takes place, and the backend isn't initialized yet?
  virtual bool sync_initialized() const;

  virtual bool HasUnrecoverableError() const;
  const std::string& unrecoverable_error_message() {
    return unrecoverable_error_message_;
  }
  tracked_objects::Location unrecoverable_error_location() {
    return unrecoverable_error_location_;
  }

  // Returns true if OnPassphraseRequired has been called for decryption and
  // we have an encrypted data type enabled.
  virtual bool IsPassphraseRequiredForDecryption() const;

  syncer::PassphraseRequiredReason passphrase_required_reason() const {
    return passphrase_required_reason_;
  }

  // Returns a user-friendly string form of last synced time (in minutes).
  virtual base::string16 GetLastSyncedTimeString() const;

  // Returns a human readable string describing backend initialization state.
  std::string GetBackendInitializationStateString() const;

  // Returns true if startup is suppressed (i.e. user has stopped syncing via
  // the google dashboard).
  virtual bool IsStartSuppressed() const;

  ProfileSyncComponentsFactory* factory() { return factory_.get(); }

  // The profile we are syncing for.
  Profile* profile() const { return profile_; }

  // Returns a weak pointer to the service's JsController.
  // Overrideable for testing purposes.
  virtual base::WeakPtr<syncer::JsController> GetJsController();

  // Record stats on various events.
  static void SyncEvent(SyncEventCodes code);

  // Returns whether sync is enabled.  Sync can be enabled/disabled both
  // at compile time (e.g., on a per-OS basis) or at run time (e.g.,
  // command-line switches).
  // Profile::IsSyncAccessible() is probably a better signal than this function.
  // This function can be called from any thread, and the implementation doesn't
  // assume it's running on the UI thread.
  static bool IsSyncEnabled();

  // Returns whether sync is managed, i.e. controlled by configuration
  // management. If so, the user is not allowed to configure sync.
  virtual bool IsManaged() const;

  // syncer::UnrecoverableErrorHandler implementation.
  virtual void OnUnrecoverableError(
      const tracked_objects::Location& from_here,
      const std::string& message) OVERRIDE;

  // Called to re-enable a type disabled by DisableDatatype(..). Note, this does
  // not change the preferred state of a datatype, and is not persisted across
  // restarts.
  void ReenableDatatype(syncer::ModelType type);

  // The functions below (until ActivateDataType()) should only be
  // called if sync_initialized() is true.

  // TODO(akalin): This is called mostly by ModelAssociators and
  // tests.  Figure out how to pass the handle to the ModelAssociators
  // directly, figure out how to expose this to tests, and remove this
  // function.
  virtual syncer::UserShare* GetUserShare() const;

  // TODO(akalin): These two functions are used only by
  // ProfileSyncServiceHarness.  Figure out a different way to expose
  // this info to that class, and remove these functions.

  virtual syncer::sessions::SyncSessionSnapshot
      GetLastSessionSnapshot() const;

  // Returns whether or not the underlying sync engine has made any
  // local changes to items that have not yet been synced with the
  // server.
  bool HasUnsyncedItems() const;

  // Used by ProfileSyncServiceHarness.  May return NULL.
  browser_sync::BackendMigrator* GetBackendMigratorForTest();

  // Used by tests to inspect interaction with OAuth2TokenService.
  bool IsRetryingAccessTokenFetchForTest() const;

  // Used by tests to inspect the OAuth2 access tokens used by PSS.
  std::string GetAccessTokenForTest() const;

  // TODO(sync): This is only used in tests.  Can we remove it?
  void GetModelSafeRoutingInfo(syncer::ModelSafeRoutingInfo* out) const;

  // Returns a ListValue indicating the status of all registered types.
  //
  // The format is:
  // [ {"name": <name>, "value": <value>, "status": <status> }, ... ]
  // where <name> is a type's name, <value> is a string providing details for
  // the type's status, and <status> is one of "error", "warning" or "ok"
  // depending on the type's current status.
  //
  // This function is used by about_sync_util.cc to help populate the about:sync
  // page.  It returns a ListValue rather than a DictionaryValue in part to make
  // it easier to iterate over its elements when constructing that page.
  base::Value* GetTypeStatusMap() const;

  // Overridden by tests.
  // TODO(zea): Remove these and have the dtc's call directly into the SBH.
  virtual void DeactivateDataType(syncer::ModelType type);

  // SyncPrefObserver implementation.
  virtual void OnSyncManagedPrefChange(bool is_sync_managed) OVERRIDE;

  // Changes which data types we're going to be syncing to |preferred_types|.
  // If it is running, the DataTypeManager will be instructed to reconfigure
  // the sync backend so that exactly these datatypes are actively synced.  See
  // class comment for more on what it means for a datatype to be Preferred.
  virtual void ChangePreferredDataTypes(
      syncer::ModelTypeSet preferred_types);

  // Returns the set of types which are preferred for enabling. This is a
  // superset of the active types (see GetActiveDataTypes()).
  virtual syncer::ModelTypeSet GetPreferredDataTypes() const;

  // Returns the set of directory types which are preferred for enabling.
  virtual syncer::ModelTypeSet GetPreferredDirectoryDataTypes() const;

  // Returns the set of off-thread types which are preferred for enabling.
  virtual syncer::ModelTypeSet GetPreferredNonBlockingDataTypes() const;

  // Returns the set of types which are enforced programmatically and can not
  // be disabled by the user.
  virtual syncer::ModelTypeSet GetForcedDataTypes() const;

  // Gets the set of all data types that could be allowed (the set that
  // should be advertised to the user).  These will typically only change
  // via a command-line option.  See class comment for more on what it means
  // for a datatype to be Registered.
  virtual syncer::ModelTypeSet GetRegisteredDataTypes() const;

  // Gets the set of directory types which could be allowed.
  virtual syncer::ModelTypeSet GetRegisteredDirectoryDataTypes() const;

  // Gets the set of off-thread types which could be allowed.
  virtual syncer::ModelTypeSet GetRegisteredNonBlockingDataTypes() const;

  // Checks whether the Cryptographer is ready to encrypt and decrypt updates
  // for sensitive data types. Caller must be holding a
  // syncapi::BaseTransaction to ensure thread safety.
  virtual bool IsCryptographerReady(
      const syncer::BaseTransaction* trans) const;

  // Returns true if a secondary (explicit) passphrase is being used. It is not
  // legal to call this method before the backend is initialized.
  virtual bool IsUsingSecondaryPassphrase() const;

  // Returns the actual passphrase type being used for encryption.
  virtual syncer::PassphraseType GetPassphraseType() const;

  // Returns the time the current explicit passphrase (if any), was set.
  // If no secondary passphrase is in use, or no time is available, returns an
  // unset base::Time.
  virtual base::Time GetExplicitPassphraseTime() const;

  // Note about setting passphrases: There are different scenarios under which
  // we might want to apply a passphrase. It could be for first-time encryption,
  // re-encryption, or for decryption by clients that sign in at a later time.
  // In addition, encryption can either be done using a custom passphrase, or by
  // reusing the GAIA password. Depending on what is happening in the system,
  // callers should determine which of the two methods below must be used.

  // Asynchronously sets the passphrase to |passphrase| for encryption. |type|
  // specifies whether the passphrase is a custom passphrase or the GAIA
  // password being reused as a passphrase.
  // TODO(atwilson): Change this so external callers can only set an EXPLICIT
  // passphrase with this API.
  virtual void SetEncryptionPassphrase(const std::string& passphrase,
                                       PassphraseType type);

  // Asynchronously decrypts pending keys using |passphrase|. Returns false
  // immediately if the passphrase could not be used to decrypt a locally cached
  // copy of encrypted keys; returns true otherwise.
  virtual bool SetDecryptionPassphrase(const std::string& passphrase)
      WARN_UNUSED_RESULT;

  // Turns on encryption for all data. Callers must call OnUserChoseDatatypes()
  // after calling this to force the encryption to occur.
  virtual void EnableEncryptEverything();

  // Returns true if we are currently set to encrypt all the sync data. Note:
  // this is based on the cryptographer's settings, so if the user has recently
  // requested encryption to be turned on, this may not be true yet. For that,
  // encryption_pending() must be checked.
  virtual bool EncryptEverythingEnabled() const;

  // Returns true if the syncer is waiting for new datatypes to be encrypted.
  virtual bool encryption_pending() const;

  const GURL& sync_service_url() const { return sync_service_url_; }
  SigninManagerBase* signin() const;

  // Used by tests.
  bool auto_start_enabled() const;
  bool setup_in_progress() const;

  // Stops the sync backend and sets the flag for suppressing sync startup.
  void StopAndSuppress();

  // Resets the flag for suppressing sync startup and starts the sync backend.
  virtual void UnsuppressAndStart();

  // Marks all currently registered types as "acknowledged" so we won't prompt
  // the user about them any more.
  void AcknowledgeSyncedTypes();

  SyncErrorController* sync_error_controller() {
    return sync_error_controller_.get();
  }

  // TODO(sync): This is only used in tests.  Can we remove it?
  const sync_driver::DataTypeStatusTable& data_type_status_table() const;

  sync_driver::DataTypeManager::ConfigureStatus configure_status() {
    return configure_status_;
  }

  // If true, the ProfileSyncService has detected that a new GAIA signin has
  // succeeded, and is waiting for initialization to complete. This is used by
  // the UI to differentiate between a new auth error (encountered as part of
  // the initialization process) and a pre-existing auth error that just hasn't
  // been cleared yet. Virtual for testing purposes.
  virtual bool waiting_for_auth() const;

  // The set of currently enabled sync experiments.
  const syncer::Experiments& current_experiments() const;

  // OAuth2TokenService::Consumer implementation.
  virtual void OnGetTokenSuccess(
      const OAuth2TokenService::Request* request,
      const std::string& access_token,
      const base::Time& expiration_time) OVERRIDE;
  virtual void OnGetTokenFailure(
      const OAuth2TokenService::Request* request,
      const GoogleServiceAuthError& error) OVERRIDE;

  // OAuth2TokenService::Observer implementation.
  virtual void OnRefreshTokenAvailable(const std::string& account_id) OVERRIDE;
  virtual void OnRefreshTokenRevoked(const std::string& account_id) OVERRIDE;
  virtual void OnRefreshTokensLoaded() OVERRIDE;

  // KeyedService implementation.  This must be called exactly
  // once (before this object is destroyed).
  virtual void Shutdown() OVERRIDE;

  // Called when a datatype (SyncableService) has a need for sync to start
  // ASAP, presumably because a local change event has occurred but we're
  // still in deferred start mode, meaning the SyncableService hasn't been
  // told to MergeDataAndStartSyncing yet.
  void OnDataTypeRequestsSyncStartup(syncer::ModelType type);

  // Return sync token status.
  SyncTokenStatus GetSyncTokenStatus() const;

  browser_sync::FaviconCache* GetFaviconCache();

  // Overrides the NetworkResources used for Sync connections.
  // This function takes ownership of |network_resources|.
  void OverrideNetworkResourcesForTest(
      scoped_ptr<syncer::NetworkResources> network_resources);

  virtual bool IsDataTypeControllerRunning(syncer::ModelType type) const;

  BackendMode backend_mode() const {
    return backend_mode_;
  }

  // Helpers for testing rollback.
  void SetBrowsingDataRemoverObserverForTesting(
      BrowsingDataRemover::Observer* observer);
  void SetClearingBrowseringDataForTesting(base::Callback<
      void(BrowsingDataRemover::Observer*, Profile*, base::Time, base::Time)>
                                               c);

  // Return the base URL of the Sync Server.
  static GURL GetSyncServiceURL(const base::CommandLine& command_line);

  base::Time GetDeviceBackupTimeForTesting() const;

 protected:
  // Helper to configure the priority data types.
  void ConfigurePriorityDataTypes();

  // Helper to install and configure a data type manager.
  void ConfigureDataTypeManager();

  // Shuts down the backend sync components.
  // |reason| dictates if syncing is being disabled or not, and whether
  // to claim ownership of sync thread from backend.
  void ShutdownImpl(syncer::ShutdownReason reason);

  // Return SyncCredentials from the OAuth2TokenService.
  syncer::SyncCredentials GetCredentials();

  virtual syncer::WeakHandle<syncer::JsEventHandler> GetJsEventHandler();

  const sync_driver::DataTypeController::TypeMap&
      directory_data_type_controllers() {
    return directory_data_type_controllers_;
  }

  // Helper method for managing encryption UI.
  bool IsEncryptedDatatypeEnabled() const;

  // Helper for OnUnrecoverableError.
  // TODO(tim): Use an enum for |delete_sync_database| here, in ShutdownImpl,
  // and in SyncBackendHost::Shutdown.
  void OnUnrecoverableErrorImpl(
      const tracked_objects::Location& from_here,
      const std::string& message,
      bool delete_sync_database);

  virtual bool NeedBackup() const;

  // This is a cache of the last authentication response we received from the
  // sync server. The UI queries this to display appropriate messaging to the
  // user.
  GoogleServiceAuthError last_auth_error_;

  // Our asynchronous backend to communicate with sync components living on
  // other threads.
  scoped_ptr<browser_sync::SyncBackendHost> backend_;

  // Was the last SYNC_PASSPHRASE_REQUIRED notification sent because it
  // was required for encryption, decryption with a cached passphrase, or
  // because a new passphrase is required?
  syncer::PassphraseRequiredReason passphrase_required_reason_;

 private:
  enum UnrecoverableErrorReason {
    ERROR_REASON_UNSET,
    ERROR_REASON_SYNCER,
    ERROR_REASON_BACKEND_INIT_FAILURE,
    ERROR_REASON_CONFIGURATION_RETRY,
    ERROR_REASON_CONFIGURATION_FAILURE,
    ERROR_REASON_ACTIONABLE_ERROR,
    ERROR_REASON_LIMIT
  };

  enum AuthErrorMetric {
    AUTH_ERROR_ENCOUNTERED,
    AUTH_ERROR_FIXED,
    AUTH_ERROR_LIMIT
  };

  friend class ProfileSyncServicePasswordTest;
  friend class SyncTest;
  friend class TestProfileSyncService;
  FRIEND_TEST_ALL_PREFIXES(ProfileSyncServiceTest, InitialState);

  // Update the last auth error and notify observers of error state.
  void UpdateAuthErrorState(const GoogleServiceAuthError& error);

  // Detects and attempts to recover from a previous improper datatype
  // configuration where Keep Everything Synced and the preferred types were
  // not correctly set.
  void TrySyncDatatypePrefRecovery();

  // Puts the backend's sync scheduler into NORMAL mode.
  // Called when configuration is complete.
  void StartSyncingWithServer();

  // Called when we've determined that we don't need a passphrase (either
  // because OnPassphraseAccepted() was called, or because we've gotten a
  // OnPassphraseRequired() but no data types are enabled).
  void ResolvePassphraseRequired();

  // During initial signin, ProfileSyncService caches the user's signin
  // passphrase so it can be used to encrypt/decrypt data after sync starts up.
  // This routine is invoked once the backend has started up to use the
  // cached passphrase and clear it out when it is done.
  void ConsumeCachedPassphraseIfPossible();

  // RequestAccessToken initiates RPC to request downscoped access token from
  // refresh token. This happens when a new OAuth2 login token is loaded and
  // when sync server returns AUTH_ERROR which indicates it is time to refresh
  // token.
  virtual void RequestAccessToken();

  // Return true if backend should start from a fresh sync DB.
  bool ShouldDeleteSyncFolder();

  // If |delete_sync_data_folder| is true, then this method will delete all
  // previous "Sync Data" folders. (useful if the folder is partial/corrupt).
  void InitializeBackend(bool delete_sync_data_folder);

  // Initializes the various settings from the command line.
  void InitSettings();

  // Sets the last synced time to the current time.
  void UpdateLastSyncedTime();

  void NotifyObservers();
  void NotifySyncCycleCompleted();

  void ClearStaleErrors();

  void ClearUnrecoverableError();

  // Starts up the backend sync components. |mode| specifies the kind of
  // backend to start, one of SYNC, BACKUP or ROLLBACK.
  virtual void StartUpSlowBackendComponents(BackendMode mode);

  // About-flags experiment names for datatypes that aren't enabled by default
  // yet.
  static std::string GetExperimentNameForDataType(
      syncer::ModelType data_type);

  // Create and register a new datatype controller.
  void RegisterNewDataType(syncer::ModelType data_type);

  // Reconfigures the data type manager with the latest enabled types.
  // Note: Does not initialize the backend if it is not already initialized.
  // This function needs to be called only after sync has been initialized
  // (i.e.,only for reconfigurations). The reason we don't initialize the
  // backend is because if we had encountered an unrecoverable error we don't
  // want to startup once more.
  virtual void ReconfigureDatatypeManager();

  // Collects preferred sync data types from |preference_providers_|.
  syncer::ModelTypeSet GetDataTypesFromPreferenceProviders() const;

  // Called when the user changes the sync configuration, to update the UMA
  // stats.
  void UpdateSelectedTypesHistogram(
      bool sync_everything,
      const syncer::ModelTypeSet chosen_types) const;

#if defined(OS_CHROMEOS)
  // Refresh spare sync bootstrap token for re-enabling the sync service.
  // Called on successful sign-in notifications.
  void RefreshSpareBootstrapToken(const std::string& passphrase);
#endif

  // Internal unrecoverable error handler. Used to track error reason via
  // Sync.UnrecoverableErrors histogram.
  void OnInternalUnrecoverableError(const tracked_objects::Location& from_here,
                                    const std::string& message,
                                    bool delete_sync_database,
                                    UnrecoverableErrorReason reason);

  // Returns the type of manager to use according to |backend_mode_|.
  syncer::SyncManagerFactory::MANAGER_TYPE GetManagerType() const;

  // Update UMA for syncing backend.
  void UpdateBackendInitUMA(bool success);

  // Various setup following backend initialization, mostly for syncing backend.
  void PostBackendInitialization();

  // True if a syncing backend exists.
  bool HasSyncingBackend() const;

  // Update first sync time stored in preferences
  void UpdateFirstSyncTimePref();

  // Clear browsing data since first sync during rollback.
  void ClearBrowsingDataSinceFirstSync();

  // Post background task to check sync backup DB state if needed.
  void CheckSyncBackupIfNeeded();

  // Callback to receive backup DB check result.
  void CheckSyncBackupCallback(base::Time backup_time);

  // Callback function to call |startup_controller_|.TryStart() after
  // backup/rollback finishes;
  void TryStartSyncAfterBackup();

  // Clean up prefs and backup DB when rollback is not needed.
  void CleanUpBackup();

 // Factory used to create various dependent objects.
  scoped_ptr<ProfileSyncComponentsFactory> factory_;

  // The profile whose data we are synchronizing.
  Profile* profile_;

  // The class that handles getting, setting, and persisting sync
  // preferences.
  sync_driver::SyncPrefs sync_prefs_;

  // TODO(ncarter): Put this in a profile, once there is UI for it.
  // This specifies where to find the sync server.
  const GURL sync_service_url_;

  // The last time we detected a successful transition from SYNCING state.
  // Our backend notifies us whenever we should take a new snapshot.
  base::Time last_synced_time_;

  // The time that OnConfigureStart is called. This member is zero if
  // OnConfigureStart has not yet been called, and is reset to zero once
  // OnConfigureDone is called.
  base::Time sync_configure_start_time_;

  // Indicates if this is the first time sync is being configured.  This value
  // is equal to !HasSyncSetupCompleted() at the time of OnBackendInitialized().
  bool is_first_time_sync_configure_;

  // List of available data type controllers for directory types.
  sync_driver::DataTypeController::TypeMap directory_data_type_controllers_;

  // Whether the SyncBackendHost has been initialized.
  bool backend_initialized_;

  // Set when sync receives DISABLED_BY_ADMIN error from server. Prevents
  // ProfileSyncService from starting backend till browser restarted or user
  // signed out.
  bool sync_disabled_by_admin_;

  // Set to true if a signin has completed but we're still waiting for the
  // backend to refresh its credentials.
  bool is_auth_in_progress_;

  // Encapsulates user signin - used to set/get the user's authenticated
  // email address.
  const scoped_ptr<SupervisedUserSigninManagerWrapper> signin_;

  // Information describing an unrecoverable error.
  UnrecoverableErrorReason unrecoverable_error_reason_;
  std::string unrecoverable_error_message_;
  tracked_objects::Location unrecoverable_error_location_;

  // Manages the start and stop of the directory data types.
  scoped_ptr<sync_driver::DataTypeManager> directory_data_type_manager_;

  // Manager for the non-blocking data types.
  sync_driver::NonBlockingDataTypeManager non_blocking_data_type_manager_;

  ObserverList<ProfileSyncServiceBase::Observer> observers_;
  ObserverList<browser_sync::ProtocolEventObserver> protocol_event_observers_;
  ObserverList<syncer::TypeDebugInfoObserver> type_debug_info_observers_;

  std::set<SyncTypePreferenceProvider*> preference_providers_;

  syncer::SyncJsController sync_js_controller_;

  // This allows us to gracefully handle an ABORTED return code from the
  // DataTypeManager in the event that the server informed us to cease and
  // desist syncing immediately.
  bool expect_sync_configuration_aborted_;

  // Sometimes we need to temporarily hold on to a passphrase because we don't
  // yet have a backend to send it to.  This happens during initialization as
  // we don't StartUp until we have a valid token, which happens after valid
  // credentials were provided.
  std::string cached_passphrase_;

  // The current set of encrypted types.  Always a superset of
  // syncer::Cryptographer::SensitiveTypes().
  syncer::ModelTypeSet encrypted_types_;

  // Whether we want to encrypt everything.
  bool encrypt_everything_;

  // Whether we're waiting for an attempt to encryption all sync data to
  // complete. We track this at this layer in order to allow the user to cancel
  // if they e.g. don't remember their explicit passphrase.
  bool encryption_pending_;

  scoped_ptr<browser_sync::BackendMigrator> migrator_;

  // This is the last |SyncProtocolError| we received from the server that had
  // an action set on it.
  syncer::SyncProtocolError last_actionable_error_;

  // Exposes sync errors to the UI.
  scoped_ptr<SyncErrorController> sync_error_controller_;

  // Tracks the set of failed data types (those that encounter an error
  // or must delay loading for some reason).
  sync_driver::DataTypeStatusTable data_type_status_table_;

  sync_driver::DataTypeManager::ConfigureStatus configure_status_;

  // The set of currently enabled sync experiments.
  syncer::Experiments current_experiments_;

  // Sync's internal debug info listener. Used to record datatype configuration
  // and association information.
  syncer::WeakHandle<syncer::DataTypeDebugInfoListener> debug_info_listener_;

  // A thread where all the sync operations happen.
  // OWNERSHIP Notes:
  //     * Created when backend starts for the first time.
  //     * If sync is disabled, PSS claims ownership from backend.
  //     * If sync is reenabled, PSS passes ownership to new backend.
  scoped_ptr<base::Thread> sync_thread_;

  // ProfileSyncService uses this service to get access tokens.
  ProfileOAuth2TokenService* const oauth2_token_service_;

  // ProfileSyncService needs to remember access token in order to invalidate it
  // with OAuth2TokenService.
  std::string access_token_;

  // ProfileSyncService needs to hold reference to access_token_request_ for
  // the duration of request in order to receive callbacks.
  scoped_ptr<OAuth2TokenService::Request> access_token_request_;

  // If RequestAccessToken fails with transient error then retry requesting
  // access token with exponential backoff.
  base::OneShotTimer<ProfileSyncService> request_access_token_retry_timer_;
  net::BackoffEntry request_access_token_backoff_;

  base::WeakPtrFactory<ProfileSyncService> weak_factory_;

  // We don't use |weak_factory_| for the StartupController because the weak
  // ptrs should be bound to the lifetime of ProfileSyncService and not to the
  // [Initialize -> sync disabled/shutdown] lifetime.  We don't pass
  // StartupController an Unretained reference to future-proof against
  // the controller impl changing to post tasks. Therefore, we have a separate
  // factory.
  base::WeakPtrFactory<ProfileSyncService> startup_controller_weak_factory_;

  // States related to sync token and connection.
  base::Time connection_status_update_time_;
  syncer::ConnectionStatus connection_status_;
  base::Time token_request_time_;
  base::Time token_receive_time_;
  GoogleServiceAuthError last_get_token_error_;
  base::Time next_token_request_time_;

  scoped_ptr<sync_driver::LocalDeviceInfoProvider> local_device_;

  // Locally owned SyncableService implementations.
  scoped_ptr<browser_sync::SessionsSyncManager> sessions_sync_manager_;
  scoped_ptr<sync_driver::DeviceInfoSyncService> device_info_sync_service_;

  scoped_ptr<syncer::NetworkResources> network_resources_;

  browser_sync::StartupController startup_controller_;

  browser_sync::BackupRollbackController backup_rollback_controller_;

  // Mode of current backend.
  BackendMode backend_mode_;

  // Whether backup is needed before sync starts.
  bool need_backup_;

  // Whether backup is finished.
  bool backup_finished_;

  base::Time backup_start_time_;

  base::Callback<
      void(BrowsingDataRemover::Observer*, Profile*, base::Time, base::Time)>
      clear_browsing_data_;

  // Last time when pre-sync data was saved. NULL pointer means backup data
  // state is unknown. If time value is null, backup data doesn't exist.
  scoped_ptr<base::Time> last_backup_time_;

  BrowsingDataRemover::Observer* browsing_data_remover_observer_;

  DISALLOW_COPY_AND_ASSIGN(ProfileSyncService);
};

bool ShouldShowActionOnUI(
    const syncer::SyncProtocolError& error);


#endif  // CHROME_BROWSER_SYNC_PROFILE_SYNC_SERVICE_H_
