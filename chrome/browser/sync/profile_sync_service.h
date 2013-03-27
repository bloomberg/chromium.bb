// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_PROFILE_SYNC_SERVICE_H_
#define CHROME_BROWSER_SYNC_PROFILE_SYNC_SERVICE_H_

#include <list>
#include <string>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/location.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/string16.h"
#include "base/time.h"
#include "base/timer.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "chrome/browser/signin/signin_global_error.h"
#include "chrome/browser/sync/backend_unrecoverable_error_handler.h"
#include "chrome/browser/sync/failed_datatypes_handler.h"
#include "chrome/browser/sync/glue/data_type_controller.h"
#include "chrome/browser/sync/glue/data_type_manager.h"
#include "chrome/browser/sync/glue/data_type_manager_observer.h"
#include "chrome/browser/sync/glue/sync_backend_host.h"
#include "chrome/browser/sync/invalidation_frontend.h"
#include "chrome/browser/sync/invalidations/invalidator_storage.h"
#include "chrome/browser/sync/profile_sync_service_base.h"
#include "chrome/browser/sync/profile_sync_service_observer.h"
#include "chrome/browser/sync/sync_prefs.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_types.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "googleurl/src/gurl.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/engine/model_safe_worker.h"
#include "sync/internal_api/public/sync_manager_factory.h"
#include "sync/internal_api/public/util/experiments.h"
#include "sync/internal_api/public/util/unrecoverable_error_handler.h"
#include "sync/js/sync_js_controller.h"

class Profile;
class ProfileSyncComponentsFactory;
class SigninManager;
class SyncGlobalError;

namespace browser_sync {
class BackendMigrator;
class ChangeProcessor;
class DeviceInfo;
class DataTypeManager;
class JsController;
class SessionModelAssociator;

namespace sessions { class SyncSessionSnapshot; }
}

namespace syncer {
class BaseTransaction;
class InvalidatorRegistrar;
struct SyncCredentials;
struct UserShare;
}

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
                           public browser_sync::SyncFrontend,
                           public browser_sync::SyncPrefObserver,
                           public browser_sync::DataTypeManagerObserver,
                           public SigninGlobalError::AuthStatusProvider,
                           public syncer::UnrecoverableErrorHandler,
                           public content::NotificationObserver,
                           public ProfileKeyedService,
                           public InvalidationFrontend {
 public:
  typedef browser_sync::SyncBackendHost::Status Status;

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

    // Miscellaneous events caused by sync service.

    MAX_SYNC_EVENT_CODE
  };

  // Defines the type of behavior the sync engine should use. If configured for
  // AUTO_START, the sync engine will automatically call SetSyncSetupCompleted()
  // and start downloading data types as soon as sync credentials are available
  // (a signed-in username and a "chromiumsync" token).
  // If configured for MANUAL_START, sync will not start until the user
  // completes sync setup, at which point the UI makes an explicit call to
  // SetSyncSetupCompleted().
  enum StartBehavior {
    AUTO_START,
    MANUAL_START,
  };

  // Used to specify the kind of passphrase with which sync data is encrypted.
  enum PassphraseType {
    IMPLICIT,  // The user did not provide a custom passphrase for encryption.
               // We implicitly use the GAIA password in such cases.
    EXPLICIT,  // The user selected the "use custom passphrase" radio button
               // during sync setup and provided a passphrase.
  };

  // Default sync server URL.
  static const char* kSyncServerUrl;
  // Sync server URL for dev channel users
  static const char* kDevServerUrl;

  // Takes ownership of |factory|.
  ProfileSyncService(ProfileSyncComponentsFactory* factory,
                     Profile* profile,
                     SigninManager* signin,
                     StartBehavior start_behavior);
  virtual ~ProfileSyncService();

  // Initializes the object. This must be called at most once, and
  // immediately after an object of this class is constructed.
  void Initialize();

  virtual void SetSyncSetupCompleted();

  // ProfileSyncServiceBase implementation.
  virtual bool HasSyncSetupCompleted() const OVERRIDE;
  virtual bool ShouldPushChanges() OVERRIDE;
  virtual syncer::ModelTypeSet GetPreferredDataTypes() const OVERRIDE;
  virtual void AddObserver(Observer* observer) OVERRIDE;
  virtual void RemoveObserver(Observer* observer) OVERRIDE;
  virtual bool HasObserver(Observer* observer) const OVERRIDE;

  void RegisterAuthNotifications();

  // Returns true if sync is enabled/not suppressed and the user is logged in.
  // (being logged in does not mean that tokens are available - tokens may
  // be missing because they have not loaded yet, or because they were deleted
  // due to http://crbug.com/121755).
  // Virtual to enable mocking in tests.
  virtual bool IsSyncEnabledAndLoggedIn();

  // Return whether all sync tokens are loaded and available for the backend to
  // start up. Virtual to enable mocking in tests.
  virtual bool IsSyncTokenAvailable();

  // Registers a data type controller with the sync service.  This
  // makes the data type controller available for use, it does not
  // enable or activate the synchronization of the data type (see
  // ActivateDataType).  Takes ownership of the pointer.
  void RegisterDataTypeController(
      browser_sync::DataTypeController* data_type_controller);

  // Returns the session model associator associated with this type, but only if
  // the associator is running.  If it is doing anything else, it will return
  // null.
  // TODO(zea): Figure out a better way to expose this to the UI elements that
  // need it.
  browser_sync::SessionModelAssociator* GetSessionModelAssociator();

  // Returns sync's representation of the local device info.
  // Return value is an empty scoped_ptr if the device info is unavailable.
  virtual scoped_ptr<browser_sync::DeviceInfo> GetLocalDeviceInfo() const;

  // Fills state_map with a map of current data types that are possible to
  // sync, as well as their states.
  void GetDataTypeControllerStates(
    browser_sync::DataTypeController::StateMap* state_map) const;

  // Disables sync for user. Use ShowLoginDialog to enable.
  virtual void DisableForUser();

  // syncer::InvalidationHandler implementation (via SyncFrontend).
  virtual void OnInvalidatorStateChange(
      syncer::InvalidatorState state) OVERRIDE;
  virtual void OnIncomingInvalidation(
      const syncer::ObjectIdInvalidationMap& invalidation_map) OVERRIDE;

  // SyncFrontend implementation.
  virtual void OnBackendInitialized(
      const syncer::WeakHandle<syncer::JsBackend>& js_backend,
      const syncer::WeakHandle<syncer::DataTypeDebugInfoListener>&
          debug_info_listener,
      bool success) OVERRIDE;
  virtual void OnSyncCycleCompleted() OVERRIDE;
  virtual void OnSyncConfigureRetry() OVERRIDE;
  virtual void OnConnectionStatusChange(
      syncer::ConnectionStatus status) OVERRIDE;
  virtual void OnStopSyncingPermanently() OVERRIDE;
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
  virtual void OnConfigureBlocked() OVERRIDE;
  virtual void OnConfigureDone(
      const browser_sync::DataTypeManager::ConfigureResult& result) OVERRIDE;
  virtual void OnConfigureRetry() OVERRIDE;
  virtual void OnConfigureStart() OVERRIDE;

  // Update the last auth error and notify observers of error state.
  void UpdateAuthErrorState(const GoogleServiceAuthError& error);

  // Called when a user chooses which data types to sync as part of the sync
  // setup wizard.  |sync_everything| represents whether they chose the
  // "keep everything synced" option; if true, |chosen_types| will be ignored
  // and all data types will be synced.  |sync_everything| means "sync all
  // current and future data types."
  virtual void OnUserChoseDatatypes(bool sync_everything,
      syncer::ModelTypeSet chosen_types);

  // Get various information for displaying in the user interface.
  std::string QuerySyncStatusSummary();

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

  // Returns true if OnPassphraseRequired has been called for any reason.
  virtual bool IsPassphraseRequired() const;

  // Returns true if OnPassphraseRequired has been called for decryption and
  // we have an encrypted data type enabled.
  virtual bool IsPassphraseRequiredForDecryption() const;

  syncer::PassphraseRequiredReason passphrase_required_reason() const {
    return passphrase_required_reason_;
  }

  // Returns a user-friendly string form of last synced time (in minutes).
  virtual string16 GetLastSyncedTimeString() const;

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

  // SigninGlobalError::AuthStatusProvider implementation.
  virtual GoogleServiceAuthError GetAuthStatus() const OVERRIDE;

  // syncer::UnrecoverableErrorHandler implementation.
  virtual void OnUnrecoverableError(
      const tracked_objects::Location& from_here,
      const std::string& message) OVERRIDE;

  // Called when a datatype wishes to disable itself due to having hit an
  // unrecoverable error.
  virtual void DisableBrokenDatatype(
      syncer::ModelType type,
      const tracked_objects::Location& from_here,
      std::string message);

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

  // TODO(sync): This is only used in tests.  Can we remove it?
  void GetModelSafeRoutingInfo(syncer::ModelSafeRoutingInfo* out) const;

  // Returns a ListValue indicating the status of all registered types.
  //
  // The format is:
  // [ {"name": <name>, "value": <value>, "status": <status> }, ... ]
  // where <name> is a type's name, <value> is a string providing details for
  // the type's status, and <status> is one of "error", "warning" or "ok"
  // dpending on the type's current status.
  //
  // This function is used by sync_ui_util.cc to help populate the about:sync
  // page.  It returns a ListValue rather than a DictionaryValue in part to make
  // it easier to iterate over its elements when constructing that page.
  Value* GetTypeStatusMap() const;

  // Overridden by tests.
  // TODO(zea): Remove these and have the dtc's call directly into the SBH.
  virtual void ActivateDataType(
      syncer::ModelType type, syncer::ModelSafeGroup group,
      browser_sync::ChangeProcessor* change_processor);
  virtual void DeactivateDataType(syncer::ModelType type);

  // SyncPrefObserver implementation.
  virtual void OnSyncManagedPrefChange(bool is_sync_managed) OVERRIDE;

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Changes which data types we're going to be syncing to |preferred_types|.
  // If it is running, the DataTypeManager will be instructed to reconfigure
  // the sync backend so that exactly these datatypes are actively synced.  See
  // class comment for more on what it means for a datatype to be Preferred.
  virtual void ChangePreferredDataTypes(
      syncer::ModelTypeSet preferred_types);

  // Gets the set of all data types that could be allowed (the set that
  // should be advertised to the user).  These will typically only change
  // via a command-line option.  See class comment for more on what it means
  // for a datatype to be Registered.
  virtual syncer::ModelTypeSet GetRegisteredDataTypes() const;

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

  // Fills |encrypted_types| with the set of currently encrypted types. Does
  // not account for types pending encryption.
  virtual syncer::ModelTypeSet GetEncryptedDataTypes() const;

#if defined(OS_ANDROID)
  // Android does not display password prompts, passwords are only allowed to be
  // synced if Cryptographer has already been initialized and does not have
  // pending keys.
  bool ShouldEnablePasswordSyncForAndroid() const;
#endif

  // Returns true if the syncer is waiting for new datatypes to be encrypted.
  virtual bool encryption_pending() const;

  const GURL& sync_service_url() const { return sync_service_url_; }
  bool auto_start_enabled() const { return auto_start_enabled_; }
  SigninManager* signin() const { return signin_; }
  bool setup_in_progress() const { return setup_in_progress_; }

  // Stops the sync backend and sets the flag for suppressing sync startup.
  void StopAndSuppress();

  // Resets the flag for suppressing sync startup and starts the sync backend.
  void UnsuppressAndStart();

  // Marks all currently registered types as "acknowledged" so we won't prompt
  // the user about them any more.
  void AcknowledgeSyncedTypes();

  SyncGlobalError* sync_global_error() { return sync_global_error_.get(); }

  // TODO(sync): This is only used in tests.  Can we remove it?
  const FailedDatatypesHandler& failed_datatypes_handler() const;

  browser_sync::DataTypeManager::ConfigureStatus configure_status() {
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

  // InvalidationFrontend implementation.  It is an error to have
  // registered handlers when Shutdown() is called.
  virtual void RegisterInvalidationHandler(
      syncer::InvalidationHandler* handler) OVERRIDE;
  virtual void UpdateRegisteredInvalidationIds(
      syncer::InvalidationHandler* handler,
      const syncer::ObjectIdSet& ids) OVERRIDE;
  virtual void UnregisterInvalidationHandler(
      syncer::InvalidationHandler* handler) OVERRIDE;
  virtual void AcknowledgeInvalidation(
      const invalidation::ObjectId& id,
      const syncer::AckHandle& ack_handle) OVERRIDE;

  virtual syncer::InvalidatorState GetInvalidatorState() const OVERRIDE;

  // ProfileKeyedService implementation.  This must be called exactly
  // once (before this object is destroyed).
  virtual void Shutdown() OVERRIDE;

  // Simulate an incoming notification for the given id and payload.
  void EmitInvalidationForTest(
      const invalidation::ObjectId& id,
      const std::string& payload);

 protected:
  // Used by test classes that derive from ProfileSyncService.
  virtual browser_sync::SyncBackendHost* GetBackendForTest();

  // Helper to configure the priority data types.
  void ConfigurePriorityDataTypes();

  // Helper to install and configure a data type manager.
  void ConfigureDataTypeManager();

  // Shuts down the backend sync components.
  // |sync_disabled| indicates if syncing is being disabled or not.
  void ShutdownImpl(bool sync_disabled);

  // Return SyncCredentials from the TokenService.
  syncer::SyncCredentials GetCredentials();

  // Test need to override this to create backends that allow setting up
  // initial conditions, such as populating sync nodes.
  //
  // TODO(akalin): Figure out a better way to do this.  Ideally, we'd
  // construct the backend outside this class and pass it in to the
  // contructor or Initialize().
  virtual void CreateBackend();

  const browser_sync::DataTypeController::TypeMap& data_type_controllers() {
    return data_type_controllers_;
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
  typedef std::vector<std::pair<invalidation::ObjectId,
                                syncer::AckHandle> > AckHandleReplayQueue;
  friend class ProfileSyncServicePasswordTest;
  friend class SyncTest;
  friend class TestProfileSyncService;
  FRIEND_TEST_ALL_PREFIXES(ProfileSyncServiceTest, InitialState);

  // Detects and attempts to recover from a previous improper datatype
  // configuration where Keep Everything Synced and the preferred types were
  // not correctly set.
  void TrySyncDatatypePrefRecovery();

  // Starts up sync if it is not suppressed and preconditions are met.
  // Called from Initialize() and UnsuppressAndStart().
  void TryStart();

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

  // If |delete_sync_data_folder| is true, then this method will delete all
  // previous "Sync Data" folders. (useful if the folder is partial/corrupt).
  void InitializeBackend(bool delete_sync_data_folder);

  // Initializes the various settings from the command line.
  void InitSettings();

  // Sets the last synced time to the current time.
  void UpdateLastSyncedTime();

  void NotifyObservers();

  void ClearStaleErrors();

  void ClearUnrecoverableError();

  enum StartUpDeferredOption {
    STARTUP_BACKEND_DEFERRED,
    STARTUP_IMMEDIATE
  };
  void StartUp(StartUpDeferredOption deferred_option);

  // Starts up the backend sync components. |deferred_option| specifies whether
  // this is being called as part of an immediate startup or startup was
  // originally deferred and we're finally getting around to finishing.
  void StartUpSlowBackendComponents(StartUpDeferredOption deferred_option);

  // About-flags experiment names for datatypes that aren't enabled by default
  // yet.
  static std::string GetExperimentNameForDataType(
      syncer::ModelType data_type);

  // Create and register a new datatype controller.
  void RegisterNewDataType(syncer::ModelType data_type);

  // Helper method to process SyncConfigureDone after unwinding the stack that
  // originally posted this SyncConfigureDone.
  void OnSyncConfigureDone(
      browser_sync::DataTypeManager::ConfigureResult result);

  // Reconfigures the data type manager with the latest enabled types.
  // Note: Does not initialize the backend if it is not already initialized.
  // This function needs to be called only after sync has been initialized
  // (i.e.,only for reconfigurations). The reason we don't initialize the
  // backend is because if we had encountered an unrecoverable error we don't
  // want to startup once more.
  virtual void ReconfigureDatatypeManager();

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

  // Must be called every time |backend_initialized_| or
  // |invalidator_state_| is changed (but only if
  // |invalidator_registrar_| is not NULL).
  void UpdateInvalidatorRegistrarState();

  // Destroys / recreates an instance of ProfileSyncService. Used exclusively by
  // the sync integration tests so they can restart sync from scratch without
  // tearing down and recreating the browser process. Needed because simply
  // calling Shutdown() and Initialize() will not recreate other internal
  // objects like SyncBackendHost, SyncManager, etc.
  void ResetForTest();

  // Factory used to create various dependent objects.
  scoped_ptr<ProfileSyncComponentsFactory> factory_;

  // The profile whose data we are synchronizing.
  Profile* profile_;

  // The class that handles getting, setting, and persisting sync
  // preferences.
  browser_sync::SyncPrefs sync_prefs_;

  // TODO(tim): Move this to InvalidationService, once it exists. Bug 124137.
  browser_sync::InvalidatorStorage invalidator_storage_;

  // TODO(ncarter): Put this in a profile, once there is UI for it.
  // This specifies where to find the sync server.
  GURL sync_service_url_;

  // The last time we detected a successful transition from SYNCING state.
  // Our backend notifies us whenever we should take a new snapshot.
  base::Time last_synced_time_;

  // The time that StartUp() is called.  This member is zero if StartUp() has
  // never been called, and is reset to zero once OnBackendInitialized() is
  // called.
  base::Time start_up_time_;

  // The time that OnConfigureStart is called. This member is zero if
  // OnConfigureStart has not yet been called, and is reset to zero once
  // OnConfigureDone is called.
  base::Time sync_configure_start_time_;

  // Indicates if this is the first time sync is being configured.  This value
  // is equal to !HasSyncSetupCompleted() at the time of OnBackendInitialized().
  bool is_first_time_sync_configure_;

  // List of available data type controllers.
  browser_sync::DataTypeController::TypeMap data_type_controllers_;

  // Whether the SyncBackendHost has been initialized.
  bool backend_initialized_;

  // Set to true if a signin has completed but we're still waiting for the
  // backend to refresh its credentials.
  bool is_auth_in_progress_;

  // Encapsulates user signin - used to set/get the user's authenticated
  // email address.
  SigninManager* signin_;

  // Information describing an unrecoverable error.
  UnrecoverableErrorReason unrecoverable_error_reason_;
  std::string unrecoverable_error_message_;
  tracked_objects::Location unrecoverable_error_location_;

  // Manages the start and stop of the various data types.
  scoped_ptr<browser_sync::DataTypeManager> data_type_manager_;

  ObserverList<Observer> observers_;

  syncer::SyncJsController sync_js_controller_;

  content::NotificationRegistrar registrar_;

  base::WeakPtrFactory<ProfileSyncService> weak_factory_;

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

  // If true, we want to automatically start sync signin whenever we have
  // credentials (user doesn't need to go through the startup flow). This is
  // typically enabled on platforms (like ChromeOS) that have their own
  // distinct signin flow.
  const bool auto_start_enabled_;

  scoped_ptr<browser_sync::BackendMigrator> migrator_;

  // This is the last |SyncProtocolError| we received from the server that had
  // an action set on it.
  syncer::SyncProtocolError last_actionable_error_;

  // This is used to show sync errors in the wrench menu.
  scoped_ptr<SyncGlobalError> sync_global_error_;

  // keeps track of data types that failed to load.
  FailedDatatypesHandler failed_datatypes_handler_;

  scoped_ptr<browser_sync::BackendUnrecoverableErrorHandler>
      backend_unrecoverable_error_handler_;

  browser_sync::DataTypeManager::ConfigureStatus configure_status_;

  // If |true|, there is setup UI visible so we should not start downloading
  // data types.
  bool setup_in_progress_;

  // The set of currently enabled sync experiments.
  syncer::Experiments current_experiments_;

  // Factory the backend will use to build the SyncManager.
  syncer::SyncManagerFactory sync_manager_factory_;

  // Holds the current invalidator state as updated by
  // OnInvalidatorStateChange().  Note that this is different from the
  // state known by |invalidator_registrar_| (See
  // UpdateInvalidatorState()).
  syncer::InvalidatorState invalidator_state_;

  // Dispatches invalidations to handlers.  Set in Initialize() and
  // unset in Shutdown().
  scoped_ptr<syncer::InvalidatorRegistrar> invalidator_registrar_;
  // Queues any acknowledgements received while the backend is uninitialized.
  AckHandleReplayQueue ack_replay_queue_;

  // Sync's internal debug info listener. Used to record datatype configuration
  // and association information.
  syncer::WeakHandle<syncer::DataTypeDebugInfoListener> debug_info_listener_;

  DISALLOW_COPY_AND_ASSIGN(ProfileSyncService);
};

bool ShouldShowActionOnUI(
    const syncer::SyncProtocolError& error);


#endif  // CHROME_BROWSER_SYNC_PROFILE_SYNC_SERVICE_H_
