// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SESSIONS_SESSION_TYPES_H_
#define CHROME_BROWSER_SESSIONS_SESSION_TYPES_H_
#pragma once

#include <string>
#include <vector>

#include "base/stl_util.h"
#include "base/string16.h"
#include "base/time.h"
#include "chrome/browser/sessions/session_id.h"
#include "content/public/common/page_transition_types.h"
#include "googleurl/src/gurl.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/rect.h"

class NavigationEntry;
class Profile;

// TabNavigation  -------------------------------------------------------------

// TabNavigation corresponds to the parts of NavigationEntry needed to restore
// the NavigationEntry during session restore and tab restore.
//
// TabNavigation is cheap and supports copy semantics.
class TabNavigation {
 public:
  enum TypeMask {
    HAS_POST_DATA = 1
  };

  TabNavigation();
  TabNavigation(int index,
                const GURL& virtual_url,
                const GURL& referrer,
                const string16& title,
                const std::string& state,
                content::PageTransition transition);
  TabNavigation(const TabNavigation& tab);
  ~TabNavigation();
  TabNavigation& operator=(const TabNavigation& tab);

  // Converts this TabNavigation into a NavigationEntry with a page id of
  // |page_id|. The caller owns the returned NavigationEntry.
  NavigationEntry* ToNavigationEntry(int page_id, Profile* profile) const;

  // Resets this TabNavigation from |entry|.
  void SetFromNavigationEntry(const NavigationEntry& entry);

  // Virtual URL of the page. See NavigationEntry::virtual_url() for details.
  void set_virtual_url(const GURL& url) { virtual_url_ = url; }
  const GURL& virtual_url() const { return virtual_url_; }

  // The referrer.
  const GURL& referrer() const { return referrer_; }

  // The title of the page.
  void set_title(const string16& title) { title_ = title; }
  const string16& title() const { return title_; }

  // State bits.
  const std::string& state() const { return state_; }

  // Transition type.
  void set_transition(content::PageTransition transition) {
    transition_ = transition;
  }
  content::PageTransition transition() const { return transition_; }

  // A mask used for arbitrary boolean values needed to represent a
  // NavigationEntry. Currently only contains HAS_POST_DATA or 0.
  void set_type_mask(int type_mask) { type_mask_ = type_mask; }
  int type_mask() const { return type_mask_; }

  // The index in the NavigationController. If this is -1, it means this
  // TabNavigation is bogus.
  //
  // This is used when determining the selected TabNavigation and only useful
  // by BaseSessionService and SessionService.
  void set_index(int index) { index_ = index; }
  int index() const { return index_; }

  // Converts a set of TabNavigations into a set of NavigationEntrys. The
  // caller owns the NavigationEntrys.
  static void CreateNavigationEntriesFromTabNavigations(
      Profile* profile,
      const std::vector<TabNavigation>& navigations,
      std::vector<NavigationEntry*>* entries);

 private:
  friend class BaseSessionService;

  GURL virtual_url_;
  GURL referrer_;
  string16 title_;
  std::string state_;
  content::PageTransition transition_;
  int type_mask_;

  int index_;
};

// SessionTab ----------------------------------------------------------------

// SessionTab corresponds to a NavigationController.
struct SessionTab {
  SessionTab();
  ~SessionTab();

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

  // Timestamp for when this tab was last modified.
  base::Time timestamp;

  std::vector<TabNavigation> navigations;

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

 private:
  DISALLOW_COPY_AND_ASSIGN(SessionWindow);
};

#endif  // CHROME_BROWSER_SESSIONS_SESSION_TYPES_H_
