// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/session_types.h"

#include "base/string_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "content/browser/tab_contents/navigation_controller.h"
#include "content/browser/tab_contents/navigation_entry.h"

// TabNavigation --------------------------------------------------------------

TabNavigation::TabNavigation()
    : transition_(content::PAGE_TRANSITION_TYPED),
      type_mask_(0),
      index_(-1) {
}

TabNavigation::TabNavigation(int index,
                             const GURL& virtual_url,
                             const content::Referrer& referrer,
                             const string16& title,
                             const std::string& state,
                             content::PageTransition transition)
    : virtual_url_(virtual_url),
      referrer_(referrer),
      title_(title),
      state_(state),
      transition_(transition),
      type_mask_(0),
      index_(index) {
}

TabNavigation::TabNavigation(const TabNavigation& tab)
    : virtual_url_(tab.virtual_url_),
      referrer_(tab.referrer_),
      title_(tab.title_),
      state_(tab.state_),
      transition_(tab.transition_),
      type_mask_(tab.type_mask_),
      index_(tab.index_) {
}

TabNavigation::~TabNavigation() {
}

TabNavigation& TabNavigation::operator=(const TabNavigation& tab) {
  virtual_url_ = tab.virtual_url_;
  referrer_ = tab.referrer_;
  title_ = tab.title_;
  state_ = tab.state_;
  transition_ = tab.transition_;
  type_mask_ = tab.type_mask_;
  index_ = tab.index_;
  return *this;
}

// static
NavigationEntry* TabNavigation::ToNavigationEntry(int page_id,
                                                  Profile *profile) const {
  NavigationEntry* entry = NavigationController::CreateNavigationEntry(
      virtual_url_,
      referrer_,
      // Use a transition type of reload so that we don't incorrectly
      // increase the typed count.
      content::PAGE_TRANSITION_RELOAD,
      false,
      // The extra headers are not sync'ed across sessions.
      std::string(),
      profile);

  entry->set_page_id(page_id);
  entry->set_title(title_);
  entry->set_content_state(state_);
  entry->set_has_post_data(type_mask_ & TabNavigation::HAS_POST_DATA);

  return entry;
}

void TabNavigation::SetFromNavigationEntry(const NavigationEntry& entry) {
  virtual_url_ = entry.virtual_url();
  referrer_ = entry.referrer();
  title_ = entry.title();
  state_ = entry.content_state();
  transition_ = entry.transition_type();
  type_mask_ = entry.has_post_data() ? TabNavigation::HAS_POST_DATA : 0;
}

// static
void TabNavigation::CreateNavigationEntriesFromTabNavigations(
    Profile* profile,
    const std::vector<TabNavigation>& navigations,
    std::vector<NavigationEntry*>* entries) {
  int page_id = 0;
  for (std::vector<TabNavigation>::const_iterator i =
           navigations.begin(); i != navigations.end(); ++i, ++page_id) {
    entries->push_back(i->ToNavigationEntry(page_id, profile));
  }
}

// SessionTab -----------------------------------------------------------------

SessionTab::SessionTab()
    : tab_visual_index(-1),
      current_navigation_index(-1),
      pinned(false) {
}

SessionTab::~SessionTab() {
}

// SessionWindow ---------------------------------------------------------------

SessionWindow::SessionWindow()
    : selected_tab_index(-1),
      type(Browser::TYPE_TABBED),
      is_constrained(true),
      show_state(ui::SHOW_STATE_DEFAULT) {
}

SessionWindow::~SessionWindow() {
  STLDeleteElements(&tabs);
}
