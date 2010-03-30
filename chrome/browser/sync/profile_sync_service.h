// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_PROFILE_SYNC_SERVICE_H_
#define CHROME_BROWSER_SYNC_PROFILE_SYNC_SERVICE_H_

#include <string>
#include <map>

#include "base/basictypes.h"
#include "base/observer_list.h"
#include "base/scoped_ptr.h"
#include "base/time.h"
#include "chrome/browser/google_service_auth_error.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/sync/glue/data_type_controller.h"
#include "chrome/browser/sync/glue/data_type_manager.h"
#include "chrome/browser/sync/glue/sync_backend_host.h"
#include "chrome/browser/sync/notification_method.h"
#include "chrome/browser/sync/sync_setup_wizard.h"
#include "chrome/browser/sync/syncable/model_type.h"
#include "chrome/browser/sync/unrecoverable_error_handler.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

class NotificationDetails;
class NotificationSource;
class NotificationType;

// Various UI components such as the New Tab page can be driven by observing
// the ProfileSyncService through this interface.
class ProfileSyncServiceObserver {
 public:
  // When one of the following events occurs, OnStateChanged() is called.
  // Observers should query the service to determine what happened.
  // - We initialized successfully.
  // - There was an authentication error and the user needs to reauthenticate.
  // - The sync servers are unavailable at this time.
  // - Credentials are now in flight for authentication.
  virtual void OnStateChanged() = 0;
 protected:
  virtual ~ProfileSyncServiceObserver() { }
};

// ProfileSyncService is the layer between browser subsystems like bookmarks,
// and the sync backend.
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

    // Events regarding cancelation of the signon process of sync.
    CANCEL_FROM_SIGNON_WITHOUT_AUTH = 10,   // Cancelled before submitting
                                            // username and password.
    CANCEL_DURING_SIGNON = 11,              // Cancelled after auth.
    CANCEL_DURING_SIGNON_AFTER_MERGE = 12,  // Cancelled during merge.

    // Events resulting in the stoppage of sync service.
    STOP_FROM_OPTIONS = 20,  // Sync was stopped from Wrench->Options.

    // Miscellaneous events caused by sync service.
    MERGE_AND_SYNC_NEEDED = 30,

    MAX_SYNC_EVENT_CODE
  };

  ProfileSyncService(ProfileSyncFactory* factory_,
                     Profile* profile,
                     bool bootstrap_sync_authentication);
  virtual ~ProfileSyncService();

  // Initializes the object. This should be called every time an object of this
  // class is constructed.
  void Initialize();

  // Registers a data type controller with the sync service.  This
  // makes the data type controller available for use, it does not
  // enable or activate the synchronization of the data type (see
  // ActivateDataType).  Takes ownership of the pointer.
  void RegisterDataTypeController(
      browser_sync::DataTypeController* data_type_controller);

  const browser_sync::DataTypeController::TypeMap& data_type_controllers()
      const {
    return data_type_controllers_;
  }

  // Enables/disables sync for user.
  virtual void EnableForUser();
  virtual void DisableForUser();

  // Whether sync is enabled by user or not.
  virtual bool HasSyncSetupCompleted() const;
  void SetSyncSetupCompleted();

  // SyncFrontend implementation.
  virtual void OnBackendInitialized();
  virtual void OnSyncCycleCompleted();
  virtual void OnAuthError();

  // Called when a user enters credentials through UI.
  virtual void OnUserSubmittedAuth(const std::string& username,
                                   const std::string& password,
                                   const std::string& captcha);

  // Called when a user decides whether to merge and sync or abort.
  virtual void OnUserAcceptedMergeAndSync();

  // Called when a user cancels any setup dialog (login, merge and sync, etc).
  virtual void OnUserCancelledDialog();

  // Get various information for displaying in the user interface.
  browser_sync::SyncBackendHost::StatusSummary QuerySyncStatusSummary();
  browser_sync::SyncBackendHost::Status QueryDetailedSyncStatus();

  const GoogleServiceAuthError& GetAuthError() const {
    return last_auth_error_;
  }

  // Displays a dialog for the user to enter GAIA credentials and attempt
  // re-authentication, and returns true if it actually opened the dialog.
  // Returns false if a dialog is already showing, an auth attempt is in
  // progress, the sync system is already authenticated, or some error
  // occurred preventing the action. We make it the duty of ProfileSyncService
  // to open the dialog to easily ensure only one is ever showing.
  bool SetupInProgress() const {
    return !HasSyncSetupCompleted() && WizardIsVisible();
  }
  bool WizardIsVisible() const {
    return wizard_.IsVisible();
  }
  void ShowLoginDialog();

  // Pretty-printed strings for a given StatusSummary.
  static std::wstring BuildSyncStatusSummaryText(
      const browser_sync::SyncBackendHost::StatusSummary& summary);

  // Returns true if the SyncBackendHost has told us it's ready to accept
  // changes.
  // TODO(timsteele): What happens if the bookmark model is loaded, a change
  // takes place, and the backend isn't initialized yet?
  bool sync_initialized() const { return backend_initialized_; }
  bool unrecoverable_error_detected() const {
    return unrecoverable_error_detected_;
  }

  bool UIShouldDepictAuthInProgress() const {
    return is_auth_in_progress_;
  }

  // A timestamp marking the last time the service observed a transition from
  // the SYNCING state to the READY state. Note that this does not reflect the
  // last time we polled the server to see if there were any changes; the
  // timestamp is only snapped when syncing takes place and we download or
  // upload some bookmark entity.
  const base::Time& last_synced_time() const { return last_synced_time_; }

  // Returns a user-friendly string form of last synced time (in minutes).
  std::wstring GetLastSyncedTimeString() const;

  // Returns the authenticated username of the sync user, or empty if none
  // exists. It will only exist if the authentication service provider (e.g
  // GAIA) has confirmed the username is authentic.
  virtual string16 GetAuthenticatedUsername() const;

  const std::string& last_attempted_user_email() const {
    return last_attempted_user_email_;
  }

  // The profile we are syncing for.
  Profile* profile() { return profile_; }

  // Adds/removes an observer. ProfileSyncService does not take ownership of
  // the observer.
  virtual void AddObserver(Observer* observer);
  virtual void RemoveObserver(Observer* observer);

  // Record stats on various events.
  static void SyncEvent(SyncEventCodes code);

  // Returns whether sync is enabled.  Sync can be enabled/disabled both
  // at compile time (e.g., on a per-OS basis) or at run time (e.g.,
  // command-line switches).
  static bool IsSyncEnabled();

  // UnrecoverableErrorHandler implementation.
  virtual void OnUnrecoverableError();

  browser_sync::SyncBackendHost* backend() { return backend_.get(); }

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

 protected:
  // Call this after any of the subsystems being synced (the bookmark
  // model and the sync backend) finishes its initialization.  When everything
  // is ready, this function will bootstrap the subsystems so that they are
  // initially in sync, and start forwarding changes between the two models.
  void StartProcessingChangesIfReady();

  // Returns whether processing changes is allowed.  Check this before doing
  // any model-modifying operations.
  bool ShouldPushChanges();

  // Starts up the backend sync components.
  void StartUp();
  // Shuts down the backend sync components.
  // |sync_disabled| indicates if syncing is being disabled or not.
  void Shutdown(bool sync_disabled);

  // Methods to register and remove preferences.
  void RegisterPreferences();
  void ClearPreferences();

  // Tests need to override this.  If |delete_sync_data_folder| is true, then
  // this method will delete all previous "Sync Data" folders. (useful if the
  // folder is partial/corrupt)
  virtual void InitializeBackend(bool delete_sync_data_folder);

  // We keep track of the last auth error observed so we can cover up the first
  // "expected" auth failure from observers.
  // TODO(timsteele): Same as expecting_first_run_auth_needed_event_. Remove
  // this!
  GoogleServiceAuthError last_auth_error_;

  // Cache of the last name the client attempted to authenticate.
  std::string last_attempted_user_email_;

 private:
  friend class ProfileSyncServiceTest;
  friend class ProfileSyncServicePreferenceTest;
  friend class ProfileSyncServiceTestHarness;
  FRIEND_TEST(ProfileSyncServiceTest, UnrecoverableErrorSuspendsService);

  // Initializes the various settings from the command line.
  void InitSettings();

  // Sets the last synced time to the current time.
  void UpdateLastSyncedTime();

  // When running inside Chrome OS, extract the LSID cookie from the cookie
  // store to bootstrap the authentication process.
  virtual std::string GetLsidForAuthBootstraping();

  // Time at which we begin an attempt a GAIA authorization.
  base::TimeTicks auth_start_time_;

  // Time at which error UI is presented for the new tab page.
  base::TimeTicks auth_error_time_;

  // Factory used to create various dependent objects.
  ProfileSyncFactory* factory_;

  // The profile whose data we are synchronizing.
  Profile* profile_;

  // True if the profile sync service should attempt to use an LSID
  // cookie for authentication.  This is typically set to true in
  // ChromiumOS since we want to use the system level authentication
  // for sync.
  bool bootstrap_sync_authentication_;

  // TODO(ncarter): Put this in a profile, once there is UI for it.
  // This specifies where to find the sync server.
  GURL sync_service_url_;

  // The last time we detected a successful transition from SYNCING state.
  // Our backend notifies us whenever we should take a new snapshot.
  base::Time last_synced_time_;

  // Our asynchronous backend to communicate with sync components living on
  // other threads.
  scoped_ptr<browser_sync::SyncBackendHost> backend_;

  // List of available data type controllers.
  browser_sync::DataTypeController::TypeMap data_type_controllers_;

  // Whether the SyncBackendHost has been initialized.
  bool backend_initialized_;

  // Set to true when the user first enables sync, and we are waiting for
  // syncapi to give us the green light on providing credentials for the first
  // time. It is set back to false as soon as we get this message, and is
  // false all other times so we don't have to persist this value as it will
  // get initialized to false.
  // TODO(timsteele): Remove this by way of starting the wizard when enabling
  // sync *before* initializing the backend. syncapi will need to change, but
  // it means we don't have to wait for the first AuthError; if we ever get
  // one, it is actually an error and this bool isn't needed.
  bool expecting_first_run_auth_needed_event_;

  // Various pieces of UI query this value to determine if they should show
  // an "Authenticating.." type of message.  We are the only central place
  // all auth attempts funnel through, so it makes sense to provide this.
  // As its name suggests, this should NOT be used for anything other than UI.
  bool is_auth_in_progress_;

  SyncSetupWizard wizard_;

  // True if an unrecoverable error (e.g. violation of an assumed invariant)
  // occurred during syncer operation.  This value should be checked before
  // doing any work that might corrupt things further.
  bool unrecoverable_error_detected_;

  // Which peer-to-peer notification method to use.
  browser_sync::NotificationMethod notification_method_;

  // Manages the start and stop of the various data types.
  scoped_ptr<browser_sync::DataTypeManager> data_type_manager_;

  ObserverList<Observer> observers_;

  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ProfileSyncService);
};

#endif  // CHROME_BROWSER_SYNC_PROFILE_SYNC_SERVICE_H_
