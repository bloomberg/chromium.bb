// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_SESSIONS_SESSIONS_SYNC_MANAGER_H_
#define CHROME_BROWSER_SYNC_SESSIONS_SESSIONS_SYNC_MANAGER_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/sessions/session_id.h"
#include "chrome/browser/sessions/session_types.h"
#include "chrome/browser/sync/glue/device_info.h"
#include "chrome/browser/sync/glue/favicon_cache.h"
#include "chrome/browser/sync/glue/synced_session.h"
#include "chrome/browser/sync/glue/synced_session_tracker.h"
#include "chrome/browser/sync/open_tabs_ui_delegate.h"
#include "chrome/browser/sync/sessions/tab_node_pool.h"
#include "components/sync_driver/sync_prefs.h"
#include "sync/api/syncable_service.h"

class Profile;

namespace syncer {
class SyncErrorFactory;
}

namespace sync_pb {
class SessionHeader;
class SessionSpecifics;
class SessionTab;
class SessionWindow;
class TabNavigation;
}  // namespace sync_pb

namespace browser_sync {

class DataTypeErrorHandler;
class LocalDeviceInfoProvider;
class SyncedTabDelegate;
class SyncedWindowDelegate;
class SyncedWindowDelegatesGetter;

// An interface defining the ways in which local open tab events can interact
// with session sync.  All local tab events flow to sync via this interface.
// In that way it is analogous to sync changes flowing to the local model
// via ProcessSyncChanges, just with a more granular breakdown.
class LocalSessionEventHandler {
 public:
  // A local navigation event took place that affects the synced session
  // for this instance of Chrome.
  virtual void OnLocalTabModified(SyncedTabDelegate* modified_tab) = 0;

  // A local navigation occurred that triggered updates to favicon data for
  // each URL in |updated_page_urls|.  This is routed through Sessions Sync so
  // that we can filter (exclude) favicon updates for pages that aren't
  // currently part of the set of local open tabs, and pass relevant updates
  // on to FaviconCache for out-of-band favicon syncing.
  virtual void OnFaviconPageUrlsUpdated(
      const std::set<GURL>& updated_page_urls) = 0;
};

// The LocalSessionEventRouter is responsible for hooking itself up to various
// notification sources in the browser process and forwarding relevant
// events to a handler as defined in the LocalSessionEventHandler contract.
class LocalSessionEventRouter {
 public:
  virtual ~LocalSessionEventRouter();
  virtual void StartRoutingTo(LocalSessionEventHandler* handler) = 0;
  virtual void Stop() = 0;
};

// Contains all logic for associating the Chrome sessions model and
// the sync sessions model.
class SessionsSyncManager : public syncer::SyncableService,
                            public OpenTabsUIDelegate,
                            public LocalSessionEventHandler {
 public:
  SessionsSyncManager(Profile* profile,
                      LocalDeviceInfoProvider* local_device,
                      scoped_ptr<LocalSessionEventRouter> router);
  virtual ~SessionsSyncManager();

  // syncer::SyncableService implementation.
  virtual syncer::SyncMergeResult MergeDataAndStartSyncing(
      syncer::ModelType type,
      const syncer::SyncDataList& initial_sync_data,
      scoped_ptr<syncer::SyncChangeProcessor> sync_processor,
      scoped_ptr<syncer::SyncErrorFactory> error_handler) OVERRIDE;
  virtual void StopSyncing(syncer::ModelType type) OVERRIDE;
  virtual syncer::SyncDataList GetAllSyncData(
      syncer::ModelType type) const OVERRIDE;
  virtual syncer::SyncError ProcessSyncChanges(
      const tracked_objects::Location& from_here,
      const syncer::SyncChangeList& change_list) OVERRIDE;

  // OpenTabsUIDelegate implementation.
  virtual bool GetSyncedFaviconForPageURL(
      const std::string& pageurl,
      scoped_refptr<base::RefCountedMemory>* favicon_png) const OVERRIDE;
  virtual bool GetAllForeignSessions(
      std::vector<const SyncedSession*>* sessions) OVERRIDE;
  virtual bool GetForeignSession(
      const std::string& tag,
      std::vector<const SessionWindow*>* windows) OVERRIDE;
  virtual bool GetForeignTab(const std::string& tag,
                             const SessionID::id_type tab_id,
                             const SessionTab** tab) OVERRIDE;
  virtual void DeleteForeignSession(const std::string& tag) OVERRIDE;
  virtual bool GetLocalSession(const SyncedSession* * local_session) OVERRIDE;

  // LocalSessionEventHandler implementation.
  virtual void OnLocalTabModified(SyncedTabDelegate* modified_tab) OVERRIDE;
  virtual void OnFaviconPageUrlsUpdated(
      const std::set<GURL>& updated_favicon_page_urls) OVERRIDE;

  // Returns the tag used to uniquely identify this machine's session in the
  // sync model.
  const std::string& current_machine_tag() const {
    DCHECK(!current_machine_tag_.empty());
    return current_machine_tag_;
  }

  // Return the virtual URL of the current tab, even if it's pending.
  static GURL GetCurrentVirtualURL(const SyncedTabDelegate& tab_delegate);

  // Return the favicon url of the current tab, even if it's pending.
  static GURL GetCurrentFaviconURL(const SyncedTabDelegate& tab_delegate);

  FaviconCache* GetFaviconCache();

  SyncedWindowDelegatesGetter* GetSyncedWindowDelegatesGetter() const;

  // Triggers garbage collection of stale sessions (as defined by
  // |stale_session_threshold_days_|). This is called automatically every
  // time we start up (via AssociateModels) and when new sessions data is
  // downloaded (sync cycles complete).
  void DoGarbageCollection();

 private:
  // Keep all the links to local tab data in one place. A tab_node_id and tab
  // must be passed at creation. The tab_node_id is not mutable, although
  // all other fields are.
  class TabLink {
   public:
    TabLink(int tab_node_id, const SyncedTabDelegate* tab)
      : tab_node_id_(tab_node_id),
        tab_(tab) {}

    void set_tab(const SyncedTabDelegate* tab) { tab_ = tab; }
    void set_url(const GURL& url) { url_ = url; }

    int tab_node_id() const { return tab_node_id_; }
    const SyncedTabDelegate* tab() const { return tab_; }
    const GURL& url() const { return url_; }

   private:
    // The id for the sync node this tab is stored in.
    const int tab_node_id_;

    // The tab object itself.
    const SyncedTabDelegate* tab_;

    // The currently visible url of the tab (used for syncing favicons).
    GURL url_;

    DISALLOW_COPY_AND_ASSIGN(TabLink);
  };

  // Container for accessing local tab data by tab id.
  typedef std::map<SessionID::id_type, linked_ptr<TabLink> > TabLinksMap;

  FRIEND_TEST_ALL_PREFIXES(SessionsSyncManagerTest, PopulateSessionHeader);
  FRIEND_TEST_ALL_PREFIXES(SessionsSyncManagerTest, PopulateSessionWindow);
  FRIEND_TEST_ALL_PREFIXES(SessionsSyncManagerTest, ValidTabs);
  FRIEND_TEST_ALL_PREFIXES(SessionsSyncManagerTest, SetSessionTabFromDelegate);
  FRIEND_TEST_ALL_PREFIXES(SessionsSyncManagerTest,
                           SetSessionTabFromDelegateNavigationIndex);
  FRIEND_TEST_ALL_PREFIXES(SessionsSyncManagerTest,
                           SetSessionTabFromDelegateCurrentInvalid);
  FRIEND_TEST_ALL_PREFIXES(SessionsSyncManagerTest, BlockedNavigations);
  FRIEND_TEST_ALL_PREFIXES(SessionsSyncManagerTest, DeleteForeignSession);
  FRIEND_TEST_ALL_PREFIXES(SessionsSyncManagerTest,
                           SaveUnassociatedNodesForReassociation);
  FRIEND_TEST_ALL_PREFIXES(SessionsSyncManagerTest, MergeDeletesCorruptNode);
  FRIEND_TEST_ALL_PREFIXES(SessionsSyncManagerTest,
                           MergeLocalSessionExistingTabs);
  FRIEND_TEST_ALL_PREFIXES(SessionsSyncManagerTest,
                           CheckPrerenderedWebContentsSwap);
  FRIEND_TEST_ALL_PREFIXES(SessionsSyncManagerTest,
                           AssociateWindowsDontReloadTabs);
  FRIEND_TEST_ALL_PREFIXES(SessionsSyncManagerTest,
                           SwappedOutOnRestore);
  FRIEND_TEST_ALL_PREFIXES(SessionsSyncManagerTest,
                           ProcessRemoteDeleteOfLocalSession);

  void InitializeCurrentMachineTag();

  // Load and add window or tab data for a foreign session to our internal
  // tracking.
  void UpdateTrackerWithForeignSession(
      const sync_pb::SessionSpecifics& specifics,
      const base::Time& modification_time);

  // Returns true if |sync_data| contained a header node for the current
  // machine, false otherwise. |restored_tabs| is a filtered tab-only
  // subset of |sync_data| returned by this function for convenience.
  // |new_changes| is a link to the SyncChange pipeline that exists in the
  // caller's context. This function will append necessary changes for
  // processing later.
  bool InitFromSyncModel(const syncer::SyncDataList& sync_data,
                         syncer::SyncDataList* restored_tabs,
                         syncer::SyncChangeList* new_changes);

  // Helper to construct a deletion SyncChange for a *tab node*.
  // Caller should check IsValid() on the returned change, as it's possible
  // this node could not be deleted.
  syncer::SyncChange TombstoneTab(const sync_pb::SessionSpecifics& tab);

  // Helper method to load the favicon data from the tab specifics. If the
  // favicon is valid, stores the favicon data into the favicon cache.
  void RefreshFaviconVisitTimesFromForeignTab(
      const sync_pb::SessionTab& tab, const base::Time& modification_time);

  // Removes a foreign session from our internal bookkeeping.
  // Returns true if the session was found and deleted, false if no data was
  // found for that session.  This will *NOT* trigger sync deletions. See
  // DeleteForeignSession below.
  bool DisassociateForeignSession(const std::string& foreign_session_tag);

  // Delete a foreign session and all its sync data.
  // |change_output| *must* be provided as a link to the SyncChange pipeline
  // that exists in the caller's context. This function will append necessary
  // changes for processing later.
  void DeleteForeignSessionInternal(const std::string& tag,
                                    syncer::SyncChangeList* change_output);

  // Used to populate a session header from the session specifics header
  // provided.
  static void PopulateSessionHeaderFromSpecifics(
      const sync_pb::SessionHeader& header_specifics,
      base::Time mtime,
      SyncedSession* session_header);

  // Builds |session_window| from the session specifics window
  // provided and updates the SessionTracker with foreign session data created.
  void BuildSyncedSessionFromSpecifics(
      const std::string& session_tag,
      const sync_pb::SessionWindow& specifics,
      base::Time mtime,
      SessionWindow* session_window);

  // Resync local window information. Updates the local sessions header node
  // with the status of open windows and the order of tabs they contain. Should
  // only be called for changes that affect a window, not a change within a
  // single tab.
  //
  // RELOAD_TABS will additionally cause a resync of all tabs (same as calling
  // AssociateTabs with a vector of all tabs).
  //
  // |restored_tabs| is a filtered tab-only subset of initial sync data, if
  // available (during MergeDataAndStartSyncing). It can be used to obtain
  // baseline SessionSpecifics for tabs we can't fully associate any other
  // way because they don't yet have a WebContents.
  //
  // Returns: false if the local session's sync nodes were deleted and
  // reassociation is necessary, true otherwise.
  //
  // |change_output| *must* be provided as a link to the SyncChange pipeline
  // that exists in the caller's context. This function will append necessary
  // changes for processing later.
  enum ReloadTabsOption {
    RELOAD_TABS,
    DONT_RELOAD_TABS
  };
  void AssociateWindows(ReloadTabsOption option,
                        const syncer::SyncDataList& restored_tabs,
                        syncer::SyncChangeList* change_output);

  // Loads and reassociates the local tabs referenced in |tabs|.
  // |change_output| *must* be provided as a link to the SyncChange pipeline
  // that exists in the caller's context. This function will append necessary
  // changes for processing later.
  void AssociateTab(SyncedTabDelegate* const tab,
                    syncer::SyncChangeList* change_output);

  // Set |session_tab| from |tab_delegate| and |mtime|.
  static void SetSessionTabFromDelegate(
      const SyncedTabDelegate& tab_delegate,
      base::Time mtime,
      SessionTab* session_tab);

  // Populates |specifics| based on the data in |tab_delegate|.
  void LocalTabDelegateToSpecifics(const SyncedTabDelegate& tab_delegate,
                                   sync_pb::SessionSpecifics* specifics);

  // It's possible that when we associate windows, tabs aren't all loaded
  // into memory yet (e.g on android) and we don't have a WebContents. In this
  // case we can't do a full association, but we still want to update tab IDs
  // as they may have changed after a session was restored.  This method
  // compares new_tab_id against the previously persisted tab ID (from
  // our TabNodePool) and updates it if it differs.
  // |restored_tabs| is a filtered tab-only subset of initial sync data, if
  // available (during MergeDataAndStartSyncing). It can be used to obtain
  // baseline SessionSpecifics for tabs we can't fully associate any other
  // way because they don't yet have a WebContents.
  // TODO(tim): Bug 98892. We should be able to test this for this on android
  // even though we didn't have tests for old API-based sessions sync.
  void AssociateRestoredPlaceholderTab(
      const SyncedTabDelegate& tab_delegate,
      SessionID::id_type new_tab_id,
      const syncer::SyncDataList& restored_tabs,
      syncer::SyncChangeList* change_output);

  // Stops and re-starts syncing to rebuild association mappings.
  // See |local_tab_pool_out_of_sync_|.
  void RebuildAssociations();

  // Mapping of current open (local) tabs to their sync identifiers.
  TabLinksMap local_tab_map_;

  SyncedSessionTracker session_tracker_;
  FaviconCache favicon_cache_;

  // Pool of used/available sync nodes associated with local tabs.
  TabNodePool local_tab_pool_;

  // Tracks whether our local representation of which sync nodes map to what
  // tabs (belonging to the current local session) is inconsistent.  This can
  // happen if a foreign client deems our session as "stale" and decides to
  // delete it. Rather than respond by bullishly re-creating our nodes
  // immediately, which could lead to ping-pong sequences, we give the benefit
  // of the doubt and hold off until another local navigation occurs, which
  // proves that we are still relevant.
  bool local_tab_pool_out_of_sync_;

  sync_driver::SyncPrefs sync_prefs_;

  const Profile* const profile_;

  scoped_ptr<syncer::SyncErrorFactory> error_handler_;
  scoped_ptr<syncer::SyncChangeProcessor> sync_processor_;

  // Local device info provider, owned by ProfileSyncService.
  const LocalDeviceInfoProvider* const local_device_;

  // Unique client tag.
  std::string current_machine_tag_;

  // User-visible machine name.
  std::string current_session_name_;

  // SyncID for the sync node containing all the window information for this
  // client.
  int local_session_header_node_id_;

  // Number of days without activity after which we consider a session to be
  // stale and a candidate for garbage collection.
  size_t stale_session_threshold_days_;

  scoped_ptr<LocalSessionEventRouter> local_event_router_;
  scoped_ptr<SyncedWindowDelegatesGetter> synced_window_getter_;

  DISALLOW_COPY_AND_ASSIGN(SessionsSyncManager);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_SESSIONS_SESSIONS_SYNC_MANAGER_H_
