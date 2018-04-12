// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SESSIONS_SYNCED_SESSION_TRACKER_H_
#define COMPONENTS_SYNC_SESSIONS_SYNCED_SESSION_TRACKER_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/macros.h"
#include "components/sessions/core/session_id.h"
#include "components/sessions/core/session_types.h"
#include "components/sync_sessions/synced_session.h"
#include "components/sync_sessions/tab_node_pool.h"

namespace sync_pb {
class SessionSpecifics;
}

namespace sync_sessions {

class SyncSessionsClient;

// Class to manage synced sessions. The tracker will own all SyncedSession
// and SyncedSessionTab objects it creates, and deletes them appropriately on
// destruction.
//
// Note: SyncedSession objects are created for all synced sessions, including
// the local session (whose tag we maintain separately).
class SyncedSessionTracker {
 public:
  // Different ways to lookup/filter tabs.
  enum SessionLookup {
    RAW,         // Return all foreign sessions.
    PRESENTABLE  // Have one window with at least one tab with syncable content.
  };

  explicit SyncedSessionTracker(SyncSessionsClient* sessions_client);
  ~SyncedSessionTracker();

  // **** Synced session/tab query methods. ****

  // Returns all foreign sessions we're tracking (skips the local session
  // object). SyncedSession ownership remains within the SyncedSessionTracker.
  // Lookup parameter is used to decide which foreign tabs should be include.
  std::vector<const SyncedSession*> LookupAllForeignSessions(
      SessionLookup lookup) const;

  // Returns the tab node ids (see GetTab) for all the tabs* associated with the
  // session having tag |session_tag|.
  std::set<int> LookupTabNodeIds(const std::string& session_tag) const;

  // Attempts to look up the session windows associatd with the session given
  // by |session_tag|. Ownership of SessionWindows stays within the
  // SyncedSessionTracker.
  // If lookup succeeds:
  // - Fills windows with the SessionWindow pointers, returns true.
  // Else
  // - Returns false.
  bool LookupSessionWindows(
      const std::string& session_tag,
      std::vector<const sessions::SessionWindow*>* windows) const;

  // Attempts to look up the tab associated with the given tag and tab id.
  // Ownership of the SessionTab remains within the SyncedSessionTracker.
  // Returns null if lookup fails.
  const sessions::SessionTab* LookupSessionTab(const std::string& session_tag,
                                               SessionID tab_id) const;

  // Allows retrieval of existing data for the local session. Unlike GetSession
  // this won't create-if-not-present and will return null instead.
  const SyncedSession* LookupLocalSession() const;

  // **** Methods for manipulating synced sessions and tabs. ****

  // Returns a pointer to the SyncedSession object associated with
  // |session_tag|. If none exists, returns nullptr. Ownership of the
  // SyncedSession remains within the SyncedSessionTracker.
  const SyncedSession* LookupSession(const std::string& session_tag) const;

  // Returns a pointer to the SyncedSession object associated with
  // |session_tag|. If none exists, creates one. Ownership of the
  // SyncedSession remains within the SyncedSessionTracker.
  SyncedSession* GetSession(const std::string& session_tag);

  // Resets the tracking information for the session specified by |session_tag|.
  // This involves clearing all the windows and tabs from the session, while
  // keeping pointers saved in the synced_window_map_ and synced_tab_map_. Once
  // reset, all calls to PutWindowInSession and PutTabInWindow will denote that
  // the requested windows and tabs are owned and add them back to their
  // session. The next call to CleanupSession(...) will delete those windows and
  // tabs not owned.
  void ResetSessionTracking(const std::string& session_tag);

  // Deletes those windows and tabs associated with |session_tag| that are no
  // longer owned. See ResetSessionTracking(...)..
  void CleanupSession(const std::string& session_tag);

  // Adds the window with id |window_id| to the session specified by
  // |session_tag|. If none existed for that session, creates one. Similarly, if
  // the session did not exist yet, creates it. Ownership of the SessionWindow
  // remains within the SyncedSessionTracker.
  // Attempting to add a window to a session multiple times will have no effect.
  void PutWindowInSession(const std::string& session_tag, SessionID window_id);

  // Adds the tab with id |tab_id| to the window |window_id|. If none existed
  // for that session, creates one. Ownership of the SessionTab remains within
  // the SyncedSessionTracker.
  //
  // Note: GetSession(..) must have already been called with |session_tag| to
  // ensure we having mapping information for this session.
  void PutTabInWindow(const std::string& session_tag,
                      SessionID window_id,
                      SessionID tab_id);

  // Adds |tab_node_id| to the session specified by |session_tag|, creating that
  // session if necessary. This is necessary to ensure that each session has an
  // up to date list of tab nodes linked to it for session deletion purposes.
  // Note that this won't update the local tab pool, even if the local session
  // tag is passed. The tab pool is only updated with new tab nodes when they're
  // associated with a tab id (see ReassociateLocalTabNode or
  // GetTabNodeFromLocalTabId).
  void OnTabNodeSeen(const std::string& session_tag, int tab_node_id);

  // Returns a pointer to the SessionTab object associated with
  // |tab_id| for the session specified with |session_tag|.
  // Note: Ownership of the SessionTab remains within the SyncedSessionTracker.
  // TODO(zea): Replace SessionTab with a Sync specific wrapper.
  // https://crbug.com/662597
  sessions::SessionTab* GetTab(const std::string& session_tag,
                               SessionID tab_id);

  // **** Methods specific to foreign sessions. ****

  // Tracks the deletion of a foreign tab by removing the given |tab_node_id|
  // from the parent session. Doesn't actually remove any tab objects because
  // the header may have or may not have already been updated to no longer
  // parent this tab. Regardless, when the header is updated then cleanup will
  // remove the actual tab data. However, this method always needs to be called
  // upon foreign tab deletion, otherwise LookupTabNodeIds(...) may return
  // already deleted tab node ids.
  void DeleteForeignTab(const std::string& session_tag, int tab_node_id);

  // Deletes the session associated with |session_tag| if it exists.
  // Returns true if the session existed and was deleted, false otherwise.
  bool DeleteForeignSession(const std::string& session_tag);

  // **** Methods specific to the local session. ****

  // Set the local session information. Must be called before any other local
  // session methods are invoked.
  void InitLocalSession(const std::string& local_session_tag,
                        const std::string& local_session_name,
                        sync_pb::SyncEnums::DeviceType local_device_type);

  // Gets the session tag previously set with InitLocalSession().
  const std::string& GetLocalSessionTag() const;

  // Similar to CleanupForeignSession, but also marks any unmapped tabs as free
  // in the tab node pool and fills |deleted_node_ids| with the set of locally
  // free tab nodes to be deleted.
  void CleanupLocalTabs(std::set<int>* deleted_node_ids);

  // Fills |tab_node_id| with a tab node for |tab_id|. Returns true if an
  // existing tab node was found, false if there was none and one had to be
  // created.
  bool GetTabNodeFromLocalTabId(SessionID tab_id, int* tab_node_id);

  // Returns whether |tab_node_id| refers to a valid tab node that is associated
  // with a tab.
  bool IsLocalTabNodeAssociated(int tab_node_id);

  // Reassociates the tab denoted by |tab_node_id| with a new tab id, preserving
  // any previous SessionTab object the node was associated with. This is useful
  // on restart when sync needs to reassociate tabs from a previous session with
  // newly restored tabs (and can be used in conjunction with PutTabInWindow).
  // If |new_tab_id| is already associated with a tab object, that tab will be
  // overwritten. Reassociating a tab with a node it is already mapped to will
  // have no effect.
  void ReassociateLocalTab(int tab_node_id, SessionID new_tab_id);

  // **** Methods for querying/manipulating overall state ****.

  // Free the memory for all dynamically allocated objects and clear the
  // tracking structures.
  void Clear();

  bool Empty() const { return session_map_.empty(); }

  // Includes both foreign sessions and the local session.
  size_t num_synced_sessions() const { return session_map_.size(); }

  // Returns the number of tabs associated with the specified session tag.
  size_t num_synced_tabs(const std::string& session_tag) const {
    auto iter = session_map_.find(session_tag);
    if (iter != session_map_.end()) {
      return iter->second.synced_tab_map.size();
    } else {
      return 0;
    }
  }

  // Returns whether a tab is unmapped or not.
  bool IsTabUnmappedForTesting(SessionID tab_id);

 private:
  friend class SessionsSyncManagerTest;
  friend class SyncedSessionTrackerTest;

  struct TrackedSession {
    TrackedSession();
    ~TrackedSession();

    // Owns the SyncedSessions, and transitively, all of the windows and tabs
    // they contain.
    SyncedSession synced_session;

    // The mapping of tab/window to their SessionTab/SessionWindow objects.
    // The SessionTab/SessionWindow objects referred to may be owned either by
    // the session in the |synced_session| or be temporarily unmapped and live
    // in the |unmapped_tabs|/|unmapped_windows| collections.
    std::map<SessionID, sessions::SessionTab*> synced_tab_map;
    std::map<SessionID, SyncedSessionWindow*> synced_window_map;

    // The collection of tabs/windows not owned by SyncedSession. This is the
    // case either because 1. (in the case of tabs) they were newly created by
    // GetTab() and not yet added to a session, or 2. they were removed from
    // their owning session by a call to ResetSessionTracking() and not yet
    // added back.
    std::map<SessionID, std::unique_ptr<sessions::SessionTab>> unmapped_tabs;
    std::map<SessionID, std::unique_ptr<SyncedSessionWindow>> unmapped_windows;

    // A tab node id is part of the identifier for the sync tab objects. Tab
    // node ids are not used for interacting with the model/browser tabs.
    // However, when when we want to delete a foreign session, we use these
    // values to inform sync which tabs to delete. We are extracting these tab
    // node ids from individual session (tab, not header) specifics, but store
    // them here during runtime. We do this because tab node ids may be reused
    // for different tabs, and tracking which tab id is currently associated
    // with each tab node id is both difficult and unnecessary.
    std::set<int> tab_node_ids;
  };

  // LookupTrackedSession() returns null if the session tag is unknown.
  const TrackedSession* LookupTrackedSession(
      const std::string& session_tag) const;
  TrackedSession* LookupTrackedSession(const std::string& session_tag);
  // Creates tracked session if it wasn't known previously. Never returns null.
  TrackedSession* GetTrackedSession(const std::string& session_tag);

  // Implementation of CleanupForeignSession/CleanupLocalTabs.
  void CleanupSessionImpl(const std::string& session_tag);

  // The client of the sync sessions datatype.
  SyncSessionsClient* const sessions_client_;

  // Map: session tag -> TrackedSession.
  std::map<std::string, TrackedSession> session_map_;

  // The tag for this machine's local session, so we can distinguish the foreign
  // sessions.
  std::string local_session_tag_;

  // Pool of used/available sync nodes associated with local tabs.
  TabNodePool local_tab_pool_;

  DISALLOW_COPY_AND_ASSIGN(SyncedSessionTracker);
};

// Helper function to load and add window or tab data from synced specifics to
// our internal tracking in SyncedSessionTracker.
void UpdateTrackerWithSpecifics(const sync_pb::SessionSpecifics& specifics,
                                base::Time modification_time,
                                SyncedSessionTracker* tracker);

}  // namespace sync_sessions

#endif  // COMPONENTS_SYNC_SESSIONS_SYNCED_SESSION_TRACKER_H_
