// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SESSIONS_SESSION_TYPES_H_
#define CHROME_BROWSER_SESSIONS_SESSION_TYPES_H_

#include <algorithm>
#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "base/time.h"
#include "chrome/browser/sessions/session_id.h"
#include "content/public/common/page_transition_types.h"
#include "content/public/common/referrer.h"
#include "googleurl/src/gurl.h"
#include "sync/protocol/session_specifics.pb.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/rect.h"

class Pickle;
class PickleIterator;

namespace content {
class BrowserContext;
class NavigationEntry;
}

// TabNavigation  -------------------------------------------------------------

// TabNavigation is a "freeze-dried" version of NavigationEntry.  It
// contains the data needed to restore a NavigationEntry during
// session restore and tab restore, and it can also be pickled and
// unpickled.  It is also convertible to a sync protocol buffer for
// session syncing.
//
// Default copy constructor and assignment operator welcome.
class TabNavigation {
 public:
  // Creates an invalid (index < 0) TabNavigation.
  TabNavigation();
  ~TabNavigation();

  // Construct a TabNavigation for a particular index from the given
  // NavigationEntry.
  static TabNavigation FromNavigationEntry(
      int index,
      const content::NavigationEntry& entry);

  // Construct a TabNavigation for a particular index from a sync
  // protocol buffer.  Note that the sync protocol buffer doesn't
  // contain all TabNavigation fields.  Also, the timestamp of the
  // returned TabNavigation is nulled out, as we assume that the
  // protocol buffer is from a foreign session.
  static TabNavigation FromSyncData(
      int index,
      const sync_pb::TabNavigation& sync_data);

  // Note that not all TabNavigation fields are preserved.
  void WriteToPickle(Pickle* pickle) const;
  bool ReadFromPickle(PickleIterator* iterator);

  // Convert this TabNavigation into a NavigationEntry with the given
  // page ID and context.  The NavigationEntry will have a transition
  // type of PAGE_TRANSITION_RELOAD and a new unique ID.
  scoped_ptr<content::NavigationEntry> ToNavigationEntry(
      int page_id,
      content::BrowserContext* browser_context) const;

  // Convert this navigation into its sync protocol buffer equivalent.
  // Note that the protocol buffer doesn't contain all TabNavigation
  // fields.
  sync_pb::TabNavigation ToSyncData() const;

  // The index in the NavigationController. This TabNavigation is
  // valid only when the index is non-negative.
  //
  // This is used when determining the selected TabNavigation and only
  // used by SessionService.
  int index() const { return index_; }
  void set_index(int index) { index_ = index; }

  // Accessors for some fields taken from NavigationEntry.
  int unique_id() const { return unique_id_; }
  const GURL& virtual_url() const { return virtual_url_; }
  const string16& title() const { return title_; }
  const std::string& content_state() const { return content_state_; }
  const string16& search_terms() const { return search_terms_; }
  const GURL& favicon_url() const { return favicon_url_; }

  // Converts a set of TabNavigations into a list of NavigationEntrys
  // with sequential page IDs and the given context. The caller owns
  // the returned NavigationEntrys.
  static std::vector<content::NavigationEntry*>
  CreateNavigationEntriesFromTabNavigations(
      const std::vector<TabNavigation>& navigations,
      content::BrowserContext* browser_context);

 private:
  friend struct SessionTypesTestHelper;

  // Index in the NavigationController.
  int index_;

  // Member variables corresponding to NavigationEntry fields.
  int unique_id_;
  content::Referrer referrer_;
  GURL virtual_url_;
  string16 title_;
  std::string content_state_;
  content::PageTransition transition_type_;
  bool has_post_data_;
  int64 post_id_;
  GURL original_request_url_;
  bool is_overriding_user_agent_;
  base::Time timestamp_;
  string16 search_terms_;
  GURL favicon_url_;
};

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
  // timestamp.  Uses TabNavigation::FromSyncData to fill
  // |navigations|.  Note that the sync protocol buffer doesn't
  // contain all TabNavigation fields.
  void SetFromSyncData(const sync_pb::SessionTab& sync_data,
                       base::Time timestamp);

  // Convert this object into its sync protocol buffer equivalent.
  // Uses TabNavigation::ToSyncData to convert |navigations|.  Note
  // that the protocol buffer doesn't contain all TabNavigation
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
  // NOTE: when the service is creating SessionTabs, initially this
  // corresponds to TabNavigation.index, not the index in navigations. When done
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

  std::vector<TabNavigation> navigations;

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
