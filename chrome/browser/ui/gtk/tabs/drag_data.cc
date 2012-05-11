// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/tabs/drag_data.h"

#include "chrome/browser/ui/gtk/tabs/tab_gtk.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"

using content::WebContents;

DraggedTabData::DraggedTabData()
    : tab_(NULL),
      contents_(NULL),
      original_delegate_(NULL),
      source_model_index_(-1),
      pinned_(false),
      mini_(false) {
}

DraggedTabData::DraggedTabData(TabGtk* tab,
                               TabContentsWrapper* contents,
                               content::WebContentsDelegate* original_delegate,
                               int source_model_index,
                               bool pinned,
                               bool mini)
    : tab_(tab),
      contents_(contents),
      original_delegate_(original_delegate),
      source_model_index_(source_model_index),
      pinned_(pinned),
      mini_(mini) {
}

DraggedTabData::~DraggedTabData() {
}

void DraggedTabData::ResetDelegate() {
  contents_->web_contents()->SetDelegate(original_delegate_);
}

DragData::DragData(std::vector<DraggedTabData> drag_data, int source_tab_index)
    : drag_data_(drag_data),
      source_tab_index_(source_tab_index),
      non_mini_tab_count_(0),
      mini_tab_count_(0) {
  GetNumberOfMiniNonMiniTabs(0, drag_data_.size(),
                             &mini_tab_count_, &non_mini_tab_count_);
}

DragData::~DragData() {
}

std::vector<TabGtk*> DragData::GetDraggedTabs() const {
  std::vector<TabGtk*> tabs;
  for (size_t i = 0; i < drag_data_.size(); ++i) {
    if (drag_data_[i].tab_)
      tabs.push_back(drag_data_[i].tab_);
  }
  return tabs;
}

std::vector<WebContents*> DragData::GetDraggedTabsContents() const {
  std::vector<WebContents*> tabs_contents;
  for (size_t i = 0; i < drag_data_.size(); ++i) {
    if (drag_data_[i].contents_->web_contents())
      tabs_contents.push_back(drag_data_[i].contents_->web_contents());
  }
  return tabs_contents;
}

void DragData::GetNumberOfMiniNonMiniTabs(
    int from, int to, int* mini, int* non_mini) const {
  DCHECK(to <= static_cast<int>(drag_data_.size()));

  *mini = 0;
  *non_mini = 0;
  for (int i = from; i < to; ++i) {
    if (drag_data_[i].mini_)
      (*mini)++;
    else
      (*non_mini)++;
  }
}

int DragData::GetAddTypesForDraggedTabAt(size_t index) {
  int add_types = TabStripModel::ADD_NONE;
  if (get(index)->pinned_)
    add_types |= TabStripModel::ADD_PINNED;
  if (static_cast<int>(index) == source_tab_index_)
    add_types |= TabStripModel::ADD_ACTIVE;
  return add_types;
}

TabContentsWrapper* DragData::GetSourceTabContentsWrapper() {
  return GetSourceTabData()->contents_;
}

WebContents* DragData::GetSourceWebContents() {
  TabContentsWrapper* contents = GetSourceTabData()->contents_;
  return contents ? contents->web_contents(): NULL;
}

DraggedTabData* DragData::GetSourceTabData() {
  return get(source_tab_index_);
}
