// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_PROFILE_SYNC_SERVICE_H_
#define CHROME_BROWSER_SYNC_PROFILE_SYNC_SERVICE_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/observer_list.h"
#include "base/scoped_ptr.h"
#include "base/string16.h"
#include "base/time.h"
#include "base/timer.h"
#include "chrome/browser/prefs/pref_member.h"
#include "chrome/browser/sync/engine/syncapi.h"
#include "chrome/browser/sync/glue/data_type_controller.h"
#include "chrome/browser/sync/glue/data_type_manager.h"
#include "chrome/browser/sync/glue/sync_backend_host.h"
#include "chrome/browser/sync/js_event_handler_list.h"
#include "chrome/browser/sync/profile_sync_service_observer.h"
#include "chrome/browser/sync/signin_manager.h"
#include "chrome/browser/sync/sync_setup_wizard.h"
#include "chrome/browser/sync/syncable/model_type.h"
#include "chrome/browser/sync/unrecoverable_error_handler.h"
#include "chrome/common/net/gaia/google_service_auth_error.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "googleurl/src/gurl.h"

class NotificationDetails;
class NotificationSource;
class NotificationType;
class Profile;
class ProfileSyncFactory;
class TabContents;
class TokenMigrator;

namespace browser_sync {
class SessionModelAssociator;
class JsFrontend;
}  // namespace browser_sync

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
                           public browser_sync::UnrecoverableErrorHandler,
                           public NotificationObserver {
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

  ProfileSyncService(ProfileSyncFactory* factory_,
                     Profile* profile,
                     const std::string& cros_user);
  virtual ~ProfileSyncService();

  // Initializes the object. This should be called every time an object of this
  // class is constructed.
  void Initialize();

  void RegisterAuthNotifications();

  // Return whether all sync tokens are loaded and
  // available for the backend to start up.
  bool AreCredentialsAvailable();

  // Loads credentials migrated from the old user settings db.
  void LoadMigratedCredentials(const std::string& username,
                               const std::string& token);

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
  virtual void OnBackendInitialized();
  virtual void OnSyncCycleCompleted();
  virtual void OnAuthError();
  virtual void OnStopSyncingPermanently();
  virtual void OnClearServerDataFailed();
  virtual void OnClearServerDataTimeout();
  virtual void OnClearServerDataSucceeded();
  virtual void OnPassphraseRequired(bool for_decryption);
  virtual void OnPassphraseAccepted();
  virtual void OnEncryptionComplete(
      const syncable::ModelTypeSet& encrypted_types);

  // Called when a user enters credentials through UI.
  virtual void OnUserSubmittedAuth(const std::string& username,
                                   const std::string& password,
                                   const std::string& captcha,
                                   const std::string& access_code);

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

  const GoogleServiceAuthError& GetAuthError() const {
    return last_auth_error_;
  }

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
  virtual void ShowLoginDialog(gfx::NativeWindow parent_window);

  // This method handles clicks on "sync error" UI, showing the appropriate
  // dialog for the error condition (relogin / enter passphrase).
  virtual void ShowErrorUI(gfx::NativeWindow parent_window);

  void ShowConfigure(gfx::NativeWindow parent_window);
  void PromptForExistingPassphrase(gfx::NativeWindow parent_window);
  void SigninForPassphraseMigration(gfx::NativeWindow parent_window);

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
  bool sync_initialized() const { return backend_initialized_; }
  virtual bool unrecoverable_error_detected() const;
  const std::string& unrecoverable_error_message() {
    return unrecoverable_error_message_;
  }
  tracked_objects::Location unrecoverable_error_location() {
    return unrecoverable_error_location_.get() ?
        *unrecoverable_error_location_.get() : tracked_objects::Location();
  }

  bool UIShouldDepictAuthInProgress() const {
    return is_auth_in_progress_;
  }

  bool tried_creating_explicit_passphrase() const {
    return tried_creating_explicit_passphrase_;
  }

  bool tried_setting_explicit_passphrase() const {
    return tried_setting_explicit_passphrase_;
  }

  bool observed_passphrase_required() const {
    return observed_passphrase_required_;
  }

  bool passphrase_required_for_decryption() const {
    return passphrase_required_for_decryption_;
  }

  // A timestamp marking the last time the service observed a transition from
  // the SYNCING state to the READY state. Note that this does not reflect the
  // last time we polled the server to see if there were any changes; the
  // timestamp is only snapped when syncing takes place and we download or
  // upload some bookmark entity.
  const base::Time& last_synced_time() const { return last_synced_time_; }

  // Returns a user-friendly string form of last synced time (in minutes).
  virtual string16 GetLastSyncedTimeString() const;

  // Returns the authenticated username of the sync user, or empty if none
  // exists. It will only exist if the authentication service provider (e.g
  // GAIA) has confirmed the username is authentic.
  virtual string16 GetAuthenticatedUsername() const;

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

  // Returns a pointer to the service's JsFrontend (which is owned by
  // the service).  Never returns NULL.  Overrideable for testing
  // purposes.
  virtual browser_sync::JsFrontend* GetJsFrontend();

  // Record stats on various events.
  static void SyncEvent(SyncEventCodes code);

  // Returns whether sync is enabled.  Sync can be enabled/disabled both
  // at compile time (e.g., on a per-OS basis) or at run time (e.g.,
  // command-line switches).
  static bool IsSyncEnabled();

  // Returns whether sync is managed, i.e. controlled by configuration
  // management. If so, the user is not allowed to configure sync.
  bool IsManaged();

  // UnrecoverableErrorHandler implementation.
  virtual void OnUnrecoverableError(
      const tracked_objects::Location& from_here,
      const std::string& message);

  // The functions below (until ActivateDataType()) should only be
  // called if sync_initialized() is true.

  // TODO(akalin): This is called mostly by ModelAssociators and
  // tests.  Figure out how to pass the handle to the ModelAssociators
  // directly, figure out how to expose this to tests, and remove this
  // function.
  sync_api::UserShare* GetUserShare() const;

  // TODO(akalin): These two functions are used only by
  // ProfileSyncServiceHarness.  Figure out a different way to expose
  // this info to that class, and remove these functions.

  const browser_sync::sessions::SyncSessionSnapshot*
      GetLastSessionSnapshot() const;

  // Returns whether or not the underlying sync engine has made any
  // local changes to items that have not yet been synced with the
  // server.
  bool HasUnsyncedItems() const;

  // Get the current routing information for all enabled model types.
  // If a model type is not enabled (that is, if the syncer should not
  // be trying to sync it), it is not in this map.
  //
  // TODO(akalin): This function is used by
  // sync_ui_util::ConstructAboutInformation() and by some test
  // classes.  Figure out a different way to expose this info and
  // remove this function.
  void GetModelSafeRoutingInfo(browser_sync::ModelSafeRoutingInfo* out);

  // TODO(akalin): Remove these four functions once we're done with
  // autofill migration.

  syncable::AutofillMigrationState
      GetAutofillMigrationState();

  void SetAutofillMigrationState(
      syncable::AutofillMigrationState state);

  syncable::AutofillMigrationDebugInfo
      GetAutofillMigrationDebugInfo();

  void SetAutofillMigrationDebugInfo(
      syncable::AutofillMigrationDebugInfo::PropertyToSet property_to_set,
      const syncable::AutofillMigrationDebugInfo& info);

  virtual void ActivateDataType(
      browser_sync::DataTypeController* data_type_controller,
      browser_sync::ChangeProcessor* change_processor);
  virtual void DeactivateDataType(
      browser_sync::DataTypeController* data_type_controller,
      browser_sync::ChangeProcessor* change_processor);

  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

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
  // for sensitive data types.
  virtual bool IsCryptographerReady() const;

  // Returns true if a secondary passphrase is being used.
  virtual bool IsUsingSecondaryPassphrase() const;

  // Sets the Cryptographer's passphrase, or caches it until that is possible.
  // This will check asynchronously whether the passphrase is valid and notify
  // ProfileSyncServiceObservers via the NotificationService when the outcome
  // is known.
  // |is_explicit| is true if the call is in response to the user explicitly
  // setting a passphrase as opposed to implicitly (from the users' perspective)
  // using their Google Account password.  An implicit SetPassphrase will *not*
  // *not* override an explicit passphrase set previously.
  // |is_creation| is true if the call is in response to the user setting
  // up a new passphrase, and false if it's being set in response to a prompt
  // for an existing passphrase.
  virtual void SetPassphrase(const std::string& passphrase,
                             bool is_explicit,
                             bool is_creation);

  // Changes the set of datatypes that require encryption. This affects all
  // machines synced to this account and all data belonging to the specified
  // types.
  // Note that this is an asynchronous operation (the encryption of data is
  // performed on SyncBackendHost's core thread) and may not have an immediate
  // effect.
  virtual void EncryptDataTypes(
      const syncable::ModelTypeSet& encrypted_types);

  // Get the currently encrypted data types.
  virtual void GetEncryptedDataTypes(
      syncable::ModelTypeSet* encrypted_types) const;

  // Returns whether processing changes is allowed.  Check this before doing
  // any model-modifying operations.
  bool ShouldPushChanges();

  const GURL& sync_service_url() const { return sync_service_url_; }
  SigninManager* signin() { return signin_.get(); }
  const std::string& cros_user() const { return cros_user_; }

 protected:
  // Used by ProfileSyncServiceMock only.
  //
  // TODO(akalin): Separate this class out into an abstract
  // ProfileSyncService interface and a ProfileSyncServiceImpl class
  // so we don't need this hack anymore.
  ProfileSyncService();

  // Used by test classes that derive from ProfileSyncService.
  virtual browser_sync::SyncBackendHost* GetBackendForTest();

  // Helper to install and configure a data type manager.
  void ConfigureDataTypeManager();

  // Starts up the backend sync components.
  void StartUp();
  // Shuts down the backend sync components.
  // |sync_disabled| indicates if syncing is being disabled or not.
  void Shutdown(bool sync_disabled);

  // Methods to register and remove preferences.
  void RegisterPreferences();
  void ClearPreferences();

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

  // Whether the user has tried creating an explicit passphrase on this
  // machine.
  bool tried_creating_explicit_passphrase_;

  // Whether the user has tried setting an explicit passphrase on this
  // machine.
  bool tried_setting_explicit_passphrase_;

  // Whether we have seen a SYNC_PASSPHRASE_REQUIRED since initializing the
  // backend, telling us that it is safe to send a passphrase down ASAP.
  bool observed_passphrase_required_;

  // Was the last SYNC_PASSPHRASE_REQUIRED notification sent because it
  // was required for decryption?
  bool passphrase_required_for_decryption_;

  // Is the user in a passphrase migration?
  bool passphrase_migration_in_progress_;

 private:
  friend class ProfileSyncServicePasswordTest;
  friend class TestProfileSyncService;
  FRIEND_TEST_ALL_PREFIXES(ProfileSyncServiceTest, InitialState);

  // If |delete_sync_data_folder| is true, then this method will delete all
  // previous "Sync Data" folders. (useful if the folder is partial/corrupt).
  void InitializeBackend(bool delete_sync_data_folder);

  // Initializes the various settings from the command line.
  void InitSettings();

  // Sets the last synced time to the current time.
  void UpdateLastSyncedTime();

  void NotifyObservers();

  static const char* GetPrefNameForDataType(syncable::ModelType data_type);

  // Time at which we begin an attempt a GAIA authorization.
  base::TimeTicks auth_start_time_;

  // Time at which error UI is presented for the new tab page.
  base::TimeTicks auth_error_time_;

  // Factory used to create various dependent objects.
  ProfileSyncFactory* factory_;

  // The profile whose data we are synchronizing.
  Profile* profile_;

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
  scoped_ptr<tracked_objects::Location> unrecoverable_error_location_;

  // Manages the start and stop of the various data types.
  scoped_ptr<browser_sync::DataTypeManager> data_type_manager_;

  ObserverList<Observer> observers_;

  browser_sync::JsEventHandlerList js_event_handlers_;

  NotificationRegistrar registrar_;

  ScopedRunnableMethodFactory<ProfileSyncService>
      scoped_runnable_method_factory_;

  // The preference that controls whether sync is under control by configuration
  // management.
  BooleanPrefMember pref_sync_managed_;

  // This allows us to gracefully handle an ABORTED return code from the
  // DataTypeManager in the event that the server informed us to cease and
  // desist syncing immediately.
  bool expect_sync_configuration_aborted_;

  scoped_ptr<TokenMigrator> token_migrator_;

  // Sometimes we need to temporarily hold on to a passphrase because we don't
  // yet have a backend to send it to.  This happens during initialization as
  // we don't StartUp until we have a valid token, which happens after valid
  // credentials were provided.
  struct CachedPassphrase {
    std::string value;
    bool is_explicit;
    bool is_creation;
    CachedPassphrase() : is_explicit(false), is_creation(false) {}
  };
  CachedPassphrase cached_passphrase_;

  // TODO(tim): Remove this once new 'explicit passphrase' code flushes through
  // dev channel. See bug 62103.
  // To "migrate" early adopters of password sync on dev channel to the new
  // model that stores their secondary passphrase preference in the cloud, we
  // need some extra state since this cloud pref will be empty for all of them
  // regardless of how they set up sync, and we can't trust
  // kSyncUsingSecondaryPassphrase due to bugs in that implementation.
  bool tried_implicit_gaia_remove_when_bug_62103_fixed_;

  // Keep track of where we are in a server clear operation
  ClearServerDataState clear_server_data_state_;

  // Timeout for the clear data command.  This timeout is a temporary hack
  // and is necessary because the nudge sync framework can drop nudges for
  // a wide variety of sync-related conditions (throttling, connections issues,
  // syncer paused, etc.).  It can only be removed correctly when the framework
  // is reworked to allow one-shot commands like clearing server data.
  base::OneShotTimer<ProfileSyncService> clear_server_data_timer_;

  // The set of encrypted types. This is updated whenever datatypes are
  // encrypted through the OnEncryptionComplete callback of SyncFrontend.
  syncable::ModelTypeSet encrypted_types_;

  DISALLOW_COPY_AND_ASSIGN(ProfileSyncService);
};

#endif  // CHROME_BROWSER_SYNC_PROFILE_SYNC_SERVICE_H_
