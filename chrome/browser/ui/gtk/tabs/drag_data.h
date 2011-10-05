// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_TABS_DRAG_DATA_H_
#define CHROME_BROWSER_UI_GTK_TABS_DRAG_DATA_H_
#pragma once

#include <vector>

#include "base/basictypes.h"

class TabContents;
class TabContentsDelegate;
class TabContentsWrapper;
class TabGtk;

struct DraggedTabData {
 public:
  DraggedTabData();
  DraggedTabData(TabGtk* tab,
                 TabContentsWrapper* contents,
                 TabContentsDelegate* original_delegate,
                 int source_model_index,
                 bool pinned,
                 bool mini);
  ~DraggedTabData();

  // Resetting the delegate of |contents_->tab_contents()| to
  // |original_delegate_|.
  void ResetDelegate();

  // The tab being dragged.
  TabGtk* tab_;

  // The TabContents being dragged.
  TabContentsWrapper* contents_;

  // The original TabContentsDelegate of |contents|, before it was detached
  // from the browser window. We store this so that we can forward certain
  // delegate notifications back to it if we can't handle them locally.
  TabContentsDelegate* original_delegate_;

  // This is the index of |contents| in |source_tabstrip_| when the drag
  // began. This is used to restore the previous state if the drag is aborted.
  int source_model_index_;

  // Is the tab pinned?
  bool pinned_;

  // Is the tab mini?
  bool mini_;
};

// Holds information about all the dragged tabs. It also provides several
// convenience methods.
class DragData {
 public:
  DragData(std::vector<DraggedTabData> drag_data, int source_tab_index);
  ~DragData();

  // Returns all the |tab_| fields of the tabs in |drag_data_|.
  std::vector<TabGtk*> GetDraggedTabs() const;

  // Returns all the |contents_| fields of the tabs in |drag_data_|.
  std::vector<TabContents*> GetDraggedTabsContents() const;

  // Returns the correct add type for the tab in |drag_data_[i]|. See
  // TabStripModel::AddTabTypes for available types.
  int GetAddTypesForDraggedTabAt(size_t index);

  // Calculates the number of mini and non mini tabs from position |from|
  // (included) up to position |to| (excluded) within |drag_data_| and
  // populates |mini| and |non_mini| accordingly.
  void GetNumberOfMiniNonMiniTabs(int from, int to, int* mini,
                                  int* non_mini) const;

  // Convenience method for getting the number of the dragged tabs.
  size_t size() const { return drag_data_.size(); }

  // Convenience method for getting the drag data associated with tab at |index|
  // within |drag_data_|.
  DraggedTabData* get(size_t index) { return &drag_data_[index]; }

  int source_tab_index() const { return source_tab_index_; }
  int mini_tab_count() const { return mini_tab_count_; }
  int non_mini_tab_count() const { return non_mini_tab_count_; }

  // Convenience for |source_tab_drag_data()->contents_|.
  TabContentsWrapper* GetSourceTabContentsWrapper();

  // Convenience for |source_tab_drag_data()->contents_->tab_contents()|.
  TabContents* GetSourceTabContents();

  // Convenience for getting the DraggedTabData corresponding to the tab that
  // was under the mouse pointer when the user started dragging.
  DraggedTabData* GetSourceTabData();

 private:
  std::vector<DraggedTabData> drag_data_;

  // Index of the source tab in |drag_data_|.
  int source_tab_index_;
  // Number of non mini tabs within |drag_data_|.
  int non_mini_tab_count_;
  // Number of mini tabs within |drag_data_|.
  int mini_tab_count_;

  DISALLOW_COPY_AND_ASSIGN(DragData);
};

#endif  // CHROME_BROWSER_UI_GTK_TABS_DRAG_DATA_H_
