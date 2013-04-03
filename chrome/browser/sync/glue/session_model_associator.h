// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_SESSION_MODEL_ASSOCIATOR_H_
#define CHROME_BROWSER_SYNC_GLUE_SESSION_MODEL_ASSOCIATOR_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "base/time.h"
#include "chrome/browser/sessions/session_id.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sessions/session_types.h"
#include "chrome/browser/sync/glue/favicon_cache.h"
#include "chrome/browser/sync/glue/model_associator.h"
#include "chrome/browser/sync/glue/synced_session_tracker.h"
#include "chrome/browser/sync/glue/tab_node_pool.h"
#include "sync/internal_api/public/base/model_type.h"

class PrefServiceSyncable;
class Profile;
class ProfileSyncService;

namespace syncer {
class ReadNode;
class WriteTransaction;
}  // namespace syncer

namespace sync_pb {
class SessionHeader;
class SessionSpecifics;
class SessionTab;
class SessionWindow;
class TabNavigation;
}  // namespace sync_pb

namespace browser_sync {

class DataTypeErrorHandler;
class SyncedTabDelegate;
class SyncedWindowDelegate;

// Contains all logic for associating the Chrome sessions model and
// the sync sessions model.
class SessionModelAssociator
    : public PerDataTypeAssociatorInterface<size_t, size_t>,
      public base::SupportsWeakPtr<SessionModelAssociator>,
      public FaviconCacheObserver,
      public base::NonThreadSafe {
 public:
  // Does not take ownership of sync_service.
  SessionModelAssociator(ProfileSyncService* sync_service,
                         DataTypeErrorHandler* error_handler);
  SessionModelAssociator(ProfileSyncService* sync_service,
                         bool setup_for_test);
  virtual ~SessionModelAssociator();

  // The has_nodes out parameter is set to true if the sync model has
  // nodes other than the permanent tagged nodes.  The method may
  // return false if an error occurred.
  virtual bool SyncModelHasUserCreatedNodes(bool* has_nodes) OVERRIDE;

  // AssociatorInterface and PerDataTypeAssociator Interface implementation.
  virtual void AbortAssociation() OVERRIDE {
    // No implementation needed, this associator runs on the main thread.
  }

  // See ModelAssociator interface.
  virtual bool CryptoReadyIfNecessary() OVERRIDE;

  // Returns sync id for the given chrome model id.
  // Returns syncer::kInvalidId if the sync node is not found for the given
  // chrome id.
  virtual int64 GetSyncIdFromChromeId(const size_t& id) OVERRIDE;

  // Returns sync id for the given session tag.
  // Returns syncer::kInvalidId if the sync node is not found for the given
  // tag
  virtual int64 GetSyncIdFromSessionTag(const std::string& tag);

  // Not used.
  virtual const size_t* GetChromeNodeFromSyncId(int64 sync_id)
      OVERRIDE;

  // Not used.
  virtual bool InitSyncNodeFromChromeId(const size_t& id,
                                        syncer::BaseNode* sync_node) OVERRIDE;

  // Not used.
  virtual void Associate(const size_t* tab, int64 sync_id) OVERRIDE;

  // Not used.
  virtual void Disassociate(int64 sync_id) OVERRIDE;

  // FaviconCacheObserver interface.
  virtual void OnFaviconUpdated(const GURL& page_url,
                                const GURL& icon_url) OVERRIDE;

  // Resync local window information. Updates the local sessions header node
  // with the status of open windows and the order of tabs they contain. Should
  // only be called for changes that affect a window, not a change within a
  // single tab.
  //
  // If |reload_tabs| is true, will also resync all tabs (same as calling
  // AssociateTabs with a vector of all tabs).
  // |error| gets set if any association error occurred.
  // Returns: false if the local session's sync nodes were deleted and
  // reassociation is necessary, true otherwise.
  bool AssociateWindows(bool reload_tabs, syncer::SyncError* error);

  // Loads and reassociates the local tabs referenced in |tabs|.
  // |error| gets set if any association error occurred.
  // Returns: false if the local session's sync nodes were deleted and
  // reassociation is necessary, true otherwise.
  bool AssociateTabs(const std::vector<SyncedTabDelegate*>& tabs,
                     syncer::SyncError* error);

  // Reassociates a single tab with the sync model. Will check if the tab
  // already is associated with a sync node and allocate one if necessary.
  // |error| gets set if any association error occurred.
  // Returns: false if the local session's sync nodes were deleted and
  // reassociation is necessary, true otherwise.
  bool AssociateTab(const SyncedTabDelegate& tab,
                    syncer::SyncError* error);

  // Load any foreign session info stored in sync db and update the sync db
  // with local client data. Processes/reuses any sync nodes owned by this
  // client and creates any further sync nodes needed to store local header and
  // tab info.
  virtual syncer::SyncError AssociateModels(
      syncer::SyncMergeResult* local_merge_result,
      syncer::SyncMergeResult* syncer_merge_result) OVERRIDE;

  // Initializes the given sync node from the given chrome node id.
  // Returns false if no sync node was found for the given chrome node id or
  // if the initialization of sync node fails.
  virtual bool InitSyncNodeFromChromeId(const std::string& id,
                                        syncer::BaseNode* sync_node);

  // Clear local sync data buffers. Does not delete sync nodes to avoid
  // tombstones. TODO(zea): way to eventually delete orphaned nodes.
  virtual syncer::SyncError DisassociateModels() OVERRIDE;

  // Returns the tag used to uniquely identify this machine's session in the
  // sync model.
  const std::string& GetCurrentMachineTag() const {
    DCHECK(!current_machine_tag_.empty());
    return current_machine_tag_;
  }

  // Load and associate window and tab data for a foreign session.
  void AssociateForeignSpecifics(const sync_pb::SessionSpecifics& specifics,
                                 const base::Time& modification_time);

  // Removes a foreign session from our internal bookkeeping.
  // Returns true if the session was found and deleted, false if no data was
  // found for that session.
  bool DisassociateForeignSession(const std::string& foreign_session_tag);

  // Attempts to asynchronously refresh the sessions sync data. If new data is
  // received, the FOREIGN_SESSIONS_UPDATED notification is sent. No
  // notification will be sent otherwise. This method is not guaranteed to
  // trigger a sync cycle.
  void AttemptSessionsDataRefresh() const;

  // Sets |*local_session| to point to the associator's representation of the
  // local machine. Used primarily for testing.
  bool GetLocalSession(const SyncedSession* * local_session);

  // Builds a list of all foreign sessions. Caller does NOT own SyncedSession
  // objects.
  // Returns true if foreign sessions were found, false otherwise.
  bool GetAllForeignSessions(std::vector<const SyncedSession*>* sessions);

  // Loads all windows for foreign session with session tag |tag|. Caller does
  // NOT own SyncedSession objects.
  // Returns true if the foreign session was found, false otherwise.
  bool GetForeignSession(const std::string& tag,
                         std::vector<const SessionWindow*>* windows);

  // Looks up the foreign tab identified by |tab_id| and belonging to foreign
  // session |tag|. Caller does NOT own the SessionTab object.
  // Returns true if the foreign session and tab were found, false otherwise.
  bool GetForeignTab(const std::string& tag,
                     const SessionID::id_type tab_id,
                     const SessionTab** tab);

  // Triggers garbage collection of stale sessions (as defined by
  // |stale_session_threshold_days_|). This is called automatically every
  // time we start up (via AssociateModels).
  void DeleteStaleSessions();

  // Set the threshold of inactivity (in days) at which we consider sessions
  // stale.
  void SetStaleSessionThreshold(size_t stale_session_threshold_days);

  // Delete a foreign session and all its sync data.
  void DeleteForeignSession(const std::string& tag);

  // Control which local tabs we're interested in syncing.
  // Ensures the profile matches sync's profile and that the tab has valid
  // entries.
  bool ShouldSyncTab(const SyncedTabDelegate& tab) const;

  // Compare |urls| against |tab_map_|'s urls to see if any tabs with
  // outstanding favicon loads can be fulfilled.
  void FaviconsUpdated(const std::set<GURL>& urls);

  // Returns the syncable model type.
  static syncer::ModelType model_type() { return syncer::SESSIONS; }

  // Testing only. Will cause the associator to call MessageLoop::Quit()
  // when a local change is made, or when timeout occurs, whichever is
  // first.
  void BlockUntilLocalChangeForTest(base::TimeDelta timeout);

  // If a valid favicon for the page at |url| is found, fills |favicon_png| with
  // the png-encoded image and returns true. Else, returns false.
  bool GetSyncedFaviconForPageURL(
      const std::string& pageurl,
      scoped_refptr<base::RefCountedMemory>* favicon_png) const;

  void SetCurrentMachineTagForTesting(const std::string& machine_tag) {
    current_machine_tag_ = machine_tag;
  }

  FaviconCache* GetFaviconCache();

 private:
  friend class SyncSessionModelAssociatorTest;
  FRIEND_TEST_ALL_PREFIXES(ProfileSyncServiceSessionTest, WriteSessionToNode);
  FRIEND_TEST_ALL_PREFIXES(ProfileSyncServiceSessionTest,
                           WriteFilledSessionToNode);
  FRIEND_TEST_ALL_PREFIXES(ProfileSyncServiceSessionTest,
                           WriteForeignSessionToNode);
  FRIEND_TEST_ALL_PREFIXES(ProfileSyncServiceSessionTest, TabNodePoolEmpty);
  FRIEND_TEST_ALL_PREFIXES(ProfileSyncServiceSessionTest, TabNodePoolNonEmpty);
  FRIEND_TEST_ALL_PREFIXES(ProfileSyncServiceSessionTest, ValidTabs);
  FRIEND_TEST_ALL_PREFIXES(ProfileSyncServiceSessionTest, ExistingTabs);
  FRIEND_TEST_ALL_PREFIXES(ProfileSyncServiceSessionTest, MissingLocalTabNode);
  FRIEND_TEST_ALL_PREFIXES(SyncSessionModelAssociatorTest,
                           PopulateSessionHeader);
  FRIEND_TEST_ALL_PREFIXES(SyncSessionModelAssociatorTest,
                           PopulateSessionWindow);
  FRIEND_TEST_ALL_PREFIXES(SyncSessionModelAssociatorTest, PopulateSessionTab);
  FRIEND_TEST_ALL_PREFIXES(SyncSessionModelAssociatorTest,
                           TabNodePool);

  // Keep all the links to local tab data in one place. A sync_id and tab must
  // be passed at creation. The sync_id is not mutable after, although all other
  // fields are.
  class TabLink {
   public:
    TabLink(int64 sync_id, const SyncedTabDelegate* tab)
      : sync_id_(sync_id),
        tab_(tab) {}

    void set_tab(const SyncedTabDelegate* tab) { tab_ = tab; }
    void set_url(const GURL& url) { url_ = url; }

    int64 sync_id() const { return sync_id_; }
    const SyncedTabDelegate* tab() const { return tab_; }
    const GURL& url() const { return url_; }

   private:
    DISALLOW_COPY_AND_ASSIGN(TabLink);

    // The id for the sync node this tab is stored in.
    const int64 sync_id_;

    // The tab object itself.
    const SyncedTabDelegate* tab_;

    // The currently visible url of the tab (used for syncing favicons).
    GURL url_;
  };

  // Container for accessing local tab data by tab id.
  typedef std::map<SessionID::id_type, linked_ptr<TabLink> > TabLinksMap;

  // Determine if a window is of a type we're interested in syncing.
  static bool ShouldSyncWindow(const SyncedWindowDelegate* window);

  // Initializes the tag corresponding to this machine.
  void InitializeCurrentMachineTag(syncer::WriteTransaction* trans);

  // Updates the server data based upon the current client session.  If no node
  // corresponding to this machine exists in the sync model, one is created.
  // Returns true on success, false if association failed.
  bool UpdateSyncModelDataFromClient(syncer::SyncError* error);

  // Pulls the current sync model from the sync database and returns true upon
  // update of the client model. Will associate any foreign sessions as well as
  // keep track of any local tab nodes, adding them to our free tab node pool.
  bool UpdateAssociationsFromSyncModel(const syncer::ReadNode& root,
                                       syncer::WriteTransaction* trans,
                                       syncer::SyncError* error);

  // Return the virtual URL of the current tab, even if it's pending.
  static GURL GetCurrentVirtualURL(const SyncedTabDelegate& tab_delegate);

  // Return the favicon url of the current tab, even if it's pending.
  static GURL GetCurrentFaviconURL(const SyncedTabDelegate& tab_delegate);

  // Fills a tab sync node with data from a WebContents object. Updates
  // |tab_link| with the current url if it's valid and triggers a favicon
  // load if the url has changed.
  // Returns true on success, false if we need to reassociate due to corruption.
  bool WriteTabContentsToSyncModel(TabLink* tab_link,
                                   syncer::SyncError* error);

  // Set |session_tab| from |tab_delegate| and |mtime|.
  static void SetSessionTabFromDelegate(
      const SyncedTabDelegate& tab_delegate,
      base::Time mtime,
      SessionTab* session_tab);

  // Used to populate a session header from the session specifics header
  // provided.
  static void PopulateSessionHeaderFromSpecifics(
    const sync_pb::SessionHeader& header_specifics,
    base::Time mtime,
    SyncedSession* session_header);

  // Used to populate a session window from the session specifics window
  // provided. Tracks any foreign session data created through |tracker|.
  static void PopulateSessionWindowFromSpecifics(
      const std::string& foreign_session_tag,
      const sync_pb::SessionWindow& window,
      base::Time mtime,
      SessionWindow* session_window,
      SyncedSessionTracker* tracker);

  // Helper method to load the favicon data from the tab specifics. If the
  // favicon is valid, stores the favicon data into the favicon cache.
  void LoadForeignTabFavicon(const sync_pb::SessionTab& tab);

  // Returns true if this tab belongs to this profile and belongs to a window,
  // false otherwise.
  bool IsValidTab(const SyncedTabDelegate& tab) const;

  // Having a valid entry is defined as the url being valid and and having a
  // syncable scheme (non chrome:// and file:// url's). In other words, we don't
  // want to sync a tab that is nothing but chrome:// and file:// navigations or
  // invalid url's.
  bool TabHasValidEntry(const SyncedTabDelegate& tab) const;

  // For testing only.
  void QuitLoopForSubtleTesting();

  // Unique client tag.
  std::string current_machine_tag_;

  // User-visible machine name.
  std::string current_session_name_;

  // Pool of all used/available sync nodes associated with tabs.
  TabNodePool tab_pool_;

  // SyncID for the sync node containing all the window information for this
  // client.
  int64 local_session_syncid_;

  // Mapping of current open (local) tabs to their sync identifiers.
  TabLinksMap tab_map_;

  SyncedSessionTracker synced_session_tracker_;

  // Weak pointer.
  ProfileSyncService* sync_service_;

  // Number of days without activity after which we consider a session to be
  // stale and a candidate for garbage collection.
  size_t stale_session_threshold_days_;

  // To avoid certain checks not applicable to tests.
  bool setup_for_test_;

  // During integration tests, we sometimes need to block until a local change
  // is made.
  bool waiting_for_change_;
  base::WeakPtrFactory<SessionModelAssociator> test_weak_factory_;

  // Profile being synced. Weak pointer.
  Profile* const profile_;

  DataTypeErrorHandler* error_handler_;

  // Our favicon cache.
  FaviconCache favicon_cache_;

  DISALLOW_COPY_AND_ASSIGN(SessionModelAssociator);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_SESSION_MODEL_ASSOCIATOR_H_
