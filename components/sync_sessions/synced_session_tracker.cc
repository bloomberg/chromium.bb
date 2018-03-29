// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/synced_session_tracker.h"

#include <algorithm>
#include <utility>

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "components/sync/protocol/session_specifics.pb.h"
#include "components/sync_sessions/sync_sessions_client.h"
#include "components/sync_sessions/synced_tab_delegate.h"

namespace sync_sessions {

namespace {

// Helper for iterating through all tabs within a window, and all navigations
// within a tab, to find if there's a valid syncable url.
bool ShouldSyncSessionWindow(SyncSessionsClient* sessions_client,
                             const sessions::SessionWindow& window) {
  for (const auto& tab : window.tabs) {
    for (const sessions::SerializedNavigationEntry& navigation :
         tab->navigations) {
      if (sessions_client->ShouldSyncURL(navigation.virtual_url())) {
        return true;
      }
    }
  }
  return false;
}

// Presentable means |foreign_session| must have syncable content.
bool IsPresentable(SyncSessionsClient* sessions_client,
                   const SyncedSession& foreign_session) {
  for (auto iter = foreign_session.windows.begin();
       iter != foreign_session.windows.end(); ++iter) {
    if (ShouldSyncSessionWindow(sessions_client,
                                iter->second->wrapped_window)) {
      return true;
    }
  }
  return false;
}

// Verify that tab IDs appear only once within a session. Intended to prevent
// http://crbug.com/360822.
bool IsValidSessionHeader(const sync_pb::SessionHeader& header) {
  std::set<int> session_tab_ids;
  for (int i = 0; i < header.window_size(); ++i) {
    const sync_pb::SessionWindow& window = header.window(i);
    for (int j = 0; j < window.tab_size(); ++j) {
      const int tab_id = window.tab(j);
      bool success = session_tab_ids.insert(tab_id).second;
      if (!success)
        return false;
    }
  }

  return true;
}

void PopulateSyncedSessionWindowFromSpecifics(
    const std::string& session_tag,
    const sync_pb::SessionWindow& specifics,
    base::Time mtime,
    SyncedSessionWindow* synced_session_window,
    SyncedSessionTracker* tracker) {
  sessions::SessionWindow* session_window =
      &synced_session_window->wrapped_window;
  if (specifics.has_window_id()) {
    session_window->window_id =
        SessionID::FromSerializedValue(specifics.window_id());
  }
  if (specifics.has_selected_tab_index())
    session_window->selected_tab_index = specifics.selected_tab_index();
  synced_session_window->window_type = specifics.browser_type();
  if (specifics.has_browser_type()) {
    if (specifics.browser_type() ==
        sync_pb::SessionWindow_BrowserType_TYPE_TABBED) {
      session_window->type = sessions::SessionWindow::TYPE_TABBED;
    } else {
      // Note: custom tabs are treated like popup windows on restore, as you can
      // restore a custom tab on a platform that doesn't support them.
      session_window->type = sessions::SessionWindow::TYPE_POPUP;
    }
  }
  session_window->timestamp = mtime;
  session_window->tabs.clear();
  for (int i = 0; i < specifics.tab_size(); i++) {
    SessionID tab_id = SessionID::FromSerializedValue(specifics.tab(i));
    tracker->PutTabInWindow(session_tag, session_window->window_id, tab_id);
  }
}

void PopulateSyncedSessionFromSpecifics(
    const std::string& session_tag,
    const sync_pb::SessionHeader& header_specifics,
    base::Time mtime,
    SyncedSession* synced_session,
    SyncedSessionTracker* tracker) {
  if (header_specifics.has_client_name())
    synced_session->session_name = header_specifics.client_name();
  if (header_specifics.has_device_type()) {
    synced_session->device_type = header_specifics.device_type();
  }
  synced_session->modified_time =
      std::max(mtime, synced_session->modified_time);

  // Process all the windows and their tab information.
  int num_windows = header_specifics.window_size();
  DVLOG(1) << "Populating " << session_tag << " with " << num_windows
           << " windows.";

  for (int i = 0; i < num_windows; ++i) {
    const sync_pb::SessionWindow& window_s = header_specifics.window(i);
    SessionID window_id = SessionID::FromSerializedValue(window_s.window_id());
    tracker->PutWindowInSession(session_tag, window_id);
    PopulateSyncedSessionWindowFromSpecifics(
        session_tag, window_s, synced_session->modified_time,
        synced_session->windows[window_id].get(), tracker);
  }
}

}  // namespace

SyncedSessionTracker::TrackedSession::TrackedSession() {}

SyncedSessionTracker::TrackedSession::~TrackedSession() {}

SyncedSessionTracker::SyncedSessionTracker(SyncSessionsClient* sessions_client)
    : sessions_client_(sessions_client) {}

SyncedSessionTracker::~SyncedSessionTracker() {}

void SyncedSessionTracker::InitLocalSession(
    const std::string& local_session_tag,
    const std::string& local_session_name,
    sync_pb::SyncEnums::DeviceType local_device_type) {
  DCHECK(local_session_tag_.empty());
  DCHECK(!local_session_tag.empty());
  local_session_tag_ = local_session_tag;

  SyncedSession* local_session = GetSession(local_session_tag);
  local_session->session_name = local_session_name;
  local_session->device_type = local_device_type;
  local_session->session_tag = local_session_tag;
}

const std::string& SyncedSessionTracker::GetLocalSessionTag() const {
  return local_session_tag_;
}

std::vector<const SyncedSession*>
SyncedSessionTracker::LookupAllForeignSessions(SessionLookup lookup) const {
  std::vector<const SyncedSession*> sessions;
  for (const auto& session_pair : session_map_) {
    const SyncedSession& foreign_session = session_pair.second.synced_session;
    if (session_pair.first != local_session_tag_ &&
        (lookup == RAW || IsPresentable(sessions_client_, foreign_session))) {
      sessions.push_back(&foreign_session);
    }
  }
  return sessions;
}

bool SyncedSessionTracker::LookupSessionWindows(
    const std::string& session_tag,
    std::vector<const sessions::SessionWindow*>* windows) const {
  DCHECK(windows);
  windows->clear();

  const TrackedSession* session = LookupTrackedSession(session_tag);
  if (!session)
    return false;  // We have no record of this session.

  for (const auto& window_pair : session->synced_session.windows)
    windows->push_back(&window_pair.second->wrapped_window);

  return true;
}

const sessions::SessionTab* SyncedSessionTracker::LookupSessionTab(
    const std::string& tag,
    SessionID tab_id) const {
  if (!tab_id.is_valid())
    return nullptr;

  const TrackedSession* session = LookupTrackedSession(tag);
  if (!session)
    return nullptr;  // We have no record of this session.

  auto tab_iter = session->synced_tab_map.find(tab_id);
  if (tab_iter == session->synced_tab_map.end())
    return nullptr;  // We have no record of this tab.

  return tab_iter->second;
}

void SyncedSessionTracker::LookupForeignTabNodeIds(
    const std::string& session_tag,
    std::set<int>* tab_node_ids) const {
  tab_node_ids->clear();
  const TrackedSession* session = LookupTrackedSession(session_tag);
  if (session) {
    tab_node_ids->insert(session->tab_node_ids.begin(),
                         session->tab_node_ids.end());
  }
  // In case an invalid node id was included, remove it.
  tab_node_ids->erase(TabNodePool::kInvalidTabNodeID);
}

const SyncedSession* SyncedSessionTracker::LookupLocalSession() const {
  const TrackedSession* session = LookupTrackedSession(local_session_tag_);
  return session ? &session->synced_session : nullptr;
}

SyncedSession* SyncedSessionTracker::GetSession(
    const std::string& session_tag) {
  return &GetTrackedSession(session_tag)->synced_session;
}

bool SyncedSessionTracker::DeleteForeignSession(
    const std::string& session_tag) {
  DCHECK_NE(local_session_tag_, session_tag);

  auto iter = session_map_.find(session_tag);
  if (iter == session_map_.end())
    return false;

  // An implicitly created session that has children tabs but no header node
  // will have never had the device_type changed from unset.
  const bool header_existed =
      iter->second.synced_session.device_type != sync_pb::SyncEnums::TYPE_UNSET;
  // SyncedSession's destructor will trigger deletion of windows which will in
  // turn trigger the deletion of tabs. This doesn't affect the convenience
  // maps.
  session_map_.erase(iter);

  return header_existed;
}

void SyncedSessionTracker::ResetSessionTracking(
    const std::string& session_tag) {
  TrackedSession* session = GetTrackedSession(session_tag);

  for (auto& window_pair : session->synced_session.windows) {
    // First unmap the tabs in the window.
    for (auto& tab : window_pair.second->wrapped_window.tabs) {
      SessionID tab_id = tab->tab_id;
      session->unmapped_tabs[tab_id] = std::move(tab);
    }
    window_pair.second->wrapped_window.tabs.clear();

    // Then unmap the window itself.
    session->unmapped_windows[window_pair.first] =
        std::move(window_pair.second);
  }
  session->synced_session.windows.clear();
}

void SyncedSessionTracker::DeleteForeignTab(const std::string& session_tag,
                                            int tab_node_id) {
  DCHECK_NE(local_session_tag_, session_tag);
  TrackedSession* session = LookupTrackedSession(session_tag);
  if (session)
    session->tab_node_ids.erase(tab_node_id);
}

const SyncedSessionTracker::TrackedSession*
SyncedSessionTracker::LookupTrackedSession(
    const std::string& session_tag) const {
  std::map<std::string, TrackedSession>::const_iterator it =
      session_map_.find(session_tag);
  return it == session_map_.end() ? nullptr : &it->second;
}

SyncedSessionTracker::TrackedSession*
SyncedSessionTracker::LookupTrackedSession(const std::string& session_tag) {
  std::map<std::string, TrackedSession>::iterator it =
      session_map_.find(session_tag);
  return it == session_map_.end() ? nullptr : &it->second;
}

SyncedSessionTracker::TrackedSession* SyncedSessionTracker::GetTrackedSession(
    const std::string& session_tag) {
  TrackedSession* session = LookupTrackedSession(session_tag);
  if (session)
    return session;

  session = &session_map_[session_tag];
  DVLOG(1) << "Creating new session with tag " << session_tag << " at "
           << session;
  session->synced_session.session_tag = session_tag;
  return session;
}

void SyncedSessionTracker::CleanupSessionImpl(const std::string& session_tag) {
  TrackedSession* session = LookupTrackedSession(session_tag);
  if (!session)
    return;

  for (const auto& window_pair : session->unmapped_windows)
    session->synced_window_map.erase(window_pair.first);
  session->unmapped_windows.clear();

  for (const auto& tab_pair : session->unmapped_tabs)
    session->synced_tab_map.erase(tab_pair.first);
  session->unmapped_tabs.clear();
}

bool SyncedSessionTracker::IsTabUnmappedForTesting(SessionID tab_id) {
  const TrackedSession* session = LookupTrackedSession(local_session_tag_);
  if (!session)
    return false;

  return session->unmapped_tabs.count(tab_id) != 0;
}

std::set<int> SyncedSessionTracker::GetTabNodeIdsForTesting(
    const std::string& session_tag) const {
  const TrackedSession* session = LookupTrackedSession(session_tag);
  return session ? session->tab_node_ids : std::set<int>();
}

void SyncedSessionTracker::PutWindowInSession(const std::string& session_tag,
                                              SessionID window_id) {
  TrackedSession* session = GetTrackedSession(session_tag);
  if (session->synced_session.windows.count(window_id) != 0) {
    DVLOG(1) << "Window " << window_id << " already added to session "
             << session_tag;
    return;
  }
  std::unique_ptr<SyncedSessionWindow> window;

  auto iter = session->unmapped_windows.find(window_id);
  if (iter != session->unmapped_windows.end()) {
    DCHECK_EQ(session->synced_window_map[window_id], iter->second.get());
    window = std::move(iter->second);
    session->unmapped_windows.erase(iter);
    DVLOG(1) << "Putting seen window " << window_id << " at " << window.get()
             << "in " << (session_tag == local_session_tag_ ? "local session"
                                                            : session_tag);
  } else {
    // Create the window.
    window = std::make_unique<SyncedSessionWindow>();
    window->wrapped_window.window_id = window_id;
    session->synced_window_map[window_id] = window.get();
    DVLOG(1) << "Putting new window " << window_id << " at " << window.get()
             << "in " << (session_tag == local_session_tag_ ? "local session"
                                                            : session_tag);
  }
  DCHECK_EQ(window->wrapped_window.window_id, window_id);
  DCHECK(GetSession(session_tag)->windows.end() ==
         GetSession(session_tag)->windows.find(window_id));
  GetSession(session_tag)->windows[window_id] = std::move(window);
}

void SyncedSessionTracker::PutTabInWindow(const std::string& session_tag,
                                          SessionID window_id,
                                          SessionID tab_id) {
  TrackedSession* session = LookupTrackedSession(session_tag);
  DCHECK(session);

  // We're called here for two reasons. 1) We've received an update to the
  // SessionWindow information of a SessionHeader node for a session,
  // and 2) The SessionHeader node for our local session changed. In both cases
  // we need to update our tracking state to reflect the change.
  //
  // Because the SessionHeader nodes are separate from the individual tab nodes
  // and we don't store tab_node_ids in the header / SessionWindow specifics,
  // the tab_node_ids are not always available when processing headers. We know
  // that we will eventually process (via GetTab) every single tab node in the
  // system, so we permit ourselves to just call GetTab and ignore the result,
  // creating a placeholder SessionTab in the process.
  sessions::SessionTab* tab_ptr = GetTab(session_tag, tab_id);

  // The tab should be unmapped.
  std::unique_ptr<sessions::SessionTab> tab;
  auto it = session->unmapped_tabs.find(tab_id);
  if (it != session->unmapped_tabs.end()) {
    tab = std::move(it->second);
    session->unmapped_tabs.erase(it);
  } else {
    // The tab has already been mapped, possibly because of the tab node id
    // being reused across tabs. Find the existing tab and move it to the right
    // window.
    for (auto& window_iter_pair : GetSession(session_tag)->windows) {
      auto tab_iter = std::find_if(
          window_iter_pair.second->wrapped_window.tabs.begin(),
          window_iter_pair.second->wrapped_window.tabs.end(),
          [&tab_ptr](const std::unique_ptr<sessions::SessionTab>& tab) {
            return tab.get() == tab_ptr;
          });
      if (tab_iter != window_iter_pair.second->wrapped_window.tabs.end()) {
        tab = std::move(*tab_iter);
        window_iter_pair.second->wrapped_window.tabs.erase(tab_iter);

        DVLOG(1) << "Moving tab " << tab_id << " from window "
                 << window_iter_pair.first << " to " << window_id;
        break;
      }
    }
    // TODO(zea): remove this once PutTabInWindow isn't crashing anymore.
    CHECK(tab) << " Unable to find tab " << tab_id
               << " within unmapped tabs or previously mapped windows."
               << " https://crbug.com/639009";
  }

  tab->window_id = window_id;
  DVLOG(1) << "  - tab " << tab_id << " added to window " << window_id;
  DCHECK(GetSession(session_tag)->windows.find(window_id) !=
         GetSession(session_tag)->windows.end());
  GetSession(session_tag)
      ->windows[window_id]
      ->wrapped_window.tabs.push_back(std::move(tab));
}

void SyncedSessionTracker::OnTabNodeSeen(const std::string& session_tag,
                                         int tab_node_id) {
  GetTrackedSession(session_tag)->tab_node_ids.insert(tab_node_id);
}

sessions::SessionTab* SyncedSessionTracker::GetTab(
    const std::string& session_tag,
    SessionID tab_id) {
  CHECK(tab_id.is_valid()) << "https://crbug.com/639009";

  TrackedSession* session = GetTrackedSession(session_tag);
  sessions::SessionTab* tab_ptr = nullptr;
  auto iter = session->synced_tab_map.find(tab_id);
  if (iter != session->synced_tab_map.end()) {
    tab_ptr = iter->second;

    if (VLOG_IS_ON(1)) {
      std::string title;
      if (tab_ptr->navigations.size() > 0) {
        title =
            " (" + base::UTF16ToUTF8(tab_ptr->navigations.back().title()) + ")";
      }
      DVLOG(1) << "Getting "
               << (session_tag == local_session_tag_ ? "local session"
                                                     : session_tag)
               << "'s seen tab " << tab_id << " at " << tab_ptr << " " << title;
    }
  } else {
    auto tab = std::make_unique<sessions::SessionTab>();
    tab_ptr = tab.get();
    tab->tab_id = tab_id;
    session->synced_tab_map[tab_id] = tab_ptr;
    session->unmapped_tabs[tab_id] = std::move(tab);
    DVLOG(1) << "Getting "
             << (session_tag == local_session_tag_ ? "local session"
                                                   : session_tag)
             << "'s new tab " << tab_id << " at " << tab_ptr;
  }
  DCHECK(tab_ptr);
  DCHECK_EQ(tab_ptr->tab_id, tab_id);
  return tab_ptr;
}

void SyncedSessionTracker::CleanupSession(const std::string& session_tag) {
  CleanupSessionImpl(session_tag);
}

void SyncedSessionTracker::CleanupLocalTabs(std::set<int>* deleted_node_ids) {
  DCHECK(!local_session_tag_.empty());
  TrackedSession* session = GetTrackedSession(local_session_tag_);
  for (const auto& tab_pair : session->unmapped_tabs)
    local_tab_pool_.FreeTab(tab_pair.first);
  CleanupSessionImpl(local_session_tag_);
  local_tab_pool_.CleanupTabNodes(deleted_node_ids);
  for (int tab_node_id : *deleted_node_ids) {
    session->tab_node_ids.erase(tab_node_id);
  }
}

bool SyncedSessionTracker::GetTabNodeFromLocalTabId(SessionID tab_id,
                                                    int* tab_node_id) {
  DCHECK(!local_session_tag_.empty());
  // Ensure a placeholder SessionTab is in place, if not already. Although we
  // don't need a SessionTab to fulfill this request, this forces the creation
  // of one if it doesn't already exist. This helps to make sure we're tracking
  // this |tab_id| if |local_tab_pool_| is, and everyone's data structures are
  // kept in sync and as consistent as possible.
  GetTab(local_session_tag_, tab_id);  // Ignore result.

  bool reused_existing_tab =
      local_tab_pool_.GetTabNodeForTab(tab_id, tab_node_id);
  DCHECK_NE(TabNodePool::kInvalidTabNodeID, *tab_node_id);
  GetTrackedSession(local_session_tag_)->tab_node_ids.insert(*tab_node_id);
  return reused_existing_tab;
}

bool SyncedSessionTracker::IsLocalTabNodeAssociated(int tab_node_id) {
  if (tab_node_id == TabNodePool::kInvalidTabNodeID)
    return false;
  return local_tab_pool_.GetTabIdFromTabNodeId(tab_node_id).is_valid();
}

void SyncedSessionTracker::ReassociateLocalTab(int tab_node_id,
                                               SessionID new_tab_id) {
  DCHECK(!local_session_tag_.empty());
  DCHECK_NE(TabNodePool::kInvalidTabNodeID, tab_node_id);
  DCHECK(new_tab_id.is_valid());

  TrackedSession* session = LookupTrackedSession(local_session_tag_);
  DCHECK(session);

  SessionID old_tab_id = local_tab_pool_.GetTabIdFromTabNodeId(tab_node_id);
  if (new_tab_id == old_tab_id) {
    return;
  }

  local_tab_pool_.ReassociateTabNode(tab_node_id, new_tab_id);

  sessions::SessionTab* tab_ptr = nullptr;

  auto new_tab_iter = session->synced_tab_map.find(new_tab_id);
  auto old_tab_iter = session->synced_tab_map.find(old_tab_id);
  if (old_tab_id.is_valid() && old_tab_iter != session->synced_tab_map.end()) {
    tab_ptr = old_tab_iter->second;
    DCHECK(tab_ptr);

    // Remove the tab from the synced tab map under the old id.
    session->synced_tab_map.erase(old_tab_iter);

    if (new_tab_iter != session->synced_tab_map.end()) {
      // If both the old and the new tab already exist, delete the new tab
      // and use the old tab in its place.
      auto unmapped_tabs_iter = session->unmapped_tabs.find(new_tab_id);
      if (unmapped_tabs_iter != session->unmapped_tabs.end()) {
        session->unmapped_tabs.erase(unmapped_tabs_iter);
      } else {
        sessions::SessionTab* new_tab_ptr = new_tab_iter->second;
        for (auto& window_iter_pair : session->synced_session.windows) {
          auto& window_tabs = window_iter_pair.second->wrapped_window.tabs;
          auto tab_iter = std::find_if(
              window_tabs.begin(), window_tabs.end(),
              [&new_tab_ptr](const std::unique_ptr<sessions::SessionTab>& tab) {
                return tab.get() == new_tab_ptr;
              });
          if (tab_iter != window_tabs.end()) {
            window_tabs.erase(tab_iter);
            break;
          }
        }
      }

      session->synced_tab_map.erase(new_tab_iter);
    }

    // If the old tab is unmapped, update the tab id under which it is
    // indexed.
    auto unmapped_tabs_iter = session->unmapped_tabs.find(old_tab_id);
    if (old_tab_id.is_valid() &&
        unmapped_tabs_iter != session->unmapped_tabs.end()) {
      std::unique_ptr<sessions::SessionTab> tab =
          std::move(unmapped_tabs_iter->second);
      DCHECK_EQ(tab_ptr, tab.get());
      session->unmapped_tabs.erase(unmapped_tabs_iter);
      session->unmapped_tabs[new_tab_id] = std::move(tab);
    }
  }

  if (tab_ptr == nullptr) {
    // It's possible a placeholder is already in place for the new tab. If so,
    // reuse it, otherwise create a new one (which will default to unmapped).
    tab_ptr = GetTab(local_session_tag_, new_tab_id);
  }

  // Update the tab id.
  if (old_tab_id.is_valid()) {
    DVLOG(1) << "Remapped tab " << old_tab_id << " with node " << tab_node_id
             << " to tab " << new_tab_id;
  } else {
    DVLOG(1) << "Mapped new tab node " << tab_node_id << " to tab "
             << new_tab_id;
  }
  tab_ptr->tab_id = new_tab_id;

  // Add the tab back into the tab map with the new id.
  session->synced_tab_map[new_tab_id] = tab_ptr;
  session->tab_node_ids.insert(tab_node_id);
}

void SyncedSessionTracker::Clear() {
  session_map_.clear();
  local_tab_pool_.Clear();
  local_session_tag_.clear();
}

void UpdateTrackerWithSpecifics(const sync_pb::SessionSpecifics& specifics,
                                base::Time modification_time,
                                SyncedSessionTracker* tracker) {
  std::string session_tag = specifics.session_tag();
  SyncedSession* session = tracker->GetSession(session_tag);
  if (specifics.has_header()) {
    // Read in the header data for this session. Header data is
    // essentially a collection of windows, each of which has an ordered id list
    // for their tabs.

    if (!IsValidSessionHeader(specifics.header())) {
      LOG(WARNING) << "Ignoring session node with invalid header "
                   << "and tag " << session_tag << ".";
      return;
    }

    // Load (or create) the SyncedSession object for this client.
    const sync_pb::SessionHeader& header = specifics.header();

    // Reset the tab/window tracking for this session (must do this before
    // we start calling PutWindowInSession and PutTabInWindow so that all
    // unused tabs/windows get cleared by the CleanupSession(...) call).
    tracker->ResetSessionTracking(session_tag);

    PopulateSyncedSessionFromSpecifics(session_tag, header, modification_time,
                                       session, tracker);

    // Delete any closed windows and unused tabs as necessary.
    tracker->CleanupSession(session_tag);
  } else if (specifics.has_tab()) {
    const sync_pb::SessionTab& tab_s = specifics.tab();
    SessionID tab_id = SessionID::FromSerializedValue(tab_s.tab_id());
    DVLOG(1) << "Populating " << session_tag << "'s tab id " << tab_id
             << " from node " << specifics.tab_node_id();

    // Ensure the tracker is aware of the tab node id. Deleting foreign sessions
    // requires deleting all relevant tab nodes, and it's easier to track the
    // tab node ids themselves separately from the tab ids.
    //
    // Note that TabIDs are not stable across restarts of a client. Consider
    // this example with two tabs:
    //
    // http://a.com  TabID1 --> NodeIDA
    // http://b.com  TabID2 --> NodeIDB
    //
    // After restart, tab ids are reallocated. e.g, one possibility:
    // http://a.com TabID2 --> NodeIDA
    // http://b.com TabID1 --> NodeIDB
    //
    // If that happened on a remote client, here we will see an update to
    // TabID1 with tab_node_id changing from NodeIDA to NodeIDB, and TabID2
    // with tab_node_id changing from NodeIDB to NodeIDA.
    //
    // We can also wind up here if we created this tab as an out-of-order
    // update to the header node for this session before actually associating
    // the tab itself, so the tab node id wasn't available at the time and
    // is currently kInvalidTabNodeID.
    //
    // In both cases, we can safely throw it into the set of node ids.
    tracker->OnTabNodeSeen(session_tag, specifics.tab_node_id());
    sessions::SessionTab* tab = tracker->GetTab(session_tag, tab_id);
    if (!tab->timestamp.is_null() && tab->timestamp > modification_time) {
      DVLOG(1) << "Ignoring " << session_tag << "'s session tab " << tab_id
               << " with earlier modification time: " << tab->timestamp
               << " vs " << modification_time;
      return;
    }

    // Update SessionTab based on protobuf.
    tab->SetFromSyncData(tab_s, modification_time);

    // Update the last modified time.
    if (session->modified_time < modification_time)
      session->modified_time = modification_time;
  } else {
    LOG(WARNING) << "Ignoring session node with missing header/tab "
                 << "fields and tag " << session_tag << ".";
  }
}

}  // namespace sync_sessions
