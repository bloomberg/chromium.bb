// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/synced_session_tracker.h"
#include "chrome/browser/sync/glue/session_model_associator.h"

namespace browser_sync {


SyncedSessionTracker::SyncedSessionTracker() {
}

SyncedSessionTracker::~SyncedSessionTracker() {
  clear();
}

bool SyncedSessionTracker::LookupAllForeignSessions(
    std::vector<const SyncedSession*>* sessions) {
  DCHECK(sessions);
  // Fill vector of sessions from our synced session map.
  for (SyncedSessionMap::const_iterator i =
    synced_session_map_.begin(); i != synced_session_map_.end(); ++i) {
    // Only include foreign sessions with open tabs.
    SyncedSession* foreign_session = i->second;
    if (i->first != local_session_tag_ &&
        !foreign_session->windows.empty() &&
        !SessionModelAssociator::SessionWindowHasNoTabsToSync(
            *foreign_session->windows[0])) {
      sessions->push_back(foreign_session);
    }
  }

  return !sessions->empty();
}

bool SyncedSessionTracker::LookupSessionWindows(
    const std::string& session_tag,
    std::vector<SessionWindow*>* windows) {
  DCHECK(windows);
  SyncedSessionMap::iterator iter = synced_session_map_.find(session_tag);
  if (iter == synced_session_map_.end())
    return false;
  *windows = iter->second->windows;
  return true;
}

bool SyncedSessionTracker::LookupSessionTab(
    const std::string& tag,
    SessionID::id_type tab_id,
    const SessionTab** tab) {
  DCHECK(tab);
  if (synced_tab_map_.find(tag) == synced_tab_map_.end()) {
    // We have no record of this session.
    *tab = NULL;
    return false;
  }
  if (synced_tab_map_[tag]->find(tab_id) == synced_tab_map_[tag]->end()) {
    // We have no record of this tab.
    *tab = NULL;
    return false;
  }
  *tab = (*synced_tab_map_[tag])[tab_id];
  return true;
}

SyncedSession* SyncedSessionTracker::GetSession(
    const std::string& session_tag) {
  scoped_ptr<SyncedSession> synced_session;
  if (synced_session_map_.find(session_tag) !=
      synced_session_map_.end()) {
    synced_session.reset(synced_session_map_[session_tag]);
  } else {
    synced_session.reset(new SyncedSession);
    synced_session->session_tag = session_tag;
    synced_session_map_[session_tag] = synced_session.get();
  }
  DCHECK(synced_session.get());
  return synced_session.release();
}

bool SyncedSessionTracker::DeleteSession(
    const std::string& session_tag) {
    SyncedSessionMap::iterator iter =
        synced_session_map_.find(session_tag);
  if (iter != synced_session_map_.end()) {
    delete iter->second;  // Delete the SyncedSession object.
    synced_session_map_.erase(iter);
    return true;
  } else {
    return false;
  }
}

SessionTab* SyncedSessionTracker::GetSessionTab(
    const std::string& session_tag,
    SessionID::id_type tab_id,
    bool has_window) {
  if (synced_tab_map_.find(session_tag) == synced_tab_map_.end())
    synced_tab_map_[session_tag] = new IDToSessionTabMap;
  scoped_ptr<SessionTab> tab;
  IDToSessionTabMap::iterator iter =
      synced_tab_map_[session_tag]->find(tab_id);
  if (iter != synced_tab_map_[session_tag]->end()) {
    tab.reset(iter->second);
    if (has_window)  // This tab is linked to a window, so it's not an orphan.
      unmapped_tabs_.erase(tab.get());
    VLOG(1) << "Associating " << session_tag << "'s seen tab " <<
        tab_id  << " at " << tab.get();
  } else {
    tab.reset(new SessionTab());
    (*synced_tab_map_[session_tag])[tab_id] = tab.get();
    if (!has_window)  // This tab is not linked to a window, it's an orphan.
    unmapped_tabs_.insert(tab.get());
    VLOG(1) << "Associating " << session_tag << "'s new tab " <<
        tab_id  << " at " << tab.get();
  }
  DCHECK(tab.get());
  return tab.release();
}

void SyncedSessionTracker::clear() {
  // Delete SyncedSession objects (which also deletes all their windows/tabs).
  STLDeleteContainerPairSecondPointers(synced_session_map_.begin(),
      synced_session_map_.end());
  synced_session_map_.clear();

  // Delete IDToSessTab maps. Does not delete the SessionTab objects, because
  // they should already be referenced through synced_session_map_.
  STLDeleteContainerPairSecondPointers(synced_tab_map_.begin(),
      synced_tab_map_.end());
  synced_tab_map_.clear();

  // Go through and delete any tabs we had allocated but had not yet placed into
  // a SyncedSessionobject.
  STLDeleteContainerPointers(unmapped_tabs_.begin(), unmapped_tabs_.end());
  unmapped_tabs_.clear();
}

}  // namespace browser_sync
