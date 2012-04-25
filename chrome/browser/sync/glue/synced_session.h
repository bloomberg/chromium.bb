// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_SYNCED_SESSION_H_
#define CHROME_BROWSER_SYNC_GLUE_SYNCED_SESSION_H_
#pragma once

#include <map>
#include <string>

#include "base/time.h"
#include "chrome/browser/sessions/session_id.h"
#include "chrome/browser/sessions/session_types.h"

namespace content {
class NavigationEntry;
}

namespace browser_sync {

// Sync-specific wrapper around a normal TabNavigation.
// Copy semantics supported.
class SyncedTabNavigation : public TabNavigation {
 public:
  SyncedTabNavigation();
  SyncedTabNavigation(const SyncedTabNavigation& tab);
  SyncedTabNavigation(int index,
                      const GURL& virtual_url,
                      const content::Referrer& referrer,
                      const string16& title,
                      const std::string& state,
                      content::PageTransition transition,
                      int unique_id,
                      const base::Time& timestamp);
  virtual ~SyncedTabNavigation();

  // Unique id for this navigation.
  void set_unique_id(int unique_id);
  int unique_id() const;

  // Timestamp this navigation occurred.
  void set_timestamp(const base::Time& timestamp);
  base::Time timestamp() const;

 private:
  int unique_id_;
  base::Time timestamp_;
};

// Sync-specific wrapper around a normal SessionTab to support using
// SyncedTabNavigation.
struct SyncedSessionTab : public SessionTab {
 public:
  SyncedSessionTab();
  virtual ~SyncedSessionTab();

  std::vector<SyncedTabNavigation> synced_tab_navigations;
};

// Defines a synced session for use by session sync. A synced session is a
// list of windows along with a unique session identifer (tag) and meta-data
// about the device being synced.
struct SyncedSession {
  typedef std::map<SessionID::id_type, SessionWindow*> SyncedWindowMap;

  // The type of device.
  enum DeviceType {
    TYPE_UNSET = 0,
    TYPE_WIN = 1,
    TYPE_MACOSX = 2,
    TYPE_LINUX = 3,
    TYPE_CHROMEOS = 4,
    TYPE_OTHER = 5,
    TYPE_PHONE = 6,
    TYPE_TABLET = 7
  };

  SyncedSession();
  ~SyncedSession();

  // Unique tag for each session.
  std::string session_tag;
  // User-visible name
  std::string session_name;

  // Type of device this session is from.
  DeviceType device_type;

  // Last time this session was modified remotely.
  base::Time modified_time;

  // Map of windows that make up this session. Windowws are owned by the session
  // itself and free'd on destruction.
  SyncedWindowMap windows;

 private:
  DISALLOW_COPY_AND_ASSIGN(SyncedSession);
};

// Control which foreign tabs we're interested in syncing/displaying. Checks
// that the tab has navigations and contains at least one valid url.
// Note: chrome:// and file:// are not considered valid urls (for syncing).
bool ShouldSyncSessionTab(const SessionTab& tab);

// Checks whether the window has tabs to sync. If no tabs to sync, it returns
// true, false otherwise.
bool SessionWindowHasNoTabsToSync(const SessionWindow& window);

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_SYNCED_SESSION_H_
