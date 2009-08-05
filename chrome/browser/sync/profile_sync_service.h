// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef CHROME_PERSONALIZATION

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
#include "chrome/browser/profile.h"
#include "chrome/browser/sync/glue/model_associator.h"
#include "chrome/browser/sync/glue/sync_backend_host.h"
#include "chrome/browser/views/sync/sync_setup_wizard.h"
#include "googleurl/src/gurl.h"

class CommandLine;
class MessageLoop;
class Profile;

namespace browser_sync {
class ModelAssociator;
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
class ProfileSyncService : public BookmarkModelObserver,
                           public browser_sync::SyncFrontend {
 public:
  typedef ProfileSyncServiceObserver Observer;
  typedef browser_sync::SyncBackendHost::Status Status;

  explicit ProfileSyncService(Profile* profile);
  virtual ~ProfileSyncService();

  // Initializes the object. This should be called every time an object of this
  // class is constructed.
  void Initialize();

  // Enables/disables sync for user.
  virtual void EnableForUser();
  virtual void DisableForUser();

  // Whether sync is enabled by user or not.
  bool IsSyncEnabledByUser() const;

  // BookmarkModelObserver implementation.
  virtual void Loaded(BookmarkModel* model);
  virtual void BookmarkModelBeingDeleted(BookmarkModel* model) {}
  virtual void BookmarkNodeMoved(BookmarkModel* model,
                                 const BookmarkNode* old_parent,
                                 int old_index,
                                 const BookmarkNode* new_parent,
                                 int new_index);
  virtual void BookmarkNodeAdded(BookmarkModel* model,
                                 const BookmarkNode* parent,
                                 int index);
  virtual void BookmarkNodeRemoved(BookmarkModel* model,
                                   const BookmarkNode* parent,
                                   int index,
                                   const BookmarkNode* node);
  virtual void BookmarkNodeChanged(BookmarkModel* model,
                                   const BookmarkNode* node);
  virtual void BookmarkNodeFavIconLoaded(BookmarkModel* model,
                                         const BookmarkNode* node);
  virtual void BookmarkNodeChildrenReordered(BookmarkModel* model,
                                             const BookmarkNode* node);

  // SyncFrontend implementation.
  virtual void OnBackendInitialized();
  virtual void OnSyncCycleCompleted();
  virtual void OnAuthError();
  virtual void ApplyModelChanges(
      const sync_api::BaseTransaction* trans,
      const sync_api::SyncManager::ChangeRecord* changes,
      int change_count);

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

  AuthErrorState GetAuthErrorState() const {
    return last_auth_error_;
  }

  // Displays a dialog for the user to enter GAIA credentials and attempt
  // re-authentication, and returns true if it actually opened the dialog.
  // Returns false if a dialog is already showing, an auth attempt is in
  // progress, the sync system is already authenticated, or some error
  // occurred preventing the action. We make it the duty of ProfileSyncService
  // to open the dialog to easily ensure only one is ever showing.
  bool SetupInProgress() const {
    return !IsSyncEnabledByUser() && WizardIsVisible();
  }
  bool WizardIsVisible() const { return wizard_.IsVisible(); }
  void ShowLoginDialog();

  // Pretty-printed strings for a given StatusSummary.
  static std::wstring BuildSyncStatusSummaryText(
      const browser_sync::SyncBackendHost::StatusSummary& summary);

  // Returns true if the SyncBackendHost has told us it's ready to accept
  // changes.
  // TODO(timsteele): What happens if the bookmark model is loaded, a change
  // takes place, and the backend isn't initialized yet?
  bool sync_initialized() const { return backend_initialized_; }

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

 protected:
  // Call this after any of the subsystems being synced (the bookmark
  // model and the sync backend) finishes its initialization.  When everything
  // is ready, this function will bootstrap the subsystems so that they are
  // initially in sync, and start forwarding changes between the two models.
  void StartProcessingChangesIfReady();

  // Various member accessors needed by unit tests.
  browser_sync::SyncBackendHost* backend() { return backend_.get(); }

  // Call this when normal operation detects that the bookmark model and the
  // syncer model are inconsistent, or similar.  The ProfileSyncService will
  // try to avoid doing any work to avoid crashing or corrupting things
  // further, and will report an error status if queried.
  void SetUnrecoverableError();

  // Returns whether processing changes is allowed.  Check this before doing
  // any model-modifying operations.
  bool ShouldPushChanges();

  // Starts up the backend sync components.
  void StartUp();
  // Shuts down the backend sync components.
  // |sync_disabled| indicates if syncing is being disabled or not.
  void Shutdown(bool sync_disabled);

  // Tests need to override this.
  virtual void InitializeBackend();

  // Tests need this.
  void set_model_associator(browser_sync::ModelAssociator* manager) {
    model_associator_ = manager;
  }

  // We keep track of the last auth error observed so we can cover up the first
  // "expected" auth failure from observers.
  // TODO(timsteele): Same as expecting_first_run_auth_needed_event_. Remove
  // this!
  AuthErrorState last_auth_error_;

  // Cache of the last name the client attempted to authenticate.
  std::string last_attempted_user_email_;

 private:
  friend class browser_sync::ModelAssociator;
  friend class ProfileSyncServiceTest;
  friend class ProfileSyncServiceTestHarness;
  friend class TestModelAssociator;
  FRIEND_TEST(ProfileSyncServiceTest, UnrecoverableErrorSuspendsService);

  enum MoveOrCreate {
    MOVE,
    CREATE,
  };

  // Initializes the various settings from the command line.
  void InitSettings();

  // Methods to register, load and remove preferences.
  void RegisterPreferences();
  void LoadPreferences();
  void ClearPreferences();

  // Treat the |index|th child of |parent| as a newly added node, and create a
  // corresponding node in the sync domain using |trans|.  All properties
  // will be transferred to the new node.  A node corresponding to |parent|
  // must already exist and be associated for this call to succeed.  Returns
  // the ID of the just-created node, or if creation fails, kInvalidID.
  int64 CreateSyncNode(const BookmarkNode* parent,
                       int index,
                       sync_api::WriteTransaction* trans);

  // Create a bookmark node corresponding to |src| if one is not already
  // associated with |src|.  Returns the node that was created or updated.
  const BookmarkNode* CreateOrUpdateBookmarkNode(
      sync_api::BaseNode* src,
      BookmarkModel* model);

  // Creates a bookmark node under the given parent node from the given sync
  // node. Returns the newly created node.
  const BookmarkNode* CreateBookmarkNode(
      sync_api::BaseNode* sync_node,
      const BookmarkNode* parent,
      int index) const;

  // Sets the favicon of the given bookmark node from the given sync node.
  // Returns whether the favicon was set in the bookmark node.
  bool SetBookmarkFavicon(sync_api::BaseNode* sync_node,
                          const BookmarkNode* bookmark_node) const;

  // Sets the favicon of the given sync node from the given bookmark node.
  void SetSyncNodeFavicon(const BookmarkNode* bookmark_node,
                          sync_api::WriteNode* sync_node) const;

  // Helper function to determine the appropriate insertion index of sync node
  // |node| under the Bookmark model node |parent|, to make the positions
  // match up between the two models. This presumes that the predecessor of the
  // item (in the bookmark model) has already been moved into its appropriate
  // position.
  int CalculateBookmarkModelInsertionIndex(
      const BookmarkNode* parent,
      const sync_api::BaseNode* node) const;

  // Helper function used to fix the position of a sync node so that it matches
  // the position of a corresponding bookmark model node. |parent| and
  // |index| identify the bookmark model position.  |dst| is the node whose
  // position is to be fixed.  If |operation| is CREATE, treat |dst| as an
  // uncreated node and set its position via InitByCreation(); otherwise,
  // |dst| is treated as an existing node, and its position will be set via
  // SetPosition().  |trans| is the transaction to which |dst| belongs. Returns
  // false on failure.
  bool PlaceSyncNode(MoveOrCreate operation,
                     const BookmarkNode* parent,
                     int index,
                     sync_api::WriteTransaction* trans,
                     sync_api::WriteNode* dst);

  // Copy properties (but not position) from |src| to |dst|.
  void UpdateSyncNodeProperties(const BookmarkNode* src,
                                sync_api::WriteNode* dst);

  // Helper function to encode a bookmark's favicon into a PNG byte vector.
  void EncodeFavicon(const BookmarkNode* src,
                     std::vector<unsigned char>* dst) const;

  // Remove the sync node corresponding to |node|.  It shouldn't have
  // any children.
  void RemoveOneSyncNode(sync_api::WriteTransaction* trans,
                         const BookmarkNode* node);

  // Remove all the sync nodes associated with |node| and its children.
  void RemoveSyncNodeHierarchy(const BookmarkNode* node);

  // Whether the sync merge warning should be shown.
  bool MergeAndSyncAcceptanceNeeded() const;

  // Sets the last synced time to the current time.
  void UpdateLastSyncedTime();

  // The profile whose data we are synchronizing.
  Profile* profile_;

  // TODO(ncarter): Put this in a profile, once there is UI for it.
  // This specifies where to find the sync server.
  GURL sync_service_url_;

  // Model assocation manager instance.
  scoped_refptr<browser_sync::ModelAssociator> model_associator_;

  // The last time we detected a successful transition from SYNCING state.
  // Our backend notifies us whenever we should take a new snapshot.
  base::Time last_synced_time_;

  // Our asynchronous backend to communicate with sync components living on
  // other threads.
  scoped_ptr<browser_sync::SyncBackendHost> backend_;

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

  // True only after all bootstrapping has succeeded: the bookmark model is
  // loaded, the sync backend is initialized, and the two domains are
  // consistent with one another.
  bool ready_to_process_changes_;

  // True if an unrecoverable error (e.g. violation of an assumed invariant)
  // occurred during syncer operation.  This value should be checked before
  // doing any work that might corrupt things further.
  bool unrecoverable_error_detected_;

  SyncSetupWizard wizard_;

  ObserverList<Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(ProfileSyncService);
};

#endif  // CHROME_BROWSER_SYNC_PROFILE_SYNC_SERVICE_H_

#endif  // CHROME_PERSONALIZATION
