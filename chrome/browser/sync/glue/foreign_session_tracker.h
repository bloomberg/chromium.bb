// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_FOREIGN_SESSION_TRACKER_H_
#define CHROME_BROWSER_SYNC_GLUE_FOREIGN_SESSION_TRACKER_H_
#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_vector.h"
#include "chrome/browser/sessions/session_id.h"
#include "chrome/browser/sessions/session_types.h"

namespace browser_sync {

// Class to manage foreign sessions. The tracker will own all ForeignSession
// and SessionTab objects it creates, and deletes them appropriately on
// destruction.
class ForeignSessionTracker {
 public:
  ForeignSessionTracker();
  ~ForeignSessionTracker();

  // Fill a preallocated vector with all foreign sessions we're tracking.
  // Returns true if we had foreign sessions to fill it with, false otherwise.
  bool LookupAllForeignSessions(std::vector<const ForeignSession*>* sessions);

  // Attempts to look up the session windows associatd with the foreign session
  // given by |foreign_session_tag|.
  // If lookup succeeds:
  // - Fills windows with the SessionWindow pointers, returns true.
  // Else
  // - Returns false.
  bool LookupSessionWindows(const std::string& foreign_session_tag,
                            std::vector<SessionWindow*>* windows);

  // Attempts to look up the foreign tab associated with the given tag and tab
  // id.
  // If lookup succeeds:
  // - Sets tab to point to the SessionTab, and returns true.
  // Else
  // - Returns false, tab is set to NULL.
  bool LookupSessionTab(const std::string& foreign_session_tag,
                        SessionID::id_type tab_id,
                        const SessionTab** tab);

  // Returns a pointer to the ForeignSession object associated with
  // foreign_session_tag. If none exists, creates one and returns its pointer.
  ForeignSession* GetForeignSession(const std::string& foreign_session_tag);

  // Deletes the foreign session associated with |foreign_session_tag| if it
  // exists.
  // Returns true if the session existed and was deleted, false otherwise.
  bool DeleteForeignSession(const std::string& foreign_session_tag);

  // Returns a pointer to the SessionTab object associated with |tab_id| for
  // the session specified with |foreign_session_tag|. If none exists, creates
  // one and returns its pointer.
  // |has_window| determines if newly created tabs are added to the pool of
  // orphaned tabs (thos which can't be reached by traversing foreign sessions).
  SessionTab* GetSessionTab(const std::string& foreign_session_tag,
                            SessionID::id_type tab_id,
                            bool has_window);

  // Free the memory for all dynamically allocated objects and clear the
  // tracking structures.
  void clear();

  inline bool empty() {
    return foreign_tab_map_.empty() && foreign_session_map_.empty();
  }

  inline size_t num_foreign_sessions() {
    return foreign_session_map_.size();
  }

  inline size_t num_foreign_tabs(const std::string& foreign_session_tag) {
    if (foreign_tab_map_.find(foreign_session_tag) != foreign_tab_map_.end()) {
      return foreign_tab_map_[foreign_session_tag]->size();
    } else {
      return 0;
    }
  }
 private:
  // Datatypes for accessing foreign tab data.
  typedef std::map<SessionID::id_type, SessionTab*> IDToSessionTabMap;
  typedef std::map<std::string, IDToSessionTabMap*> ForeignTabMap;
  typedef std::map<std::string, ForeignSession*> ForeignSessionMap;

  // Per foreign client mapping of their tab id's to their SessionTab objects.
  ForeignTabMap foreign_tab_map_;

  // Map of foreign sessions, accessed by the foreign client id.
  ForeignSessionMap foreign_session_map_;

  // The set of foreign tabs that we have seen, and created SessionTab objects
  // for, but have not yet mapped to ForeignSessions. These are temporarily
  // orphaned tabs, and won't be deleted if we delete foreign_session_map_.
  std::set<SessionTab*> unmapped_tabs_;

  DISALLOW_COPY_AND_ASSIGN(ForeignSessionTracker);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_FOREIGN_SESSION_TRACKER_H_
