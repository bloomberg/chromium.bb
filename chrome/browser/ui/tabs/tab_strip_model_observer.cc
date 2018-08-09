// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"

using content::WebContents;

// static
TabStripModelChange::Delta TabStripModelChange::CreateInsertDelta(
    content::WebContents* contents,
    int index) {
  TabStripModelChange::Delta delta;
  delta.insert = {contents, index};
  return delta;
}

// static
TabStripModelChange::Delta TabStripModelChange::CreateRemoveDelta(
    content::WebContents* contents,
    int index,
    bool will_be_deleted) {
  TabStripModelChange::Delta delta;
  delta.remove = {contents, index, will_be_deleted};
  return delta;
}

// static
TabStripModelChange::Delta TabStripModelChange::CreateMoveDelta(
    content::WebContents* contents,
    int from_index,
    int to_index) {
  TabStripModelChange::Delta delta;
  delta.move = {contents, from_index, to_index};
  return delta;
}

// static
TabStripModelChange::Delta TabStripModelChange::CreateReplaceDelta(
    content::WebContents* old_contents,
    content::WebContents* new_contents,
    int index) {
  TabStripModelChange::Delta delta;
  delta.replace = {old_contents, new_contents, index};
  return delta;
}

TabStripModelChange::TabStripModelChange() = default;

TabStripModelChange::TabStripModelChange(Type type, const Delta& delta)
    : type_(type), deltas_({delta}) {}

TabStripModelChange::TabStripModelChange(
    TabStripModelChange::Type type,
    const std::vector<TabStripModelChange::Delta>& deltas)
    : type_(type), deltas_(deltas) {}

TabStripModelChange::~TabStripModelChange() = default;

TabStripModelChange::TabStripModelChange(TabStripModelChange&& other) = default;

TabStripSelectionChange::TabStripSelectionChange() = default;

TabStripSelectionChange::TabStripSelectionChange(
    content::WebContents* contents,
    const ui::ListSelectionModel& selection_model)
    : old_contents(contents),
      new_contents(contents),
      old_model(selection_model),
      new_model(selection_model),
      reason(0) {}

TabStripModelObserver::TabStripModelObserver() {
}

void TabStripModelObserver::OnTabStripModelChanged(
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {}

void TabStripModelObserver::TabInsertedAt(TabStripModel* tab_strip_model,
                                          WebContents* contents,
                                          int index,
                                          bool foreground) {
}

void TabStripModelObserver::TabClosingAt(TabStripModel* tab_strip_model,
                                         WebContents* contents,
                                         int index) {
}

void TabStripModelObserver::TabDetachedAt(WebContents* contents,
                                          int index,
                                          bool was_active) {}

void TabStripModelObserver::TabDeactivated(WebContents* contents) {
}

void TabStripModelObserver::ActiveTabChanged(WebContents* old_contents,
                                             WebContents* new_contents,
                                             int index,
                                             int reason) {
}

void TabStripModelObserver::TabSelectionChanged(
    TabStripModel* tab_strip_model,
    const ui::ListSelectionModel& model) {
}

void TabStripModelObserver::TabMoved(WebContents* contents,
                                     int from_index,
                                     int to_index) {
}

void TabStripModelObserver::TabChangedAt(WebContents* contents,
                                         int index,
                                         TabChangeType change_type) {
}

void TabStripModelObserver::TabReplacedAt(TabStripModel* tab_strip_model,
                                          WebContents* old_contents,
                                          WebContents* new_contents,
                                          int index) {
}

void TabStripModelObserver::TabPinnedStateChanged(
    TabStripModel* tab_strip_model,
    WebContents* contents,
    int index) {
}

void TabStripModelObserver::TabBlockedStateChanged(WebContents* contents,
                                                   int index) {
}

void TabStripModelObserver::TabStripEmpty() {
}

void TabStripModelObserver::WillCloseAllTabs(TabStripModel* tab_strip_model) {}

void TabStripModelObserver::CloseAllTabsStopped(TabStripModel* tab_strip_model,
                                                CloseAllStoppedReason reason) {}
void TabStripModelObserver::SetTabNeedsAttentionAt(int index, bool attention) {}
