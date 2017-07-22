// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SESSIONS_SESSIONS_SYNC_MANAGER_H_
#define COMPONENTS_SYNC_SESSIONS_SESSIONS_SYNC_MANAGER_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/sessions/core/session_id.h"
#include "components/sessions/core/session_types.h"
#include "components/sync/base/sync_prefs.h"
#include "components/sync/device_info/device_info.h"
#include "components/sync/model/syncable_service.h"
#include "components/sync/user_events/global_id_mapper.h"
#include "components/sync_sessions/favicon_cache.h"
#include "components/sync_sessions/local_session_event_router.h"
#include "components/sync_sessions/lost_navigations_recorder.h"
#include "components/sync_sessions/open_tabs_ui_delegate.h"
#include "components/sync_sessions/revisit/page_revisit_broadcaster.h"
#include "components/sync_sessions/synced_session.h"
#include "components/sync_sessions/synced_session_tracker.h"
#include "components/sync_sessions/task_tracker.h"

namespace syncer {
class LocalDeviceInfoProvider;
class SyncErrorFactory;
class SyncPrefs;
}  // namespace syncer

namespace sync_pb {
class SessionHeader;
class SessionSpecifics;
class SessionTab;
class SessionWindow;
}  // namespace sync_pb

namespace extensions {
class ExtensionSessionsTest;
}  // namespace extensions

namespace sync_sessions {

class SyncedTabDelegate;
class SyncedWindowDelegatesGetter;

// Contains all logic for associating the Chrome sessions model and
// the sync sessions model.
class SessionsSyncManager : public syncer::SyncableService,
                            public OpenTabsUIDelegate,
                            public LocalSessionEventHandler,
                            public syncer::GlobalIdMapper {
 public:
  SessionsSyncManager(SyncSessionsClient* sessions_client,
                      syncer::SyncPrefs* sync_prefs,
                      syncer::LocalDeviceInfoProvider* local_device,
                      LocalSessionEventRouter* router,
                      const base::Closure& sessions_updated_callback,
                      const base::Closure& datatype_refresh_callback);
  ~SessionsSyncManager() override;

  // syncer::SyncableService implementation.
  syncer::SyncMergeResult MergeDataAndStartSyncing(
      syncer::ModelType type,
      const syncer::SyncDataList& initial_sync_data,
      std::unique_ptr<syncer::SyncChangeProcessor> sync_processor,
      std::unique_ptr<syncer::SyncErrorFactory> error_handler) override;
  void StopSyncing(syncer::ModelType type) override;
  syncer::SyncDataList GetAllSyncData(syncer::ModelType type) const override;
  syncer::SyncError ProcessSyncChanges(
      const tracked_objects::Location& from_here,
      const syncer::SyncChangeList& change_list) override;

  // OpenTabsUIDelegate implementation.
  bool GetSyncedFaviconForPageURL(
      const std::string& pageurl,
      scoped_refptr<base::RefCountedMemory>* favicon_png) const override;
  bool GetAllForeignSessions(
      std::vector<const SyncedSession*>* sessions) override;
  bool GetForeignSession(
      const std::string& tag,
      std::vector<const sessions::SessionWindow*>* windows) override;
  bool GetForeignTab(const std::string& tag,
                     SessionID::id_type tab_id,
                     const sessions::SessionTab** tab) override;
  bool GetForeignSessionTabs(
      const std::string& tag,
      std::vector<const sessions::SessionTab*>* tabs) override;
  void DeleteForeignSession(const std::string& tag) override;
  bool GetLocalSession(const SyncedSession** local_session) override;

  // LocalSessionEventHandler implementation.
  void OnLocalTabModified(SyncedTabDelegate* modified_tab) override;
  void OnFaviconsChanged(const std::set<GURL>& page_urls,
                         const GURL& icon_url) override;

  // Returns the tag used to uniquely identify this machine's session in the
  // sync model.
  const std::string& current_machine_tag() const {
    DCHECK(!current_machine_tag_.empty());
    return current_machine_tag_;
  }

  FaviconCache* GetFaviconCache();

  // Triggers garbage collection of stale sessions (as defined by
  // |stale_session_threshold_days_|). This is called every time we see new
  // sessions data downloaded (sync cycles complete).
  void DoGarbageCollection();

  // GlobalIdMapper implementation.
  void AddGlobalIdChangeObserver(syncer::GlobalIdChange callback) override;
  int64_t GetLatestGlobalId(int64_t global_id) override;

 private:
  friend class extensions::ExtensionSessionsTest;
  friend class SessionsSyncManagerTest;
  FRIEND_TEST_ALL_PREFIXES(SessionsSyncManagerTest, PopulateSyncedSession);
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
                           ProcessForeignDeleteTabsWithShadowing);
  FRIEND_TEST_ALL_PREFIXES(SessionsSyncManagerTest,
                           ProcessForeignDeleteTabsWithReusedNodeIds);
  FRIEND_TEST_ALL_PREFIXES(SessionsSyncManagerTest,
                           SaveUnassociatedNodesForReassociation);
  FRIEND_TEST_ALL_PREFIXES(SessionsSyncManagerTest, MergeDeletesCorruptNode);
  FRIEND_TEST_ALL_PREFIXES(SessionsSyncManagerTest, MergeDeletesBadHash);
  FRIEND_TEST_ALL_PREFIXES(SessionsSyncManagerTest,
                           MergeLocalSessionExistingTabs);
  FRIEND_TEST_ALL_PREFIXES(SessionsSyncManagerTest,
                           CheckPrerenderedWebContentsSwap);
  FRIEND_TEST_ALL_PREFIXES(SessionsSyncManagerTest,
                           AssociateWindowsDontReloadTabs);
  FRIEND_TEST_ALL_PREFIXES(SessionsSyncManagerTest, SwappedOutOnRestore);
  FRIEND_TEST_ALL_PREFIXES(SessionsSyncManagerTest,
                           ProcessRemoteDeleteOfLocalSession);

  void InitializeCurrentMachineTag(const std::string& cache_guid);

  // Load and add window or tab data from synced specifics to our internal
  // tracking.
  void UpdateTrackerWithSpecifics(const sync_pb::SessionSpecifics& specifics,
                                  const base::Time& modification_time);

  // Returns true if |sync_data| contained a header node for the current
  // machine, false otherwise. |new_changes| is a link to the SyncChange
  // pipeline that exists in the caller's context. This function will append
  // necessary changes for processing later.
  bool InitFromSyncModel(const syncer::SyncDataList& sync_data,
                         syncer::SyncChangeList* new_changes);

  // Helper to construct a deletion SyncChange for a *tab node*.
  // Caller should check IsValid() on the returned change, as it's possible
  // this node could not be deleted.
  syncer::SyncChange TombstoneTab(const sync_pb::SessionSpecifics& tab);

  // Helper method to load the favicon data from the tab specifics. If the
  // favicon is valid, stores the favicon data into the favicon cache.
  void RefreshFaviconVisitTimesFromForeignTab(
      const sync_pb::SessionTab& tab,
      const base::Time& modification_time);

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
  void PopulateSyncedSessionFromSpecifics(
      const std::string& session_tag,
      const sync_pb::SessionHeader& header_specifics,
      base::Time mtime,
      SyncedSession* synced_session);

  // Builds |synced_session_window| from the session specifics window
  // provided and updates the SessionTracker with foreign session data created.
  void PopulateSyncedSessionWindowFromSpecifics(
      const std::string& session_tag,
      const sync_pb::SessionWindow& specifics,
      base::Time mtime,
      SyncedSessionWindow* synced_session_window);

  // Resync local window information. Updates the local sessions header node
  // with the status of open windows and the order of tabs they contain. Should
  // only be called for changes that affect a window, not a change within a
  // single tab.
  //
  // RELOAD_TABS will additionally cause a resync of all tabs (same as calling
  // AssociateTabs with a vector of all tabs).
  //
  // Returns: false if the local session's sync nodes were deleted and
  // reassociation is necessary, true otherwise.
  //
  // |change_output| *must* be provided as a link to the SyncChange pipeline
  // that exists in the caller's context. This function will append necessary
  // changes for processing later.
  enum ReloadTabsOption { RELOAD_TABS, DONT_RELOAD_TABS };
  void AssociateWindows(ReloadTabsOption option,
                        syncer::SyncChangeList* change_output);

  // Loads and reassociates the local tabs referenced in |tabs|.
  // |change_output| *must* be provided as a link to the SyncChange pipeline
  // that exists in the caller's context. This function will append necessary
  // changes for processing later.
  void AssociateTab(SyncedTabDelegate* const tab,
                    syncer::SyncChangeList* change_output);

  // Set |session_tab| from |tab_delegate| and |mtime|.
  void SetSessionTabFromDelegate(const SyncedTabDelegate& tab_delegate,
                                 base::Time mtime,
                                 sessions::SessionTab* session_tab);

  // Populates |specifics| based on the data in |tab_delegate|.
  void LocalTabDelegateToSpecifics(const SyncedTabDelegate& tab_delegate,
                                   sync_pb::SessionSpecifics* specifics);

  // Updates task tracker with the navigations of |tab_delegate|.
  void UpdateTaskTracker(SyncedTabDelegate* const tab_delegate);

  // Update |tab_specifics| with the corresponding task ids.
  void WriteTasksIntoSpecifics(sync_pb::SessionTab* tab_specifics);

  // It's possible that when we associate windows, tabs aren't all loaded
  // into memory yet (e.g on android) and we don't have a WebContents. In this
  // case we can't do a full association, but we still want to update tab IDs
  // as they may have changed after a session was restored.  This method
  // compares new_tab_id and new_window_id against the previously persisted tab
  // ID and window ID (from our TabNodePool) and updates them if either differs.
  void AssociateRestoredPlaceholderTab(
      const SyncedTabDelegate& tab_delegate,
      SessionID::id_type new_tab_id,
      SessionID::id_type new_window_id,
      syncer::SyncChangeList* change_output);

  // Appends an ACTION_UPDATE for a sync tab entity onto |change_output| to
  // reflect the contents of |tab|, given the tab node id |sync_id|.
  void AppendChangeForExistingTab(int sync_id,
                                  const sessions::SessionTab& tab,
                                  syncer::SyncChangeList* change_output);

  // Stops and re-starts syncing to rebuild association mappings. Returns true
  // when re-starting succeeds.
  // See |local_tab_pool_out_of_sync_|.
  bool RebuildAssociations();

  // Validates the content of a SessionHeader protobuf.
  // Returns false if validation fails.
  static bool IsValidSessionHeader(const sync_pb::SessionHeader& header);

  // Calculates the tag hash from a specifics object. Calculating the hash is
  // something we typically want to avoid doing in the model type like this.
  // However, the only place that understands how to generate a tag from the
  // specifics is the model type, ie us. We need to generate the tag because it
  // is not passed over the wire for remote data. The use case this function was
  // created for is detecting bad tag hashes from remote data, see
  // crbug.com/604657.
  static std::string TagHashFromSpecifics(
      const sync_pb::SessionSpecifics& specifics);

  SyncedWindowDelegatesGetter* synced_window_delegates_getter() const;

  void TrackNavigationIds(const sessions::SerializedNavigationEntry& current);

  void CleanupNavigationTracking();

  // The client of this sync sessions datatype.
  SyncSessionsClient* const sessions_client_;

  SyncedSessionTracker session_tracker_;
  FaviconCache favicon_cache_;

  // Tracks whether our local representation of which sync nodes map to what
  // tabs (belonging to the current local session) is inconsistent.  This can
  // happen if a foreign client deems our session as "stale" and decides to
  // delete it. Rather than respond by bullishly re-creating our nodes
  // immediately, which could lead to ping-pong sequences, we give the benefit
  // of the doubt and hold off until another local navigation occurs, which
  // proves that we are still relevant.
  bool local_tab_pool_out_of_sync_;

  syncer::SyncPrefs* sync_prefs_;

  std::unique_ptr<syncer::SyncErrorFactory> error_handler_;
  std::unique_ptr<syncer::SyncChangeProcessor> sync_processor_;

  // Local device info provider, owned by ProfileSyncService.
  const syncer::LocalDeviceInfoProvider* const local_device_;

  // Unique client tag.
  std::string current_machine_tag_;

  // User-visible machine name and device type to populate header.
  std::string current_session_name_;
  sync_pb::SyncEnums::DeviceType current_device_type_;

  // SyncID for the sync node containing all the window information for this
  // client.
  int local_session_header_node_id_;

  // Number of days without activity after which we consider a session to be
  // stale and a candidate for garbage collection.
  int stale_session_threshold_days_;

  LocalSessionEventRouter* local_event_router_;

  // Owns revisiting instrumentation logic for page visit events.
  PageRevisitBroadcaster page_revisit_broadcaster_;

  std::unique_ptr<sync_sessions::LostNavigationsRecorder>
      lost_navigations_recorder_;

  // Callback to inform interested observer that new sessions data has arrived.
  base::Closure sessions_updated_callback_;

  // Callback to inform sync that a sync data refresh is requested.
  base::Closure datatype_refresh_callback_;

  // Tracks Chrome Tasks, which associates navigations, with tab and navigation
  // changes of current session.
  std::unique_ptr<TaskTracker> task_tracker_;

  // Used to track global_ids that should be used when referencing various
  // pieces of sessions data, and notify observer when things have changed.
  std::map<int64_t, int> global_to_unique_;
  std::map<int, int64_t> unique_to_current_global_;
  std::vector<syncer::GlobalIdChange> global_id_change_observers_;

  DISALLOW_COPY_AND_ASSIGN(SessionsSyncManager);
};

}  // namespace sync_sessions

#endif  // COMPONENTS_SYNC_SESSIONS_SESSIONS_SYNC_MANAGER_H_
