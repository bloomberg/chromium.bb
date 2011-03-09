// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TABS_TAB_STRIP_SELECTION_MODEL_H_
#define CHROME_BROWSER_TABS_TAB_STRIP_SELECTION_MODEL_H_
#pragma once

#include <vector>

#include "base/basictypes.h"

// Selection model used by the tab strip. In addition to the set of selected
// indices TabStripSelectionModel maintains the following:
// active: the index of the currently visible tab in the tab strip.
// anchor: the index of the last tab the user clicked on. Extending the
// selection extends it from this index.
//
// Typically there is only one selected tab in the tabstrip, in which case the
// anchor and active index correspond to the same thing.
class TabStripSelectionModel {
 public:
  typedef std::vector<int> SelectedIndices;

  // Used to identify no selection.
  static const int kUnselectedIndex;

  TabStripSelectionModel();
  ~TabStripSelectionModel();

  // See class description for details of the anchor.
  void set_anchor(int anchor) { anchor_ = anchor; }
  int anchor() const { return anchor_; }

  // See class description for details of active.
  void set_active(int active) { active_ = active; }
  int active() const { return active_; }

  // True if nothing is selected.
  bool empty() const { return selected_indices_.empty(); }

  // Number of selected indices.
  size_t size() const { return selected_indices_.size(); }

  // Increments all indices >= |index|. For example, if the selection consists
  // of [0, 1, 5] and this is invoked with 1, it results in [0, 2, 6]. This also
  // updates the anchor and active indices.
  // This is used when a new tab is inserted into the tabstrip.
  void IncrementFrom(int index);

  // Shifts all indices < |index| down by 1. If |index| is selected, it is
  // removed. For example, if the selection consists of [0, 1, 5] and this is
  // invoked with 1, it results in [0, 4]. This is used when a tab is removed
  // from the tabstrip.
  void DecrementFrom(int index);

  // Sets the anchor, active and selection to |index|.
  void SetSelectedIndex(int index);

  // Returns true if |index| is selected.
  bool IsSelected(int index);

  // Adds |index| to the selection. This does not change the active or anchor
  // indices.
  void AddIndexToSelection(int index);

  // Removes |index| from the selection. This does not change the active or
  // anchor indices.
  void RemoveIndexFromSelection(int index);

  // Extends the selection from the anchor to |index|. If the anchor is empty,
  // this sets the anchor, selection and active indices to |index|.
  void SetSelectionFromAnchorTo(int index);

  // Invoked when an item moves. |from| is the original index, and |to| the
  // target index.
  // NOTE: this matches the TabStripModel API. If moving to a greater index,
  // |to| should be the index *after* removing |from|. For example, consider
  // three tabs 'A B C', to move A to the end of the list, this should be
  // invoked with '0, 2'.
  void Move(int from, int to);

  // Sets the anchor and active to kUnselectedIndex, and removes all the
  // selected indices.
  void Clear();

  // Returns the selected indices. The selection is always ordered in acending
  // order.
  const SelectedIndices& selected_indices() const { return selected_indices_; }

  // Copies the selection from |source| to this.
  void Copy(const TabStripSelectionModel& source);

 private:
  SelectedIndices selected_indices_;

  int active_;

  int anchor_;

  DISALLOW_COPY_AND_ASSIGN(TabStripSelectionModel);
};

#endif  // CHROME_BROWSER_TABS_TAB_STRIP_SELECTION_MODEL_H_
