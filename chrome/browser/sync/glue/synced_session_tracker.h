// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_SYNCED_SESSION_TRACKER_H_
#define CHROME_BROWSER_SYNC_GLUE_SYNCED_SESSION_TRACKER_H_
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

// Class to manage synced sessions. The tracker will own all SyncedSession
// and SessionTab objects it creates, and deletes them appropriately on
// destruction.
// Note: SyncedSession objects are created for all synced sessions, including
// the local session (whose tag we maintain separately).
class SyncedSessionTracker {
 public:
  SyncedSessionTracker();
  ~SyncedSessionTracker();

  // We track and distinguish the local session from foreign sessions.
  void SetLocalSessionTag(const std::string& local_session_tag);

  // Fill a preallocated vector with all foreign sessions we're tracking (skips
  // the local session object).
  // Returns true if we had foreign sessions to fill it with, false otherwise.
  bool LookupAllForeignSessions(std::vector<const SyncedSession*>* sessions);

  // Attempts to look up the session windows associatd with the session given
  // by |session_tag|.
  // If lookup succeeds:
  // - Fills windows with the SessionWindow pointers, returns true.
  // Else
  // - Returns false.
  bool LookupSessionWindows(const std::string& session_tag,
                            std::vector<SessionWindow*>* windows);

  // Attempts to look up the tab associated with the given tag and tab id.
  // If lookup succeeds:
  // - Sets tab to point to the SessionTab, and returns true.
  // Else
  // - Returns false, tab is set to NULL.
  bool LookupSessionTab(const std::string& session_tag,
                        SessionID::id_type tab_id,
                        const SessionTab** tab);

  // Returns a pointer to the SyncedSession object associated with session_tag.
  // If none exists, creates one and returns its pointer.
  SyncedSession* GetSession(const std::string& session_tag);

  // Deletes the session associated with |session_tag| if it exists.
  // Returns true if the session existed and was deleted, false otherwise.
  bool DeleteSession(const std::string& session_tag);

  // Returns a pointer to the SessionTab object associated with |tab_id| for
  // the session specified with |session_tag|. If none exists, creates one and
  // returns its pointer.
  // |has_window| determines if newly created tabs are added to the pool of
  // orphaned tabs (those which can't be reached by traversing sessions).
  SessionTab* GetSessionTab(const std::string& session_tag,
                            SessionID::id_type tab_id,
                            bool has_window);

  // Free the memory for all dynamically allocated objects and clear the
  // tracking structures.
  void clear();

  inline bool empty() {
    return synced_tab_map_.empty() && synced_session_map_.empty();
  }

  // Includes both foreign sessions and the local session.
  inline size_t num_synced_sessions() {
    return synced_session_map_.size();
  }

  // Returns the number of tabs associated with the specified session tag.
  inline size_t num_synced_tabs(const std::string& session_tag) {
    if (synced_tab_map_.find(session_tag) != synced_tab_map_.end()) {
      return synced_tab_map_[session_tag]->size();
    } else {
      return 0;
    }
  }
 private:
  // Datatypes for accessing session data.
  typedef std::map<SessionID::id_type, SessionTab*> IDToSessionTabMap;
  typedef std::map<std::string, IDToSessionTabMap*> SyncedTabMap;
  typedef std::map<std::string, SyncedSession*> SyncedSessionMap;

  // Per client mapping of tab id's to their SessionTab objects.
  // Key: session tag.
  // Value: Tab id to SessionTab map pointer.
  SyncedTabMap synced_tab_map_;

  // Per client mapping synced session objects.
  // Key: session tag.
  // Value: SyncedSession object pointer.
  SyncedSessionMap synced_session_map_;

  // The set of tabs that we have seen, and created SessionTab objects for, but
  // have not yet mapped to SyncedSessions. These are temporarily orphaned
  // tabs, and won't be deleted if we delete synced_session_map_.
  std::set<SessionTab*> unmapped_tabs_;

  // The tag for this machine's local session, so we can distinguish the foreign
  // sessions.
  std::string local_session_tag_;

  DISALLOW_COPY_AND_ASSIGN(SyncedSessionTracker);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_SYNCED_SESSION_TRACKER_H_
