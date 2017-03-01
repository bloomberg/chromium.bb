// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/synced_session_tracker.h"

#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/sync_sessions/sync_sessions_client.h"

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
                   SyncedSession* foreign_session) {
  for (auto iter = foreign_session->windows.begin();
       iter != foreign_session->windows.end(); ++iter) {
    if (ShouldSyncSessionWindow(sessions_client, *(iter->second))) {
      return true;
    }
  }
  return false;
}

}  // namespace

SyncedSessionTracker::SyncedSessionTracker(SyncSessionsClient* sessions_client)
    : sessions_client_(sessions_client) {}

SyncedSessionTracker::~SyncedSessionTracker() {
  Clear();
}

void SyncedSessionTracker::SetLocalSessionTag(
    const std::string& local_session_tag) {
  DCHECK(local_session_tag_.empty());
  DCHECK(!local_session_tag.empty());
  local_session_tag_ = local_session_tag;
}

bool SyncedSessionTracker::LookupAllForeignSessions(
    std::vector<const SyncedSession*>* sessions,
    SessionLookup lookup) const {
  DCHECK(sessions);
  sessions->clear();
  for (const auto& session_pair : synced_session_map_) {
    SyncedSession* foreign_session = session_pair.second.get();
    if (session_pair.first != local_session_tag_ &&
        (lookup == RAW || IsPresentable(sessions_client_, foreign_session))) {
      sessions->push_back(foreign_session);
    }
  }
  return !sessions->empty();
}

bool SyncedSessionTracker::LookupSessionWindows(
    const std::string& session_tag,
    std::vector<const sessions::SessionWindow*>* windows) const {
  DCHECK(windows);
  windows->clear();

  auto iter = synced_session_map_.find(session_tag);
  if (iter == synced_session_map_.end())
    return false;
  for (const auto& window_pair : iter->second->windows)
    windows->push_back(window_pair.second.get());

  return true;
}

bool SyncedSessionTracker::LookupSessionTab(
    const std::string& tag,
    SessionID::id_type tab_id,
    const sessions::SessionTab** tab) const {
  if (tab_id == TabNodePool::kInvalidTabID)
    return false;

  DCHECK(tab);
  auto tab_map_iter = synced_tab_map_.find(tag);
  if (tab_map_iter == synced_tab_map_.end()) {
    // We have no record of this session.
    *tab = nullptr;
    return false;
  }
  auto tab_iter = tab_map_iter->second.find(tab_id);
  if (tab_iter == tab_map_iter->second.end()) {
    // We have no record of this tab.
    *tab = nullptr;
    return false;
  }
  *tab = tab_iter->second;
  return true;
}

void SyncedSessionTracker::LookupForeignTabNodeIds(
    const std::string& session_tag,
    std::set<int>* tab_node_ids) const {
  tab_node_ids->clear();
  auto session_iter = synced_session_map_.find(session_tag);
  if (session_iter != synced_session_map_.end()) {
    tab_node_ids->insert(session_iter->second->tab_node_ids.begin(),
                         session_iter->second->tab_node_ids.end());
  }
  // In case an invalid node id was included, remove it.
  tab_node_ids->erase(TabNodePool::kInvalidTabNodeID);
}

bool SyncedSessionTracker::LookupLocalSession(
    const SyncedSession** output) const {
  auto it = synced_session_map_.find(local_session_tag_);
  if (it != synced_session_map_.end()) {
    *output = it->second.get();
    return true;
  }
  return false;
}

SyncedSession* SyncedSessionTracker::GetSession(
    const std::string& session_tag) {
  if (synced_session_map_.find(session_tag) != synced_session_map_.end())
    return synced_session_map_[session_tag].get();

  std::unique_ptr<SyncedSession> synced_session =
      base::MakeUnique<SyncedSession>();
  DVLOG(1) << "Creating new session with tag " << session_tag << " at "
           << synced_session.get();
  synced_session->session_tag = session_tag;
  synced_session_map_[session_tag] = std::move(synced_session);

  return synced_session_map_[session_tag].get();
}

bool SyncedSessionTracker::DeleteForeignSession(
    const std::string& session_tag) {
  DCHECK_NE(local_session_tag_, session_tag);
  unmapped_windows_.erase(session_tag);
  unmapped_tabs_.erase(session_tag);

  bool header_existed = false;
  auto iter = synced_session_map_.find(session_tag);
  if (iter != synced_session_map_.end()) {
    // An implicitly created session that has children tabs but no header node
    // will have never had the device_type changed from unset.
    header_existed = iter->second->device_type != SyncedSession::TYPE_UNSET;
    // SyncedSession's destructor will trigger deletion of windows which will in
    // turn trigger the deletion of tabs. This doesn't affect the convenience
    // maps.
    synced_session_map_.erase(iter);
  }

  // These two erase(...) calls only affect the convenience maps.
  synced_window_map_.erase(session_tag);
  synced_tab_map_.erase(session_tag);

  return header_existed;
}

void SyncedSessionTracker::ResetSessionTracking(
    const std::string& session_tag) {
  SyncedSession* session = GetSession(session_tag);

  for (auto& window_pair : session->windows) {
    // First unmap the tabs in the window.
    for (auto& tab : window_pair.second->tabs) {
      SessionID::id_type tab_id = tab->tab_id.id();
      unmapped_tabs_[session_tag][tab_id] = std::move(tab);
    }
    window_pair.second->tabs.clear();

    // Then unmap the window itself.
    unmapped_windows_[session_tag][window_pair.first] =
        std::move(window_pair.second);
  }
  session->windows.clear();
}

void SyncedSessionTracker::DeleteForeignTab(const std::string& session_tag,
                                            int tab_node_id) {
  DCHECK_NE(local_session_tag_, session_tag);
  auto session_iter = synced_session_map_.find(session_tag);
  if (session_iter != synced_session_map_.end()) {
    session_iter->second->tab_node_ids.erase(tab_node_id);
  }
}

void SyncedSessionTracker::CleanupSessionImpl(const std::string& session_tag) {
  for (const auto& window_pair : unmapped_windows_[session_tag])
    synced_window_map_[session_tag].erase(window_pair.first);
  unmapped_windows_[session_tag].clear();

  for (const auto& tab_pair : unmapped_tabs_[session_tag])
    synced_tab_map_[session_tag].erase(tab_pair.first);
  unmapped_tabs_[session_tag].clear();
}

bool SyncedSessionTracker::IsTabUnmappedForTesting(SessionID::id_type tab_id) {
  auto it = unmapped_tabs_[local_session_tag_].find(tab_id);
  return it != unmapped_tabs_[local_session_tag_].end();
}

void SyncedSessionTracker::PutWindowInSession(const std::string& session_tag,
                                              SessionID::id_type window_id) {
  std::unique_ptr<sessions::SessionWindow> window;

  auto iter = unmapped_windows_[session_tag].find(window_id);
  if (iter != unmapped_windows_[session_tag].end()) {
    DCHECK_EQ(synced_window_map_[session_tag][window_id], iter->second.get());
    window = std::move(iter->second);
    unmapped_windows_[session_tag].erase(iter);
    DVLOG(1) << "Putting seen window " << window_id << " at " << window.get()
             << "in " << (session_tag == local_session_tag_ ? "local session"
                                                            : session_tag);
  } else {
    // Create the window.
    window = base::MakeUnique<sessions::SessionWindow>();
    window->window_id.set_id(window_id);
    synced_window_map_[session_tag][window_id] = window.get();
    DVLOG(1) << "Putting new window " << window_id << " at " << window.get()
             << "in " << (session_tag == local_session_tag_ ? "local session"
                                                            : session_tag);
  }
  DCHECK_EQ(window->window_id.id(), window_id);
  DCHECK(GetSession(session_tag)->windows.end() ==
         GetSession(session_tag)->windows.find(window_id));
  GetSession(session_tag)->windows[window_id] = std::move(window);
}

void SyncedSessionTracker::PutTabInWindow(const std::string& session_tag,
                                          SessionID::id_type window_id,
                                          SessionID::id_type tab_id) {
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
  auto it = unmapped_tabs_[session_tag].find(tab_id);
  if (it != unmapped_tabs_[session_tag].end()) {
    tab = std::move(it->second);
    unmapped_tabs_[session_tag].erase(it);
  } else {
    // The tab has already been mapped, possibly because of the tab node id
    // being reused across tabs. Find the existing tab and move it to the right
    // window.
    for (auto& window_iter_pair : GetSession(session_tag)->windows) {
      auto tab_iter = std::find_if(
          window_iter_pair.second->tabs.begin(),
          window_iter_pair.second->tabs.end(),
          [&tab_ptr](const std::unique_ptr<sessions::SessionTab>& tab) {
            return tab.get() == tab_ptr;
          });
      if (tab_iter != window_iter_pair.second->tabs.end()) {
        tab = std::move(*tab_iter);
        window_iter_pair.second->tabs.erase(tab_iter);

        DVLOG(1) << "Moving tab " << tab_id << " from window "
                 << window_iter_pair.first << " to " << window_id;
        break;
      }
    }
    // TODO(zea): remove this once PutTabInWindow isn't crashing anymore.
    CHECK(tab) << " Unable to find tab " << tab_id
               << " within unmapped tabs or previously mapped windows."
               << " crbug.com/639009";
  }

  tab->window_id.set_id(window_id);
  DVLOG(1) << "  - tab " << tab_id << " added to window " << window_id;
  DCHECK(GetSession(session_tag)->windows.find(window_id) !=
         GetSession(session_tag)->windows.end());
  auto& window_tabs = GetSession(session_tag)->windows[window_id]->tabs;
  window_tabs.push_back(std::move(tab));
}

void SyncedSessionTracker::OnTabNodeSeen(const std::string& session_tag,
                                         int tab_node_id) {
  GetSession(session_tag)->tab_node_ids.insert(tab_node_id);
}

sessions::SessionTab* SyncedSessionTracker::GetTab(
    const std::string& session_tag,
    SessionID::id_type tab_id) {
  CHECK_NE(TabNodePool::kInvalidTabNodeID, tab_id) << "crbug.com/673618";
  sessions::SessionTab* tab_ptr = nullptr;
  auto iter = synced_tab_map_[session_tag].find(tab_id);
  if (iter != synced_tab_map_[session_tag].end()) {
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
    std::unique_ptr<sessions::SessionTab> tab =
        base::MakeUnique<sessions::SessionTab>();
    tab_ptr = tab.get();
    tab->tab_id.set_id(tab_id);
    synced_tab_map_[session_tag][tab_id] = tab_ptr;
    unmapped_tabs_[session_tag][tab_id] = std::move(tab);
    DVLOG(1) << "Getting "
             << (session_tag == local_session_tag_ ? "local session"
                                                   : session_tag)
             << "'s new tab " << tab_id << " at " << tab_ptr;
  }
  DCHECK(tab_ptr);
  DCHECK_EQ(tab_ptr->tab_id.id(), tab_id);
  return tab_ptr;
}

void SyncedSessionTracker::CleanupForeignSession(
    const std::string& session_tag) {
  DCHECK_NE(local_session_tag_, session_tag);
  CleanupSessionImpl(session_tag);
}

void SyncedSessionTracker::CleanupLocalTabs(std::set<int>* deleted_node_ids) {
  DCHECK(!local_session_tag_.empty());
  for (const auto& tab_pair : unmapped_tabs_[local_session_tag_])
    local_tab_pool_.FreeTab(tab_pair.first);
  CleanupSessionImpl(local_session_tag_);
  local_tab_pool_.CleanupTabNodes(deleted_node_ids);
  for (int tab_node_id : *deleted_node_ids) {
    GetSession(local_session_tag_)->tab_node_ids.erase(tab_node_id);
  }
}

bool SyncedSessionTracker::GetTabNodeFromLocalTabId(SessionID::id_type tab_id,
                                                    int* tab_node_id) {
  DCHECK(!local_session_tag_.empty());
  // Ensure a placeholder SessionTab is in place, if not already.
  // Although we don't need a SessionTab to fulfill this request, this forces
  // the
  // creation of one if it doesn't already exist. This helps to make sure we're
  // tracking this |tab_id| if |local_tab_pool_| is, and everyone's data
  // structures
  // are kept in sync and as consistent as possible.
  GetTab(local_session_tag_, tab_id);  // Ignore result.

  bool reused_existing_tab =
      local_tab_pool_.GetTabNodeForTab(tab_id, tab_node_id);
  DCHECK_NE(TabNodePool::kInvalidTabNodeID, *tab_node_id);
  GetSession(local_session_tag_)->tab_node_ids.insert(*tab_node_id);
  return reused_existing_tab;
}

bool SyncedSessionTracker::IsLocalTabNodeAssociated(int tab_node_id) {
  if (tab_node_id == TabNodePool::kInvalidTabNodeID)
    return false;
  return local_tab_pool_.GetTabIdFromTabNodeId(tab_node_id) !=
         TabNodePool::kInvalidTabID;
}

void SyncedSessionTracker::ReassociateLocalTab(int tab_node_id,
                                               SessionID::id_type new_tab_id) {
  DCHECK(!local_session_tag_.empty());
  DCHECK_NE(TabNodePool::kInvalidTabNodeID, tab_node_id);
  DCHECK_NE(TabNodePool::kInvalidTabID, new_tab_id);

  SessionID::id_type old_tab_id =
      local_tab_pool_.GetTabIdFromTabNodeId(tab_node_id);
  local_tab_pool_.ReassociateTabNode(tab_node_id, new_tab_id);

  sessions::SessionTab* tab_ptr = nullptr;

  auto old_tab_iter = synced_tab_map_[local_session_tag_].find(old_tab_id);
  if (old_tab_id != TabNodePool::kInvalidTabID &&
      old_tab_iter != synced_tab_map_[local_session_tag_].end()) {
    tab_ptr = old_tab_iter->second;
    // Remove the tab from the synced tab map under the old id.
    synced_tab_map_[local_session_tag_].erase(old_tab_iter);
  } else {
    // It's possible a placeholder is already in place for the new tab. If so,
    // reuse it, otherwise create a new one (which will default to unmapped).
    tab_ptr = GetTab(local_session_tag_, new_tab_id);
  }

  // If the old tab is unmapped, update the tab id under which it is indexed.
  auto unmapped_tabs_iter = unmapped_tabs_[local_session_tag_].find(old_tab_id);
  if (old_tab_id != TabNodePool::kInvalidTabID &&
      unmapped_tabs_iter != unmapped_tabs_[local_session_tag_].end()) {
    std::unique_ptr<sessions::SessionTab> tab =
        std::move(unmapped_tabs_iter->second);
    DCHECK_EQ(tab_ptr, tab.get());
    unmapped_tabs_[local_session_tag_].erase(unmapped_tabs_iter);
    unmapped_tabs_[local_session_tag_][new_tab_id] = std::move(tab);
  }

  // Update the tab id.
  if (old_tab_id != TabNodePool::kInvalidTabID) {
    DVLOG(1) << "Remapped tab " << old_tab_id << " with node " << tab_node_id
             << " to tab " << new_tab_id;
  } else {
    DVLOG(1) << "Mapped new tab node " << tab_node_id << " to tab "
             << new_tab_id;
  }
  tab_ptr->tab_id.set_id(new_tab_id);

  // Add the tab back into the tab map with the new id.
  synced_tab_map_[local_session_tag_][new_tab_id] = tab_ptr;
  GetSession(local_session_tag_)->tab_node_ids.insert(tab_node_id);
}

void SyncedSessionTracker::Clear() {
  // Cleanup unmapped tabs and windows.
  unmapped_windows_.clear();
  unmapped_tabs_.clear();

  // Delete SyncedSession objects (which also deletes all their windows/tabs).
  synced_session_map_.clear();

  // Get rid of our convenience maps (does not delete the actual Window/Tabs
  // themselves; they should have all been deleted above).
  synced_window_map_.clear();
  synced_tab_map_.clear();

  local_tab_pool_.Clear();
  local_session_tag_.clear();
}

}  // namespace sync_sessions
