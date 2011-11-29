// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/sync/glue/synced_session_tracker.h"

namespace browser_sync {

SyncedSessionTracker::SyncedSessionTracker() {
}

SyncedSessionTracker::~SyncedSessionTracker() {
  Clear();
}

void SyncedSessionTracker::SetLocalSessionTag(
    const std::string& local_session_tag) {
  local_session_tag_ = local_session_tag;
}

bool SyncedSessionTracker::LookupAllForeignSessions(
    std::vector<const SyncedSession*>* sessions) const {
  DCHECK(sessions);
  sessions->clear();
  // Fill vector of sessions from our synced session map.
  for (SyncedSessionMap::const_iterator i =
    synced_session_map_.begin(); i != synced_session_map_.end(); ++i) {
    // Only include foreign sessions with open tabs.
    SyncedSession* foreign_session = i->second;
    if (i->first != local_session_tag_ && !foreign_session->windows.empty()) {
      bool found_tabs = false;
      for (SyncedSession::SyncedWindowMap::const_iterator iter =
               foreign_session->windows.begin();
           iter != foreign_session->windows.end(); ++iter) {
        if (!SessionWindowHasNoTabsToSync(*(iter->second))) {
          found_tabs = true;
          break;
        }
      }
      if (found_tabs)
        sessions->push_back(foreign_session);
    }
  }

  return !sessions->empty();
}

bool SyncedSessionTracker::LookupSessionWindows(
    const std::string& session_tag,
    std::vector<const SessionWindow*>* windows) const {
  DCHECK(windows);
  windows->clear();
  SyncedSessionMap::const_iterator iter = synced_session_map_.find(session_tag);
  if (iter == synced_session_map_.end())
    return false;
  windows->clear();
  for (SyncedSession::SyncedWindowMap::const_iterator window_iter =
           iter->second->windows.begin();
       window_iter != iter->second->windows.end(); window_iter++) {
    windows->push_back(window_iter->second);
  }
  return true;
}

bool SyncedSessionTracker::LookupSessionTab(
    const std::string& tag,
    SessionID::id_type tab_id,
    const SessionTab** tab) const {
  DCHECK(tab);
  SyncedTabMap::const_iterator tab_map_iter = synced_tab_map_.find(tag);
  if (tab_map_iter == synced_tab_map_.end()) {
    // We have no record of this session.
    *tab = NULL;
    return false;
  }
  IDToSessionTabMap::const_iterator tab_iter =
      tab_map_iter->second.find(tab_id);
  if (tab_iter == tab_map_iter->second.end()) {
    // We have no record of this tab.
    *tab = NULL;
    return false;
  }
  *tab = tab_iter->second.tab_ptr;
  return true;
}

SyncedSession* SyncedSessionTracker::GetSession(
    const std::string& session_tag) {
  SyncedSession* synced_session = NULL;
  if (synced_session_map_.find(session_tag) !=
      synced_session_map_.end()) {
    synced_session = synced_session_map_[session_tag];
  } else {
    synced_session = new SyncedSession;
    DVLOG(1) << "Creating new session with tag " << session_tag << " at "
             << synced_session;
    synced_session->session_tag = session_tag;
    synced_session_map_[session_tag] = synced_session;
  }
  DCHECK(synced_session);
  return synced_session;
}

bool SyncedSessionTracker::DeleteSession(const std::string& session_tag) {
  bool found_session = false;
  SyncedSessionMap::iterator iter = synced_session_map_.find(session_tag);
  if (iter != synced_session_map_.end()) {
    SyncedSession* session = iter->second;
    synced_session_map_.erase(iter);
    delete session;  // Delete the SyncedSession object.
    found_session = true;
  }
  synced_window_map_.erase(session_tag);
  // It's possible there was no header node but there were tab nodes.
  if (synced_tab_map_.erase(session_tag) > 0) {
    found_session = true;
  }
  return found_session;
}

void SyncedSessionTracker::ResetSessionTracking(
    const std::string& session_tag) {
  // Reset window tracking.
  GetSession(session_tag)->windows.clear();
  SyncedWindowMap::iterator window_iter = synced_window_map_.find(session_tag);
  if (window_iter != synced_window_map_.end()) {
    for (IDToSessionWindowMap::iterator window_map_iter =
             window_iter->second.begin();
         window_map_iter != window_iter->second.end(); ++window_map_iter) {
      window_map_iter->second.owned = false;
      // We clear out the tabs to prevent double referencing of the same tab.
      // All tabs that are in use will be added back as needed.
      window_map_iter->second.window_ptr->tabs.clear();
    }
  }

  // Reset tab tracking.
  SyncedTabMap::iterator tab_iter = synced_tab_map_.find(session_tag);
  if (tab_iter != synced_tab_map_.end()) {
    for (IDToSessionTabMap::iterator tab_map_iter =
             tab_iter->second.begin();
         tab_map_iter != tab_iter->second.end(); ++tab_map_iter) {
      tab_map_iter->second.owned = false;
    }
  }
}

bool SyncedSessionTracker::DeleteOldSessionWindowIfNecessary(
    SessionWindowWrapper window_wrapper) {
   // Clear the tabs first, since we don't want the destructor to destroy
   // them. Their deletion will be handled by DeleteOldSessionTab below.
  if (!window_wrapper.owned) {
    DVLOG(1) << "Deleting closed window "
             << window_wrapper.window_ptr->window_id.id();
    window_wrapper.window_ptr->tabs.clear();
    delete window_wrapper.window_ptr;
    return true;
  }
  return false;
}

bool SyncedSessionTracker::DeleteOldSessionTabIfNecessary(
    SessionTabWrapper tab_wrapper) {
  if (!tab_wrapper.owned) {
    if (VLOG_IS_ON(1)) {
      SessionTab* tab_ptr = tab_wrapper.tab_ptr;
      std::string title;
      if (tab_ptr->navigations.size() > 0) {
        title = " (" + UTF16ToUTF8(
            tab_ptr->navigations[tab_ptr->navigations.size()-1].title()) + ")";
      }
      DVLOG(1) << "Deleting closed tab " << tab_ptr->tab_id.id() << title
               << " from window " << tab_ptr->window_id.id();
    }
    unmapped_tabs_.erase(tab_wrapper.tab_ptr);
    delete tab_wrapper.tab_ptr;
    return true;
  }
  return false;
}

void SyncedSessionTracker::CleanupSession(const std::string& session_tag) {
  // Go through and delete any windows or tabs without owners.
  SyncedWindowMap::iterator window_iter = synced_window_map_.find(session_tag);
  if (window_iter != synced_window_map_.end()) {
    for (IDToSessionWindowMap::iterator iter = window_iter->second.begin();
         iter != window_iter->second.end();) {
      SessionWindowWrapper window_wrapper = iter->second;
      if (DeleteOldSessionWindowIfNecessary(window_wrapper))
        window_iter->second.erase(iter++);
      else
        ++iter;
    }
  }

  SyncedTabMap::iterator tab_iter = synced_tab_map_.find(session_tag);
  if (tab_iter != synced_tab_map_.end()) {
    for (IDToSessionTabMap::iterator iter = tab_iter->second.begin();
         iter != tab_iter->second.end();) {
      SessionTabWrapper tab_wrapper = iter->second;
      if (DeleteOldSessionTabIfNecessary(tab_wrapper))
        tab_iter->second.erase(iter++);
      else
        ++iter;
    }
  }
}

void SyncedSessionTracker::PutWindowInSession(const std::string& session_tag,
                                              SessionID::id_type window_id) {
  SessionWindow* window_ptr = NULL;
  IDToSessionWindowMap::iterator iter =
      synced_window_map_[session_tag].find(window_id);
  if (iter != synced_window_map_[session_tag].end()) {
    iter->second.owned = true;
    window_ptr = iter->second.window_ptr;
    DVLOG(1) << "Putting seen window " << window_id  << " at " << window_ptr
             << "in " << (session_tag == local_session_tag_ ?
                          "local session" : session_tag);
  } else {
    // Create the window.
    window_ptr = new SessionWindow();
    window_ptr->window_id.set_id(window_id);
    synced_window_map_[session_tag][window_id] =
        SessionWindowWrapper(window_ptr, true);
    DVLOG(1) << "Putting new window " << window_id  << " at " << window_ptr
             << "in " << (session_tag == local_session_tag_ ?
                          "local session" : session_tag);
  }
  DCHECK(window_ptr);
  DCHECK_EQ(window_ptr->window_id.id(), window_id);
  DCHECK_EQ((SessionWindow*)NULL, GetSession(session_tag)->windows[window_id]);
  GetSession(session_tag)->windows[window_id] = window_ptr;
}

void SyncedSessionTracker::PutTabInWindow(const std::string& session_tag,
                                          SessionID::id_type window_id,
                                          SessionID::id_type tab_id,
                                          size_t tab_index) {
  SessionTab* tab_ptr = GetTab(session_tag, tab_id);
  unmapped_tabs_.erase(tab_ptr);
  synced_tab_map_[session_tag][tab_id].owned = true;
  tab_ptr->window_id.set_id(window_id);
  DVLOG(1) << "  - tab " << tab_id << " added to window "<< window_id;
  DCHECK(GetSession(session_tag)->windows.find(window_id) !=
         GetSession(session_tag)->windows.end());
  std::vector<SessionTab*>& window_tabs =
      GetSession(session_tag)->windows[window_id]->tabs;
  if (window_tabs.size() <= tab_index) {
    window_tabs.resize(tab_index+1, NULL);
  }
  DCHECK_EQ((SessionTab*)NULL, window_tabs[tab_index]);
  window_tabs[tab_index] = tab_ptr;
}

SessionTab* SyncedSessionTracker::GetTab(
    const std::string& session_tag,
    SessionID::id_type tab_id) {
  SessionTab* tab_ptr = NULL;
  IDToSessionTabMap::iterator iter =
      synced_tab_map_[session_tag].find(tab_id);
  if (iter != synced_tab_map_[session_tag].end()) {
    tab_ptr = iter->second.tab_ptr;
    if (VLOG_IS_ON(1)) {
      std::string title;
      if (tab_ptr->navigations.size() > 0) {
        title = " (" + UTF16ToUTF8(
            tab_ptr->navigations[tab_ptr->navigations.size()-1].title()) + ")";
      }
      DVLOG(1) << "Getting "
               << (session_tag == local_session_tag_ ?
                   "local session" : session_tag)
               << "'s seen tab " << tab_id  << " at " << tab_ptr << title;
    }
  } else {
    tab_ptr = new SessionTab();
    tab_ptr->tab_id.set_id(tab_id);
    synced_tab_map_[session_tag][tab_id] = SessionTabWrapper(tab_ptr, false);
    unmapped_tabs_.insert(tab_ptr);
    DVLOG(1) << "Getting "
             << (session_tag == local_session_tag_ ?
                 "local session" : session_tag)
             << "'s new tab " << tab_id  << " at " << tab_ptr;
  }
  DCHECK(tab_ptr);
  DCHECK_EQ(tab_ptr->tab_id.id(), tab_id);
  return tab_ptr;
}

void SyncedSessionTracker::Clear() {
  // Delete SyncedSession objects (which also deletes all their windows/tabs).
  STLDeleteContainerPairSecondPointers(synced_session_map_.begin(),
      synced_session_map_.end());
  synced_session_map_.clear();

  // Go through and delete any tabs we had allocated but had not yet placed into
  // a SyncedSessionobject.
  STLDeleteContainerPointers(unmapped_tabs_.begin(), unmapped_tabs_.end());
  unmapped_tabs_.clear();

  // Get rid of our Window/Tab maps (does not delete the actual Window/Tabs
  // themselves; they should have all been deleted above).
  synced_window_map_.clear();
  synced_tab_map_.clear();

  local_session_tag_.clear();
}

}  // namespace browser_sync
