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
#include "chrome/browser/sync/glue/synced_session.h"

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
  // the local session object). SyncedSession ownership remains within the
  // SyncedSessionTracker.
  // Returns true if we had foreign sessions to fill it with, false otherwise.
  bool LookupAllForeignSessions(std::vector<const SyncedSession*>* sessions)
      const;

  // Attempts to look up the session windows associatd with the session given
  // by |session_tag|. Ownership Of SessionWindows stays within the
  // SyncedSessionTracker.
  // If lookup succeeds:
  // - Fills windows with the SessionWindow pointers, returns true.
  // Else
  // - Returns false.
  bool LookupSessionWindows(const std::string& session_tag,
                            std::vector<const SessionWindow*>* windows) const;

  // Attempts to look up the tab associated with the given tag and tab id.
  // Ownership of the SessionTab remains within the SyncedSessionTracker.
  // If lookup succeeds:
  // - Sets tab to point to the SessionTab, and returns true.
  // Else
  // - Returns false, tab is set to NULL.
  bool LookupSessionTab(const std::string& session_tag,
                        SessionID::id_type tab_id,
                        const SessionTab** tab) const;

  // Returns a pointer to the SyncedSession object associated with
  // |session_tag|.  If none exists, creates one. Ownership of the
  // SyncedSession remains within the SyncedSessionTracker.
  SyncedSession* GetSession(const std::string& session_tag);

  // Deletes the session associated with |session_tag| if it exists.
  // Returns true if the session existed and was deleted, false otherwise.
  bool DeleteSession(const std::string& session_tag);

  // Resets the tracking information for the session specified by |session_tag|.
  // This involves clearing all the windows and tabs from the session, while
  // keeping pointers saved in the synced_window_map_ and synced_tab_map_.
  // Once reset, all calls to PutWindowInSession and PutTabInWindow will denote
  // that the requested windows and tabs are owned (by setting the boolean
  // in their SessionWindowWrapper/SessionTabWrapper to true) and add them back
  // to their session. The next call to CleanupSession(...) will delete those
  // windows and tabs not owned.
  void ResetSessionTracking(const std::string& session_tag);

  // Deletes those windows and tabs associated with |session_tag| that are no
  // longer owned.
  // See ResetSessionTracking(...).
  void CleanupSession(const std::string& session_tag);

  // Adds the window with id |window_id| to the session specified by
  // |session_tag|, and markes the window as being owned. If none existed for
  // that session, creates one. Similarly, if the session did not exist yet,
  // creates it. Ownership of the SessionWindow remains within the
  // SyncedSessionTracker.
  void PutWindowInSession(const std::string& session_tag,
                          SessionID::id_type window_id);

  // Adds the tab with id |tab_id| to the window |window_id|, and marks it as
  // being owned. If none existed for that session, creates one. Ownership of
  // the SessionTab remains within the SyncedSessionTracker.
  // Note: GetSession(..) must have already been called with |session_tag| to
  // ensure we having mapping information for this session.
  void PutTabInWindow(const std::string& session_tag,
                      SessionID::id_type window_id,
                      SessionID::id_type tab_id,
                      size_t tab_index);

  // Returns a pointer to the SessionTab object associated with |tab_id| for
  // the session specified with |session_tag|. If none exists, creates one.
  // Ownership of the SessionTab remains within the SyncedSessionTracker.
  SessionTab* GetTab(const std::string& session_tag,
                     SessionID::id_type tab_id);

  // Free the memory for all dynamically allocated objects and clear the
  // tracking structures.
  void Clear();

  bool Empty() const {
    return synced_tab_map_.empty() && synced_session_map_.empty();
  }

  // Includes both foreign sessions and the local session.
  size_t num_synced_sessions() const {
    return synced_session_map_.size();
  }

  // Returns the number of tabs associated with the specified session tag.
  size_t num_synced_tabs(const std::string& session_tag) const {
    SyncedTabMap::const_iterator iter = synced_tab_map_.find(session_tag);
    if (iter != synced_tab_map_.end()) {
      return iter->second.size();
    } else {
      return 0;
    }
  }
 private:
  // Datatypes for accessing session data. Neither of the *Wrappers actually
  // have ownership of the Windows/Tabs, they just provide id-based access to
  // them. The ownership remains within it's containing session (for windows and
  // mapped tabs, unmapped tabs are owned by the unmapped_tabs_ container).
  // Note, we pair pointers with bools so that we can track what is owned and
  // what can be deleted (see ResetSessionTracking(..) and CleanupSession(..)
  // above).
  struct SessionTabWrapper {
    SessionTabWrapper() : tab_ptr(NULL), owned(false) {}
    SessionTabWrapper(SessionTab* tab_ptr, bool owned)
        : tab_ptr(tab_ptr),
          owned(owned) {}
    SessionTab* tab_ptr;
    bool owned;
  };
  typedef std::map<SessionID::id_type, SessionTabWrapper> IDToSessionTabMap;
  typedef std::map<std::string, IDToSessionTabMap> SyncedTabMap;

  struct SessionWindowWrapper {
    SessionWindowWrapper() : window_ptr(NULL), owned(false) {}
    SessionWindowWrapper(SessionWindow* window_ptr, bool owned)
        : window_ptr(window_ptr),
          owned(owned) {}
    SessionWindow* window_ptr;
    bool owned;
  };
  typedef std::map<SessionID::id_type, SessionWindowWrapper>
      IDToSessionWindowMap;
  typedef std::map<std::string, IDToSessionWindowMap> SyncedWindowMap;

  typedef std::map<std::string, SyncedSession*> SyncedSessionMap;

  // Helper methods for deleting SessionWindows and SessionTabs without owners.
  bool DeleteOldSessionWindowIfNecessary(SessionWindowWrapper window_wrapper);
  bool DeleteOldSessionTabIfNecessary(SessionTabWrapper tab_wrapper);

  // Per client mapping of tab id's to their SessionTab objects.
  // Key: session tag.
  // Value: Tab id to SessionTabWrapper map.
  SyncedTabMap synced_tab_map_;

  // Per client mapping of the window id's to their SessionWindow objects.
  // Key: session_tag
  // Value: Window id to SessionWindowWrapper map.
  SyncedWindowMap synced_window_map_;

  // Per client mapping synced session objects.
  // Key: session tag.
  // Value: SyncedSession object pointer.
  SyncedSessionMap synced_session_map_;

  // The set of tabs that we have seen, and created SessionTab objects for, but
  // have not yet mapped to SyncedSessions. These are temporarily orphaned
  // tabs, and won't be deleted if we delete synced_session_map_, but are still
  // owned by the SyncedSessionTracker itself (and deleted on Clear()).
  std::set<SessionTab*> unmapped_tabs_;

  // The tag for this machine's local session, so we can distinguish the foreign
  // sessions.
  std::string local_session_tag_;

  DISALLOW_COPY_AND_ASSIGN(SyncedSessionTracker);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_SYNCED_SESSION_TRACKER_H_
