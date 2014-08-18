// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SESSIONS_SESSION_TYPES_H_
#define CHROME_BROWSER_SESSIONS_SESSION_TYPES_H_

#include <algorithm>
#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "components/sessions/serialized_navigation_entry.h"
#include "components/sessions/session_id.h"
#include "content/public/common/page_transition_types.h"
#include "sync/protocol/session_specifics.pb.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/rect.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
class NavigationEntry;
}

// SessionTab ----------------------------------------------------------------

// SessionTab corresponds to a NavigationController.
struct SessionTab {
  SessionTab();
  ~SessionTab();

  // Since the current_navigation_index can be larger than the index for number
  // of navigations in the current sessions (chrome://newtab is not stored), we
  // must perform bounds checking.
  // Returns a normalized bounds-checked navigation_index.
  int normalized_navigation_index() const {
    return std::max(0, std::min(current_navigation_index,
                                static_cast<int>(navigations.size() - 1)));
  }

  // Set all the fields of this object from the given sync data and
  // timestamp.  Uses SerializedNavigationEntry::FromSyncData to fill
  // |navigations|.  Note that the sync protocol buffer doesn't
  // contain all SerializedNavigationEntry fields.
  void SetFromSyncData(const sync_pb::SessionTab& sync_data,
                       base::Time timestamp);

  // Convert this object into its sync protocol buffer equivalent.
  // Uses SerializedNavigationEntry::ToSyncData to convert |navigations|.  Note
  // that the protocol buffer doesn't contain all SerializedNavigationEntry
  // fields, and that the returned protocol buffer doesn't have any
  // favicon data.
  sync_pb::SessionTab ToSyncData() const;

  // Unique id of the window.
  SessionID window_id;

  // Unique if of the tab.
  SessionID tab_id;

  // Visual index of the tab within its window. There may be gaps in these
  // values.
  //
  // NOTE: this is really only useful for the SessionService during
  // restore, others can likely ignore this and use the order of the
  // tabs in SessionWindow.tabs.
  int tab_visual_index;

  // Identifies the index of the current navigation in navigations. For
  // example, if this is 2 it means the current navigation is navigations[2].
  //
  // NOTE: when the service is creating SessionTabs, initially this corresponds
  // to SerializedNavigationEntry.index, not the index in navigations. When done
  // creating though, this is set to the index in navigations.
  //
  // NOTE 2: this value can be larger than the size of |navigations|, due to
  // only valid url's being stored (ie chrome://newtab is not stored). Bounds
  // checking must be performed before indexing into |navigations|.
  int current_navigation_index;

  // True if the tab is pinned.
  bool pinned;

  // If non-empty, this tab is an app tab and this is the id of the extension.
  std::string extension_app_id;

  // If non-empty, this string is used as the user agent whenever the tab's
  // NavigationEntries need it overridden.
  std::string user_agent_override;

  // Timestamp for when this tab was last modified.
  base::Time timestamp;

  std::vector<sessions::SerializedNavigationEntry> navigations;

  // For reassociating sessionStorage.
  std::string session_storage_persistent_id;

 private:
  DISALLOW_COPY_AND_ASSIGN(SessionTab);
};

// SessionWindow -------------------------------------------------------------

// Describes a saved window.
struct SessionWindow {
  SessionWindow();
  ~SessionWindow();

  // Convert this object into its sync protocol buffer equivalent. Note that
  // not all fields are synced here, because they don't all make sense or
  // translate when restoring a SessionWindow on another device.
  sync_pb::SessionWindow ToSyncData() const;

  // Identifier of the window.
  SessionID window_id;

  // Bounds of the window.
  gfx::Rect bounds;

  // Index of the selected tab in tabs; -1 if no tab is selected. After restore
  // this value is guaranteed to be a valid index into tabs.
  //
  // NOTE: when the service is creating SessionWindows, initially this
  // corresponds to SessionTab.tab_visual_index, not the index in
  // tabs. When done creating though, this is set to the index in
  // tabs.
  int selected_tab_index;

  // Type of the browser. Currently we only store browsers of type
  // TYPE_TABBED and TYPE_POPUP.
  // This would be Browser::Type, but that would cause a circular dependency.
  int type;

  // If true, the window is constrained.
  //
  // Currently SessionService prunes all constrained windows so that session
  // restore does not attempt to restore them.
  bool is_constrained;

  // Timestamp for when this window was last modified.
  base::Time timestamp;

  // The tabs, ordered by visual order.
  std::vector<SessionTab*> tabs;

  // Is the window maximized, minimized, or normal?
  ui::WindowShowState show_state;

  std::string app_name;

 private:
  DISALLOW_COPY_AND_ASSIGN(SessionWindow);
};

#endif  // CHROME_BROWSER_SESSIONS_SESSION_TYPES_H_
