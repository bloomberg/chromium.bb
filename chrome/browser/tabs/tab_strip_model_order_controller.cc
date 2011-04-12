// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tabs/tab_strip_model_order_controller.h"

#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"

///////////////////////////////////////////////////////////////////////////////
// TabStripModelOrderController, public:

TabStripModelOrderController::TabStripModelOrderController(
    TabStripModel* tabstrip)
    : tabstrip_(tabstrip),
      insertion_policy_(TabStripModel::INSERT_AFTER) {
  tabstrip_->AddObserver(this);
}

TabStripModelOrderController::~TabStripModelOrderController() {
  tabstrip_->RemoveObserver(this);
}

int TabStripModelOrderController::DetermineInsertionIndex(
    TabContentsWrapper* new_contents,
    PageTransition::Type transition,
    bool foreground) {
  int tab_count = tabstrip_->count();
  if (!tab_count)
    return 0;

  // NOTE: TabStripModel enforces that all non-mini-tabs occur after mini-tabs,
  // so we don't have to check here too.
  if (transition == PageTransition::LINK && tabstrip_->active_index() != -1) {
    int delta = (insertion_policy_ == TabStripModel::INSERT_AFTER) ? 1 : 0;
    if (foreground) {
      // If the page was opened in the foreground by a link click in another
      // tab, insert it adjacent to the tab that opened that link.
      return tabstrip_->active_index() + delta;
    }
    NavigationController* opener =
        &tabstrip_->GetSelectedTabContents()->controller();
    // Get the index of the next item opened by this tab, and insert after
    // it...
    int index;
    if (insertion_policy_ == TabStripModel::INSERT_AFTER) {
      index = tabstrip_->GetIndexOfLastTabContentsOpenedBy(
          opener, tabstrip_->active_index());
    } else {
      index = tabstrip_->GetIndexOfFirstTabContentsOpenedBy(
          opener, tabstrip_->active_index());
    }
    if (index != TabStripModel::kNoTab)
      return index + delta;
    // Otherwise insert adjacent to opener...
    return tabstrip_->active_index() + delta;
  }
  // In other cases, such as Ctrl+T, open at the end of the strip.
  return DetermineInsertionIndexForAppending();
}

int TabStripModelOrderController::DetermineInsertionIndexForAppending() {
  return (insertion_policy_ == TabStripModel::INSERT_AFTER) ?
      tabstrip_->count() : 0;
}

int TabStripModelOrderController::DetermineNewSelectedIndex(
    int removing_index) const {
  int tab_count = tabstrip_->count();
  DCHECK(removing_index >= 0 && removing_index < tab_count);
  NavigationController* parent_opener =
      tabstrip_->GetOpenerOfTabContentsAt(removing_index);
  // First see if the index being removed has any "child" tabs. If it does, we
  // want to select the first in that child group, not the next tab in the same
  // group of the removed tab.
  NavigationController* removed_controller =
      &tabstrip_->GetTabContentsAt(removing_index)->controller();
  // The parent opener should never be the same as the controller being removed.
  DCHECK(parent_opener != removed_controller);
  int index = tabstrip_->GetIndexOfNextTabContentsOpenedBy(removed_controller,
                                                           removing_index,
                                                           false);
  if (index != TabStripModel::kNoTab)
    return GetValidIndex(index, removing_index);

  if (parent_opener) {
    // If the tab was in a group, shift selection to the next tab in the group.
    int index = tabstrip_->GetIndexOfNextTabContentsOpenedBy(parent_opener,
                                                             removing_index,
                                                             false);
    if (index != TabStripModel::kNoTab)
      return GetValidIndex(index, removing_index);

    // If we can't find a subsequent group member, just fall back to the
    // parent_opener itself. Note that we use "group" here since opener is
    // reset by select operations..
    index = tabstrip_->GetIndexOfController(parent_opener);
    if (index != TabStripModel::kNoTab)
      return GetValidIndex(index, removing_index);
  }

  // No opener set, fall through to the default handler...
  int selected_index = tabstrip_->active_index();
  if (selected_index >= (tab_count - 1))
    return selected_index - 1;

  return selected_index;
}

void TabStripModelOrderController::TabSelectedAt(
    TabContentsWrapper* old_contents,
    TabContentsWrapper* new_contents,
    int index,
    bool user_gesture) {
  if (old_contents == new_contents)
    return;

  NavigationController* old_opener = NULL;
  if (old_contents) {
    int index = tabstrip_->GetIndexOfTabContents(old_contents);
    if (index != TabStripModel::kNoTab) {
      old_opener = tabstrip_->GetOpenerOfTabContentsAt(index);

      // Forget any group/opener relationships that need to be reset whenever
      // selection changes (see comment in TabStripModel::AddTabContentsAt).
      if (tabstrip_->ShouldResetGroupOnSelect(old_contents))
        tabstrip_->ForgetGroup(old_contents);
    }
  }
  NavigationController* new_opener =
      tabstrip_->GetOpenerOfTabContentsAt(index);
  if (user_gesture && new_opener != old_opener &&
      new_opener != &old_contents->controller() &&
      old_opener != &new_contents->controller()) {
    tabstrip_->ForgetAllOpeners();
  }
}

///////////////////////////////////////////////////////////////////////////////
// TabStripModelOrderController, private:

int TabStripModelOrderController::GetValidIndex(
    int index, int removing_index) const {
  if (removing_index < index)
    index = std::max(0, index - 1);
  return index;
}
