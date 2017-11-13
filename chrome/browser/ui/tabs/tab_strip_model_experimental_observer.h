// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_MODEL_EXPERIMENTAL_OBSERVER_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_MODEL_EXPERIMENTAL_OBSERVER_H_

#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"

class TabDataExperimental;

class TabStripModelExperimentalObserver {
 public:
  // If the group is null the index is into the toplevel tab strip.
  virtual void TabInsertedAt(/*Group* group,*/ int index, bool is_active) = 0;
  virtual void TabClosedAt(/*Group* group,*/ int index) = 0;

  virtual void TabChangedAt(int index,
                            const TabDataExperimental& data,
                            TabStripModelObserver::TabChangeType type) = 0;

  virtual void TabSelectionChanged(
      const ui::ListSelectionModel& old_selection,
      const ui::ListSelectionModel& new_selection) = 0;

  /* TODO(brettw) add group notifications. Some initial thinking:
  struct GroupCreateParams {
    Group* group;

    // This is the range of existing tabs in the toplevel tab strip that are
    // incorporated into the group. These are effectively removed from the
    // toplevel and inserted into the group without changing them.
    //
    // The range is, like STL, [begin, end) and it may be empty.
    int incorporate_begin;
    int incorporate_end;

    // These are indicies of new tabs that are added inside the group.
    std::vector<int> added_indices;
  };
  virtual void GroupCreatedAt(const GroupCreateParams& params) = 0;

  virtual void GroupDestroyedAt(int index) = 0;
  */

 private:
};

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_MODEL_EXPERIMENTAL_OBSERVER_H_
