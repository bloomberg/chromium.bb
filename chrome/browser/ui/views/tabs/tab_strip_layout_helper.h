// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_LAYOUT_HELPER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_LAYOUT_HELPER_H_

#include <map>
#include <vector>

#include "base/optional.h"
#include "ui/views/view_model.h"

class Tab;
class TabGroupHeader;
class TabGroupId;
class TabStripController;

// Helper class for TabStrip, that is responsible for calculating the tabs'
// ideal layout and assigning those bounds, as well as caching the derived
// values resulting from that calculation.
class TabStripLayoutHelper {
 public:
  TabStripLayoutHelper();
  ~TabStripLayoutHelper();

  int active_tab_width() { return active_tab_width_; }
  int inactive_tab_width() { return inactive_tab_width_; }
  int first_non_pinned_tab_index() { return first_non_pinned_tab_index_; }
  int first_non_pinned_tab_x() { return first_non_pinned_tab_x_; }

  // Returns the number of pinned tabs in |tabs|.
  int GetPinnedTabCount(const views::ViewModelT<Tab>* tabs) const;

  // Generates and sets the ideal bounds for the views in |tabs| and
  // |group_headers|. Updates the cached widths in |active_tab_width_| and
  // |inactive_tab_width_|.
  void UpdateIdealBounds(TabStripController* controller,
                         views::ViewModelT<Tab>* tabs,
                         std::map<TabGroupId, TabGroupHeader*> group_headers,
                         int available_width);

  // Generates and sets the ideal bounds for |tabs|. Updates
  // the cached values in |first_non_pinned_tab_index_| and
  // |first_non_pinned_tab_x_|.
  void UpdateIdealBoundsForPinnedTabs(views::ViewModelT<Tab>* tabs);

 private:
  // The current widths of tabs. If the space for tabs is not evenly divisible
  // into these widths, the initial tabs in the strip will be 1 px larger.
  int active_tab_width_;
  int inactive_tab_width_;

  int first_non_pinned_tab_index_;
  int first_non_pinned_tab_x_;

  DISALLOW_COPY_AND_ASSIGN(TabStripLayoutHelper);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_LAYOUT_HELPER_H_
