// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/foreign_session_tracker.h"
#include "chrome/browser/sync/glue/session_model_associator.h"

namespace browser_sync {


ForeignSessionTracker::ForeignSessionTracker() {
}

ForeignSessionTracker::~ForeignSessionTracker() {
  clear();
}

bool ForeignSessionTracker::LookupAllForeignSessions(
    std::vector<const ForeignSession*>* sessions) {
  DCHECK(sessions);
  // Fill vector of sessions from our foreign session map.
  for (ForeignSessionMap::const_iterator i =
    foreign_session_map_.begin(); i != foreign_session_map_.end(); ++i) {
    // Only include sessions with open tabs.
    ForeignSession* foreign_session = i->second;
    if (!foreign_session->windows.empty() &&
        !SessionModelAssociator::SessionWindowHasNoTabsToSync(
            *foreign_session->windows[0])) {
      sessions->push_back(foreign_session);
    }
  }

  return !sessions->empty();
}

bool ForeignSessionTracker::LookupSessionWindows(
    const std::string& foreign_session_tag,
    std::vector<SessionWindow*>* windows) {
  DCHECK(windows);
  ForeignSessionMap::iterator iter = foreign_session_map_.find(
      foreign_session_tag);
  if (iter == foreign_session_map_.end())
    return false;
  *windows = iter->second->windows;
  return true;
}

bool ForeignSessionTracker::LookupSessionTab(
    const std::string& tag,
    SessionID::id_type tab_id,
    const SessionTab** tab) {
  DCHECK(tab);
  if (foreign_tab_map_.find(tag) == foreign_tab_map_.end()) {
    // We have no record of this session.
    *tab = NULL;
    return false;
  }
  if (foreign_tab_map_[tag]->find(tab_id) == foreign_tab_map_[tag]->end()) {
    // We have no record of this tab.
    *tab = NULL;
    return false;
  }
  *tab = (*foreign_tab_map_[tag])[tab_id];
  return true;
}

ForeignSession* ForeignSessionTracker::GetForeignSession(
    const std::string& foreign_session_tag) {
  scoped_ptr<ForeignSession> foreign_session;
  if (foreign_session_map_.find(foreign_session_tag) !=
      foreign_session_map_.end()) {
    foreign_session.reset(foreign_session_map_[foreign_session_tag]);
  } else {
    foreign_session.reset(new ForeignSession);
    foreign_session->foreign_session_tag = foreign_session_tag;
    foreign_session_map_[foreign_session_tag] = foreign_session.get();
  }
  DCHECK(foreign_session.get());
  return foreign_session.release();
}

bool ForeignSessionTracker::DeleteForeignSession(
    const std::string& foreign_session_tag) {
    ForeignSessionMap::iterator iter =
        foreign_session_map_.find(foreign_session_tag);
  if (iter != foreign_session_map_.end()) {
    delete iter->second;  // Delete the ForeignSession object.
    foreign_session_map_.erase(iter);
    return true;
  } else {
    return false;
  }
}

SessionTab* ForeignSessionTracker::GetSessionTab(
    const std::string& foreign_session_tag,
    SessionID::id_type tab_id,
    bool has_window) {
  if (foreign_tab_map_.find(foreign_session_tag) == foreign_tab_map_.end())
    foreign_tab_map_[foreign_session_tag] = new IDToSessionTabMap;
  scoped_ptr<SessionTab> tab;
  IDToSessionTabMap::iterator iter =
      foreign_tab_map_[foreign_session_tag]->find(tab_id);
  if (iter != foreign_tab_map_[foreign_session_tag]->end()) {
    tab.reset(iter->second);
    if (has_window)  // This tab is linked to a window, so it's not an orphan.
      unmapped_tabs_.erase(tab.get());
    VLOG(1) << "Associating " << foreign_session_tag << "'s seen tab " <<
        tab_id  << " at " << tab.get();
  } else {
    tab.reset(new SessionTab());
    (*foreign_tab_map_[foreign_session_tag])[tab_id] = tab.get();
    if (!has_window)  // This tab is not linked to a window, it's an orphan.
    unmapped_tabs_.insert(tab.get());
    VLOG(1) << "Associating " << foreign_session_tag << "'s new tab " <<
        tab_id  << " at " << tab.get();
  }
  DCHECK(tab.get());
  return tab.release();
}

void ForeignSessionTracker::clear() {
  // Delete ForeignSession objects (which also deletes all their windows/tabs).
  STLDeleteContainerPairSecondPointers(foreign_session_map_.begin(),
      foreign_session_map_.end());
  foreign_session_map_.clear();

  // Delete IDToSessTab maps. Does not delete the SessionTab objects, because
  // they should already be referenced through foreign_session_map_.
  STLDeleteContainerPairSecondPointers(foreign_tab_map_.begin(),
      foreign_tab_map_.end());
  foreign_tab_map_.clear();

  // Go through and delete any tabs we had allocated but had not yet placed into
  // a ForeignSessionobject.
  STLDeleteContainerPointers(unmapped_tabs_.begin(), unmapped_tabs_.end());
  unmapped_tabs_.clear();
}

}  // namespace browser_sync
