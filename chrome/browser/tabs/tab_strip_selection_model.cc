// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tabs/tab_strip_selection_model.h"

#include <algorithm>
#include <valarray>

#include "base/logging.h"

// static
const int TabStripSelectionModel::kUnselectedIndex = -1;

static void IncrementFromImpl(int index, int* value) {
  if (*value >= index)
    (*value)++;
}

static bool DecrementFromImpl(int index, int* value) {
  if (*value == index) {
    *value = TabStripSelectionModel::kUnselectedIndex;
    return true;
  }
  if (*value > index)
    (*value)--;
  return false;
}

TabStripSelectionModel::TabStripSelectionModel()
    : active_(kUnselectedIndex),
      anchor_(kUnselectedIndex) {
}

TabStripSelectionModel::~TabStripSelectionModel() {
}

void TabStripSelectionModel::IncrementFrom(int index) {
  // Shift the selection to account for the newly inserted tab.
  for (SelectedIndices::iterator i = selected_indices_.begin();
       i != selected_indices_.end(); ++i) {
    IncrementFromImpl(index, &(*i));
  }
  IncrementFromImpl(index, &anchor_);
  IncrementFromImpl(index, &active_);
}

void TabStripSelectionModel::DecrementFrom(int index) {
  for (SelectedIndices::iterator i = selected_indices_.begin();
       i != selected_indices_.end(); ) {
    if (DecrementFromImpl(index, &(*i)))
      i = selected_indices_.erase(i);
    else
      ++i;
  }
  DecrementFromImpl(index, &anchor_);
  DecrementFromImpl(index, &active_);
}

void TabStripSelectionModel::SetSelectedIndex(int index) {
  anchor_ = active_ = index;
  SetSelectionFromAnchorTo(index);
}

bool TabStripSelectionModel::IsSelected(int index) const {
  return std::find(selected_indices_.begin(), selected_indices_.end(), index) !=
      selected_indices_.end();
}

void TabStripSelectionModel::AddIndexToSelection(int index) {
  if (!IsSelected(index)) {
    selected_indices_.push_back(index);
    std::sort(selected_indices_.begin(), selected_indices_.end());
  }
}

void TabStripSelectionModel::RemoveIndexFromSelection(int index) {
  SelectedIndices::iterator i = std::find(selected_indices_.begin(),
                                          selected_indices_.end(), index);
  if (i != selected_indices_.end())
    selected_indices_.erase(i);
}

void TabStripSelectionModel::SetSelectionFromAnchorTo(int index) {
  if (anchor_ == kUnselectedIndex) {
    SetSelectedIndex(index);
  } else {
    int delta = std::abs(index - anchor_);
    SelectedIndices new_selection(delta + 1, 0);
    for (int i = 0, min = std::min(index, anchor_); i <= delta; ++i)
      new_selection[i] = i + min;
    selected_indices_.swap(new_selection);
    active_ = index;
  }
}

void TabStripSelectionModel::AddSelectionFromAnchorTo(int index) {
  if (anchor_ == kUnselectedIndex) {
    SetSelectedIndex(index);
  } else {
    for (int i = std::min(index, anchor_), end = std::max(index, anchor_);
         i <= end; ++i) {
      if (!IsSelected(i))
        selected_indices_.push_back(i);
    }
    std::sort(selected_indices_.begin(), selected_indices_.end());
    active_ = index;
  }
}

void TabStripSelectionModel::Move(int from, int to) {
  DCHECK_NE(to, from);
  bool was_anchor = from == anchor_;
  bool was_active = from == active_;
  bool was_selected = IsSelected(from);
  if (to < from) {
    IncrementFrom(to);
    DecrementFrom(from + 1);
  } else {
    DecrementFrom(from);
    IncrementFrom(to);
  }
  if (was_active)
    active_ = to;
  if (was_anchor)
    anchor_ = to;
  if (was_selected)
    AddIndexToSelection(to);
}

void TabStripSelectionModel::Clear() {
  anchor_ = active_ = kUnselectedIndex;
  SelectedIndices empty_selection;
  selected_indices_.swap(empty_selection);
}

void TabStripSelectionModel::Copy(const TabStripSelectionModel& source) {
  selected_indices_ = source.selected_indices_;
  active_ = source.active_;
  anchor_ = source.anchor_;
}
