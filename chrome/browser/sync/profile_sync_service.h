// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_PROFILE_SYNC_SERVICE_H_
#define CHROME_BROWSER_SYNC_PROFILE_SYNC_SERVICE_H_
#pragma once

#include <list>
#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/location.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/string16.h"
#include "base/task.h"
#include "base/time.h"
#include "base/timer.h"
#include "chrome/browser/sync/engine/model_safe_worker.h"
#include "chrome/browser/sync/failed_datatypes_handler.h"
#include "chrome/browser/sync/glue/data_type_controller.h"
#include "chrome/browser/sync/glue/sync_backend_host.h"
#include "chrome/browser/sync/internal_api/sync_manager.h"
#include "chrome/browser/sync/profile_sync_service_observer.h"
#include "chrome/browser/sync/syncable/model_type.h"
#include "chrome/browser/sync/sync_js_controller.h"
#include "chrome/browser/sync/sync_prefs.h"
#include "chrome/browser/sync/sync_setup_wizard.h"
#include "chrome/browser/sync/unrecoverable_error_handler.h"
#include "chrome/common/net/gaia/google_service_auth_error.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_types.h"
#include "googleurl/src/gurl.h"

class Profile;
class ProfileSyncComponentsFactory;
class SigninManager;
class SyncGlobalError;

namespace browser_sync {
class BackendMigrator;
class ChangeProcessor;
class DataTypeManager;
class JsController;
class SessionModelAssociator;
namespace sessions { struct SyncSessionSnapshot; }
}

namespace sync_api {
class BaseTransaction;
struct SyncCredentials;
struct UserShare;
}

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
class ProfileSyncService : public browser_sync::SyncFrontend,
                           public browser_sync::SyncPrefObserver,
                           public browser_sync::UnrecoverableErrorHandler,
                           public content::NotificationObserver,
                           // TODO(lipalani): crbug.com/100829. Instead of
                           // doing this vend weak pointers from a factory.
                           public base::SupportsWeakPtr<ProfileSyncService> {
 public:
  typedef ProfileSyncServiceObserver Observer;
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

  // Keep track of where we are when clearing server data.
  enum ClearServerDataState {
    CLEAR_NOT_STARTED = 1,
    CLEAR_CLEARING = 2,
    CLEAR_FAILED = 3,
    CLEAR_SUCCEEDED = 4,
  };

  // Default sync server URL.
  static const char* kSyncServerUrl;
  // Sync server URL for dev channel users
  static const char* kDevServerUrl;

  ProfileSyncService(ProfileSyncComponentsFactory* factory,
                     Profile* profile,
                     SigninManager* signin,  // Service takes ownership.
                     const std::string& cros_user);
  virtual ~ProfileSyncService();

  // Initializes the object. This should be called every time an object of this
  // class is constructed.
  void Initialize();

  void RegisterAuthNotifications();

  // Return whether all sync tokens are loaded and
  // available for the backend to start up.
  bool AreCredentialsAvailable();

  // Registers a data type controller with the sync service.  This
  // makes the data type controller available for use, it does not
  // enable or activate the synchronization of the data type (see
  // ActivateDataType).  Takes ownership of the pointer.
  void RegisterDataTypeController(
      browser_sync::DataTypeController* data_type_controller);

  // Returns the session model associator associated with this type, but only if
  // the associator is running.  If it is doing anything else, it will return
  // null.
  browser_sync::SessionModelAssociator* GetSessionModelAssociator();

  // Maintain state of where we are in a server clear operation.
  void ResetClearServerDataState();
  ClearServerDataState GetClearServerDataState();

  // Fills state_map with a map of current data types that are possible to
  // sync, as well as their states.
  void GetDataTypeControllerStates(
    browser_sync::DataTypeController::StateMap* state_map) const;

  // Disables sync for user. Use ShowLoginDialog to enable.
  virtual void DisableForUser();

  // Clears all Chromesync data from the server.
  void ClearServerData();

  // Whether sync is enabled by user or not.
  virtual bool HasSyncSetupCompleted() const;
  virtual void SetSyncSetupCompleted();

  // SyncFrontend implementation.
  virtual void OnBackendInitialized(
      const browser_sync::WeakHandle<browser_sync::JsBackend>& js_backend,
      bool success) OVERRIDE;
  virtual void OnSyncCycleCompleted() OVERRIDE;
  virtual void OnAuthError() OVERRIDE;
  virtual void OnStopSyncingPermanently() OVERRIDE;
  virtual void OnClearServerDataFailed() OVERRIDE;
  virtual void OnClearServerDataSucceeded() OVERRIDE;
  virtual void OnPassphraseRequired(
      sync_api::PassphraseRequiredReason reason) OVERRIDE;
  virtual void OnPassphraseAccepted() OVERRIDE;
  virtual void OnEncryptedTypesChanged(
      const syncable::ModelTypeSet& enncrypted_types,
      bool encrypt_everything) OVERRIDE;
  virtual void OnEncryptionComplete() OVERRIDE;
  virtual void OnMigrationNeededForTypes(
      const syncable::ModelTypeSet& types) OVERRIDE;
  virtual void OnDataTypesChanged(
      const syncable::ModelTypeSet& to_add) OVERRIDE;
  virtual void OnActionableError(
      const browser_sync::SyncProtocolError& error) OVERRIDE;

  void OnClearServerDataTimeout();

  // Called when a user enters credentials through UI.
  virtual void OnUserSubmittedAuth(const std::string& username,
                                   const std::string& password,
                                   const std::string& captcha,
                                   const std::string& access_code);

  // Called when a user enters credentials through UI.
  virtual void OnUserSubmittedOAuth(const std::string& oauth1_request_token);

  // Update the last auth error and notify observers of error state.
  void UpdateAuthErrorState(const GoogleServiceAuthError& error);

  // Called when a user chooses which data types to sync as part of the sync
  // setup wizard.  |sync_everything| represents whether they chose the
  // "keep everything synced" option; if true, |chosen_types| will be ignored
  // and all data types will be synced.  |sync_everything| means "sync all
  // current and future data types."
  virtual void OnUserChoseDatatypes(bool sync_everything,
      const syncable::ModelTypeSet& chosen_types);

  // Called when a user cancels any setup dialog (login, etc).
  virtual void OnUserCancelledDialog();

  // Get various information for displaying in the user interface.
  browser_sync::SyncBackendHost::StatusSummary QuerySyncStatusSummary();
  virtual browser_sync::SyncBackendHost::Status QueryDetailedSyncStatus();

  virtual const GoogleServiceAuthError& GetAuthError() const;

  // Displays a dialog for the user to enter GAIA credentials and attempt
  // re-authentication, and returns true if it actually opened the dialog.
  // Returns false if a dialog is already showing, an auth attempt is in
  // progress, the sync system is already authenticated, or some error
  // occurred preventing the action. We make it the duty of ProfileSyncService
  // to open the dialog to easily ensure only one is ever showing.
  virtual bool SetupInProgress() const;
  bool WizardIsVisible() const {
    return wizard_.IsVisible();
  }

  SyncSetupWizard& get_wizard() { return wizard_; }

  // Shows the login screen of the Sync setup wizard.
  virtual void ShowLoginDialog();

  // This method handles clicks on "sync error" UI, showing the appropriate
  // dialog for the error condition (relogin / enter passphrase).
  virtual void ShowErrorUI();

  // Shows the configure screen of the Sync setup wizard. If |sync_everything|
  // is true, shows the corresponding page in the customize screen; otherwise,
  // displays the page that gives the user the ability to select which data
  // types to sync.
  void ShowConfigure(bool sync_everything);

  virtual void ShowSyncSetup(const std::string& sub_page);
  void ShowSyncSetupWithWizard(SyncSetupWizard::State state);

  // Pretty-printed strings for a given StatusSummary.
  static std::string BuildSyncStatusSummaryText(
      const browser_sync::SyncBackendHost::StatusSummary& summary);

  // Returns true if the SyncBackendHost has told us it's ready to accept
  // changes.
  // [REMARK] - it is safe to call this function only from the ui thread.
  // because the variable is not thread safe and should only be accessed from
  // single thread. If we want multiple threads to access this(and there is
  // currently no need to do so) we need to protect this with a lock.
  // TODO(timsteele): What happens if the bookmark model is loaded, a change
  // takes place, and the backend isn't initialized yet?
  virtual bool sync_initialized() const;
  virtual bool unrecoverable_error_detected() const;
  const std::string& unrecoverable_error_message() {
    return unrecoverable_error_message_;
  }
  tracked_objects::Location unrecoverable_error_location() {
    return unrecoverable_error_location_;
  }

  virtual bool UIShouldDepictAuthInProgress() const;

  // Returns true if OnPassphraseRequired has been called for any reason.
  virtual bool IsPassphraseRequired() const;

  // Returns true if OnPassphraseRequired has been called for decryption and
  // we have an encrypted data type enabled.
  virtual bool IsPassphraseRequiredForDecryption() const;

  sync_api::PassphraseRequiredReason passphrase_required_reason() const {
    return passphrase_required_reason_;
  }

  // Returns a user-friendly string form of last synced time (in minutes).
  virtual string16 GetLastSyncedTimeString() const;

  const std::string& last_attempted_user_email() const {
    return last_attempted_user_email_;
  }

  // The profile we are syncing for.
  Profile* profile() const { return profile_; }

  // Adds/removes an observer. ProfileSyncService does not take ownership of
  // the observer.
  virtual void AddObserver(Observer* observer);
  virtual void RemoveObserver(Observer* observer);

  // Returns true if |observer| has already been added as an observer.
  bool HasObserver(Observer* observer) const;

  // Returns a weak pointer to the service's JsController.
  // Overrideable for testing purposes.
  virtual base::WeakPtr<browser_sync::JsController> GetJsController();

  // Record stats on various events.
  static void SyncEvent(SyncEventCodes code);

  // Returns whether sync is enabled.  Sync can be enabled/disabled both
  // at compile time (e.g., on a per-OS basis) or at run time (e.g.,
  // command-line switches).
  static bool IsSyncEnabled();

  // Returns whether sync is managed, i.e. controlled by configuration
  // management. If so, the user is not allowed to configure sync.
  bool IsManaged() const;

  // UnrecoverableErrorHandler implementation.
  virtual void OnUnrecoverableError(
      const tracked_objects::Location& from_here,
      const std::string& message) OVERRIDE;

  // The functions below (until ActivateDataType()) should only be
  // called if sync_initialized() is true.

  // TODO(akalin): This is called mostly by ModelAssociators and
  // tests.  Figure out how to pass the handle to the ModelAssociators
  // directly, figure out how to expose this to tests, and remove this
  // function.
  virtual sync_api::UserShare* GetUserShare() const;

  // TODO(akalin): These two functions are used only by
  // ProfileSyncServiceHarness.  Figure out a different way to expose
  // this info to that class, and remove these functions.

  virtual const browser_sync::sessions::SyncSessionSnapshot*
      GetLastSessionSnapshot() const;

  // Returns whether or not the underlying sync engine has made any
  // local changes to items that have not yet been synced with the
  // server.
  bool HasUnsyncedItems() const;

  // Used by ProfileSyncServiceHarness.  May return NULL.
  browser_sync::BackendMigrator* GetBackendMigratorForTest();

  // Get the current routing information for all enabled model types.
  // If a model type is not enabled (that is, if the syncer should not
  // be trying to sync it), it is not in this map.
  //
  // TODO(akalin): This function is used by
  // sync_ui_util::ConstructAboutInformation() and by some test
  // classes.  Figure out a different way to expose this info and
  // remove this function.
  void GetModelSafeRoutingInfo(
      browser_sync::ModelSafeRoutingInfo* out) const;

  // Overridden by tests.
  // TODO(zea): Remove these and have the dtc's call directly into the SBH.
  virtual void ActivateDataType(
      syncable::ModelType type, browser_sync::ModelSafeGroup group,
      browser_sync::ChangeProcessor* change_processor);
  virtual void DeactivateDataType(syncable::ModelType type);

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
      const syncable::ModelTypeSet& preferred_types);

  // Get the set of currently enabled data types (as chosen or configured by
  // the user).  See class comment for more on what it means for a datatype
  // to be Preferred.
  virtual void GetPreferredDataTypes(
      syncable::ModelTypeSet* preferred_types) const;

  // Gets the set of all data types that could be allowed (the set that
  // should be advertised to the user).  These will typically only change
  // via a command-line option.  See class comment for more on what it means
  // for a datatype to be Registered.
  virtual void GetRegisteredDataTypes(
      syncable::ModelTypeSet* registered_types) const;

  // Checks whether the Cryptographer is ready to encrypt and decrypt updates
  // for sensitive data types. Caller must be holding a
  // syncapi::BaseTransaction to ensure thread safety.
  virtual bool IsCryptographerReady(
      const sync_api::BaseTransaction* trans) const;

  // Returns true if a secondary passphrase is being used. It is not legal
  // to call this method before the backend is initialized.
  virtual bool IsUsingSecondaryPassphrase() const;

  // Sets the Cryptographer's passphrase, or caches it until that is possible.
  // This will check asynchronously whether the passphrase is valid and notify
  // ProfileSyncServiceObservers via the NotificationService when the outcome
  // is known.
  // |is_explicit| is true if the call is in response to the user explicitly
  // setting a passphrase as opposed to implicitly (from the users' perspective)
  // using their Google Account password.  An implicit SetPassphrase will
  // *not* override an explicit passphrase set previously.
  virtual void SetPassphrase(const std::string& passphrase,
                             bool is_explicit);

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
  virtual void GetEncryptedDataTypes(
      syncable::ModelTypeSet* encrypted_types) const;

  // Returns true if the syncer is waiting for new datatypes to be encrypted.
  virtual bool encryption_pending() const;

  // Returns whether processing changes is allowed.  Check this before doing
  // any model-modifying operations.
  bool ShouldPushChanges();

  const GURL& sync_service_url() const { return sync_service_url_; }
  SigninManager* signin() { return signin_.get(); }
  const std::string& cros_user() const { return cros_user_; }
  bool auto_start_enabled() const { return auto_start_enabled_; }

  // Stops the sync backend and sets the flag for suppressing sync startup.
  void StopAndSuppress();

  // Resets the flag for suppressing sync startup and starts the sync backend.
  void UnsuppressAndStart();

  // Marks all currently registered types as "acknowledged" so we won't prompt
  // the user about them any more.
  void AcknowledgeSyncedTypes();

  SyncGlobalError* sync_global_error() { return sync_global_error_.get(); }

  virtual const FailedDatatypesHandler& failed_datatypes_handler();

 protected:
  // Used by test classes that derive from ProfileSyncService.
  virtual browser_sync::SyncBackendHost* GetBackendForTest();

  // Helper to install and configure a data type manager.
  void ConfigureDataTypeManager();

  // Starts up the backend sync components.
  void StartUp();
  // Shuts down the backend sync components.
  // |sync_disabled| indicates if syncing is being disabled or not.
  void Shutdown(bool sync_disabled);

  // Return SyncCredentials from the TokenService.
  sync_api::SyncCredentials GetCredentials();

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

  // The wizard will try to read the auth state out of the profile sync
  // service using this member. Captcha and error state are reflected.
  GoogleServiceAuthError last_auth_error_;

  // Our asynchronous backend to communicate with sync components living on
  // other threads.
  scoped_ptr<browser_sync::SyncBackendHost> backend_;

  // Cache of the last name the client attempted to authenticate.
  std::string last_attempted_user_email_;

  // Was the last SYNC_PASSPHRASE_REQUIRED notification sent because it
  // was required for encryption, decryption with a cached passphrase, or
  // because a new passphrase is required?
  sync_api::PassphraseRequiredReason passphrase_required_reason_;

 private:
  friend class ProfileSyncServicePasswordTest;
  friend class TestProfileSyncService;
  friend class ProfileSyncServiceForWizardTest;
  FRIEND_TEST_ALL_PREFIXES(ProfileSyncServiceTest, InitialState);

  // Starts up sync if it is not suppressed and preconditions are met.
  // Called from Initialize() and UnsuppressAndStart().
  void TryStart();

  // Called when we've determined that we don't need a passphrase (either
  // because OnPassphraseAccepted() was called, or because we've gotten a
  // OnPassphraseRequired() but no data types are enabled).
  void ResolvePassphraseRequired();

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

  // About-flags experiment names for datatypes that aren't enabled by default
  // yet.
  static std::string GetExperimentNameForDataType(
      syncable::ModelType data_type);

  // Create and register a new datatype controller.
  void RegisterNewDataType(syncable::ModelType data_type);

  // Helper method to process SyncConfigureDone after unwinding the stack that
  // originally posted this SyncConfigureDone.
  void OnSyncConfigureDone(
      browser_sync::DataTypeManager::ConfigureResult result);

  // Reconfigures the data type manager with the latest enabled types.
  // Note: Does not initialize the backend if it is not already initialized.
  // This function needs to be called only after sync has been initialized
  // (i.e.,only for reconfigurations). The reason we don't initialize the
  // backend is because if we had encountered an unrecoverable error we dont
  // want to startup once more.
  virtual void ReconfigureDatatypeManager();


  // Time at which we begin an attempt a GAIA authorization.
  base::TimeTicks auth_start_time_;

  // Time at which error UI is presented for the new tab page.
  base::TimeTicks auth_error_time_;

  // Factory used to create various dependent objects.
  ProfileSyncComponentsFactory* factory_;

  // The profile whose data we are synchronizing.
  Profile* profile_;

  // The class that handles getting, setting, and persisting sync
  // preferences.
  browser_sync::SyncPrefs sync_prefs_;

  // Email for the ChromiumOS user, if we're running under ChromiumOS.
  std::string cros_user_;

  // TODO(ncarter): Put this in a profile, once there is UI for it.
  // This specifies where to find the sync server.
  GURL sync_service_url_;

  // The last time we detected a successful transition from SYNCING state.
  // Our backend notifies us whenever we should take a new snapshot.
  base::Time last_synced_time_;

  // List of available data type controllers.
  browser_sync::DataTypeController::TypeMap data_type_controllers_;

  // Whether the SyncBackendHost has been initialized.
  bool backend_initialized_;

  // Various pieces of UI query this value to determine if they should show
  // an "Authenticating.." type of message.  We are the only central place
  // all auth attempts funnel through, so it makes sense to provide this.
  // As its name suggests, this should NOT be used for anything other than UI.
  bool is_auth_in_progress_;

  SyncSetupWizard wizard_;

  // Encapsulates user signin with TokenService.
  scoped_ptr<SigninManager> signin_;

  // True if an unrecoverable error (e.g. violation of an assumed invariant)
  // occurred during syncer operation.  This value should be checked before
  // doing any work that might corrupt things further.
  bool unrecoverable_error_detected_;

  // A message sent when an unrecoverable error occurred.
  std::string unrecoverable_error_message_;
  tracked_objects::Location unrecoverable_error_location_;

  // Manages the start and stop of the various data types.
  scoped_ptr<browser_sync::DataTypeManager> data_type_manager_;

  ObserverList<Observer> observers_;

  browser_sync::SyncJsController sync_js_controller_;

  content::NotificationRegistrar registrar_;

  ScopedRunnableMethodFactory<ProfileSyncService>
      scoped_runnable_method_factory_;

  // This allows us to gracefully handle an ABORTED return code from the
  // DataTypeManager in the event that the server informed us to cease and
  // desist syncing immediately.
  bool expect_sync_configuration_aborted_;

  // Sometimes we need to temporarily hold on to a passphrase because we don't
  // yet have a backend to send it to.  This happens during initialization as
  // we don't StartUp until we have a valid token, which happens after valid
  // credentials were provided.
  struct CachedPassphrases {
    std::string explicit_passphrase;
    std::string gaia_passphrase;
  };
  CachedPassphrases cached_passphrases_;

  // Keep track of where we are in a server clear operation
  ClearServerDataState clear_server_data_state_;

  // Timeout for the clear data command.  This timeout is a temporary hack
  // and is necessary because the nudge sync framework can drop nudges for
  // a wide variety of sync-related conditions (throttling, connections issues,
  // syncer paused, etc.).  It can only be removed correctly when the framework
  // is reworked to allow one-shot commands like clearing server data.
  base::OneShotTimer<ProfileSyncService> clear_server_data_timer_;

  // The current set of encrypted types.  Always a superset of
  // Cryptographer::SensitiveTypes().
  syncable::ModelTypeSet encrypted_types_;

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
  bool auto_start_enabled_;

  scoped_ptr<browser_sync::BackendMigrator> migrator_;

  // This is the last |SyncProtocolError| we received from the server that had
  // an action set on it.
  browser_sync::SyncProtocolError last_actionable_error_;

  // This is used to show sync errors in the wrench menu.
  scoped_ptr<SyncGlobalError> sync_global_error_;

  // keeps track of data types that failed to load.
  FailedDatatypesHandler failed_datatypes_handler_;

  DISALLOW_COPY_AND_ASSIGN(ProfileSyncService);
};

bool ShouldShowActionOnUI(
    const browser_sync::SyncProtocolError& error);


#endif  // CHROME_BROWSER_SYNC_PROFILE_SYNC_SERVICE_H_
