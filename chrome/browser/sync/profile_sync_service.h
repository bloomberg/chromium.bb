// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_PROFILE_SYNC_SERVICE_H_
#define CHROME_BROWSER_SYNC_PROFILE_SYNC_SERVICE_H_

#include <string>
#include <map>
#include <vector>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/observer_list.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/google_service_auth_error.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/sync/glue/change_processor.h"
#include "chrome/browser/sync/glue/model_associator.h"
#include "chrome/browser/sync/glue/sync_backend_host.h"
#include "chrome/browser/sync/sync_setup_wizard.h"
#include "chrome/common/notification_registrar.h"
#include "googleurl/src/gurl.h"

class CommandLine;
class MessageLoop;
class Profile;

namespace browser_sync {
class ModelAssociator;

class UnrecoverableErrorHandler {
 public:
  // Call this when normal operation detects that the bookmark model and the
  // syncer model are inconsistent, or similar.  The ProfileSyncService will
  // try to avoid doing any work to avoid crashing or corrupting things
  // further, and will report an error status if queried.
  virtual void OnUnrecoverableError() = 0;
 protected:
  virtual ~UnrecoverableErrorHandler() { }
};

}

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
class ProfileSyncService : public NotificationObserver,
                           public browser_sync::SyncFrontend,
                           public browser_sync::UnrecoverableErrorHandler {
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

  explicit ProfileSyncService(Profile* profile);
  virtual ~ProfileSyncService();

  // Initializes the object. This should be called every time an object of this
  // class is constructed.
  void Initialize();

  // Enables/disables sync for user.
  virtual void EnableForUser();
  virtual void DisableForUser();

  // Whether sync is enabled by user or not.
  bool HasSyncSetupCompleted() const;
  void SetSyncSetupCompleted();

  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // SyncFrontend implementation.
  virtual void OnBackendInitialized();
  virtual void OnSyncCycleCompleted();
  virtual void OnAuthError();

  // Called when a user enters credentials through UI.
  virtual void OnUserSubmittedAuth(const std::string& username,
                                   const std::string& password);

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
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Record stats on various events.
  static void SyncEvent(SyncEventCodes code);

  // Returns whether sync is enabled.  Sync can be enabled/disabled both
  // at compile time (e.g., on a per-OS basis) or at run time (e.g.,
  // command-line switches).
  static bool IsSyncEnabled();

  // UnrecoverableErrorHandler implementation.
  virtual void OnUnrecoverableError();

  browser_sync::SyncBackendHost* backend() { return backend_.get(); }

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

  // Tests need to override this.
  virtual void InitializeBackend();

  // Tests need this.
  void set_model_associator(browser_sync::ModelAssociator* associator);

  // We keep track of the last auth error observed so we can cover up the first
  // "expected" auth failure from observers.
  // TODO(timsteele): Same as expecting_first_run_auth_needed_event_. Remove
  // this!
  GoogleServiceAuthError last_auth_error_;

  // Cache of the last name the client attempted to authenticate.
  std::string last_attempted_user_email_;

 private:
  friend class ProfileSyncServiceTest;
  friend class ProfileSyncServiceTestHarness;
  friend class TestModelAssociator;
  FRIEND_TEST(ProfileSyncServiceTest, UnrecoverableErrorSuspendsService);

  // Initializes the various settings from the command line.
  void InitSettings();

  // Whether the sync merge warning should be shown.
  bool MergeAndSyncAcceptanceNeeded() const;

  // Sets the last synced time to the current time.
  void UpdateLastSyncedTime();

  // Time at which we begin an attempt a GAIA authorization.
  base::TimeTicks auth_start_time_;

  // Time at which error UI is presented for the new tab page.
  base::TimeTicks auth_error_time_;

  // The profile whose data we are synchronizing.
  Profile* profile_;

  // TODO(ncarter): Put this in a profile, once there is UI for it.
  // This specifies where to find the sync server.
  GURL sync_service_url_;

  // Model association manager instance.
  scoped_refptr<browser_sync::ModelAssociator> model_associator_;

  // The last time we detected a successful transition from SYNCING state.
  // Our backend notifies us whenever we should take a new snapshot.
  base::Time last_synced_time_;

  // Our asynchronous backend to communicate with sync components living on
  // other threads.
  scoped_ptr<browser_sync::SyncBackendHost> backend_;

  scoped_ptr<browser_sync::ChangeProcessor> change_processor_;

  NotificationRegistrar registrar_;

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

  ObserverList<Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(ProfileSyncService);
};

#endif  // CHROME_BROWSER_SYNC_PROFILE_SYNC_SERVICE_H_
