// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/session_types.h"

#include "base/string_util.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/tab_contents/navigation_controller.h"
#include "chrome/browser/tab_contents/navigation_entry.h"

// TabNavigation --------------------------------------------------------------

// static
NavigationEntry* TabNavigation::ToNavigationEntry(int page_id,
                                                  Profile *profile) const {
  NavigationEntry* entry = NavigationController::CreateNavigationEntry(
      virtual_url_,
      referrer_,
      // Use a transition type of reload so that we don't incorrectly
      // increase the typed count.
      PageTransition::RELOAD,
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

// SessionWindow ---------------------------------------------------------------

SessionWindow::SessionWindow()
    : selected_tab_index(-1),
      type(Browser::TYPE_NORMAL),
      is_constrained(true),
      is_maximized(false) {
}

SessionWindow::~SessionWindow() {
  STLDeleteElements(&tabs);
}

// ForeignSession --------------------------------------------------------------

ForeignSession::ForeignSession() : foreign_tession_tag("invalid") {
}

ForeignSession::~ForeignSession() {
  STLDeleteElements(&windows);
}

