// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/tabs/browser_tab_strip_controller.h"

#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/views/tabs/side_tab_strip.h"

////////////////////////////////////////////////////////////////////////////////
// BrowserTabStripController, public:

BrowserTabStripController::BrowserTabStripController(TabStripModel* model,
                                                     SideTabStrip* tabstrip)
    : model_(model),
      tabstrip_(tabstrip) {
  model_->AddObserver(this);
}

BrowserTabStripController::~BrowserTabStripController() {
}

////////////////////////////////////////////////////////////////////////////////
// BrowserTabStripController, SideTabStripModel implementation:

SkBitmap BrowserTabStripController::GetIcon(int index) const {
  return model_->GetTabContentsAt(index)->GetFavIcon();
}

string16 BrowserTabStripController::GetTitle(int index) const {
  return model_->GetTabContentsAt(index)->GetTitle();
}

bool BrowserTabStripController::IsSelected(int index) const {
  return model_->selected_index() == index;
}

SideTabStripModel::NetworkState
    BrowserTabStripController::GetNetworkState(int index) const {
  TabContents* contents = model_->GetTabContentsAt(index);
  if (!contents || !contents->is_loading())
    return NetworkState_None;
  if (contents->waiting_for_response())
    return NetworkState_Waiting;
  return NetworkState_Loading;
}

void BrowserTabStripController::SelectTab(int index) {
  model_->SelectTabContentsAt(index, true);
}

void BrowserTabStripController::CloseTab(int index) {
  model_->CloseTabContentsAt(index);
}

////////////////////////////////////////////////////////////////////////////////
// BrowserTabStripController, TabStripModelObserver implementation:

void BrowserTabStripController::TabInsertedAt(TabContents* contents, int index,
                                              bool foreground) {
  tabstrip_->AddTabAt(index);
}

void BrowserTabStripController::TabDetachedAt(TabContents* contents,
                                              int index) {
  tabstrip_->RemoveTabAt(index);
}

void BrowserTabStripController::TabSelectedAt(TabContents* old_contents,
                                              TabContents* contents, int index,
                                              bool user_gesture) {
  tabstrip_->SelectTabAt(index);
}

void BrowserTabStripController::TabMoved(TabContents* contents, int from_index,
                                         int to_index) {
}

void BrowserTabStripController::TabChangedAt(TabContents* contents, int index,
                                             TabChangeType change_type) {
  tabstrip_->UpdateTabAt(index);
}

void BrowserTabStripController::TabReplacedAt(TabContents* old_contents,
                                              TabContents* new_contents,
                                              int index) {
}

void BrowserTabStripController::TabPinnedStateChanged(TabContents* contents,
                                                      int index) {
}

void BrowserTabStripController::TabBlockedStateChanged(TabContents* contents,
                                                       int index) {
}

