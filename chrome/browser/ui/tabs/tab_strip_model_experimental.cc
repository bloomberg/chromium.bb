// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_model_experimental.h"

#include "base/logging.h"

TabStripModelExperimental::TabStripModelExperimental(
    TabStripModelDelegate* delegate,
    Profile* profile)
    : delegate_(delegate), profile_(profile) {}

TabStripModelExperimental::~TabStripModelExperimental() {}

TabStripModelDelegate* TabStripModelExperimental::delegate() const {
  return delegate_;
}

void TabStripModelExperimental::AddObserver(TabStripModelObserver* observer) {
  observers_.AddObserver(observer);
}

void TabStripModelExperimental::RemoveObserver(
    TabStripModelObserver* observer) {
  observers_.RemoveObserver(observer);
}

int TabStripModelExperimental::count() const {
  NOTIMPLEMENTED();
  return 0;
}

bool TabStripModelExperimental::empty() const {
  NOTIMPLEMENTED();
  return true;
}

Profile* TabStripModelExperimental::profile() const {
  return profile_;
}

int TabStripModelExperimental::active_index() const {
  NOTIMPLEMENTED();
  return kNoTab;
}

bool TabStripModelExperimental::closing_all() const {
  NOTIMPLEMENTED();
  return false;
}

bool TabStripModelExperimental::ContainsIndex(int index) const {
  NOTIMPLEMENTED();
  return false;
}

void TabStripModelExperimental::AppendWebContents(
    content::WebContents* contents,
    bool foreground) {
  NOTIMPLEMENTED();
}

void TabStripModelExperimental::InsertWebContentsAt(
    int index,
    content::WebContents* contents,
    int add_types) {
  NOTIMPLEMENTED();
}

bool TabStripModelExperimental::CloseWebContentsAt(int index,
                                                   uint32_t close_types) {
  NOTIMPLEMENTED();
  return true;
}

content::WebContents* TabStripModelExperimental::ReplaceWebContentsAt(
    int index,
    content::WebContents* new_contents) {
  NOTIMPLEMENTED();
  return nullptr;
}

content::WebContents* TabStripModelExperimental::DetachWebContentsAt(
    int index) {
  NOTIMPLEMENTED();
  return nullptr;
}

void TabStripModelExperimental::ActivateTabAt(int index, bool user_gesture) {
  NOTIMPLEMENTED();
}

void TabStripModelExperimental::AddTabAtToSelection(int index) {
  NOTIMPLEMENTED();
}

void TabStripModelExperimental::MoveWebContentsAt(int index,
                                                  int to_position,
                                                  bool select_after_move) {
  NOTIMPLEMENTED();
}

void TabStripModelExperimental::MoveSelectedTabsTo(int index) {
  NOTIMPLEMENTED();
}

content::WebContents* TabStripModelExperimental::GetActiveWebContents() const {
  NOTIMPLEMENTED();
  return nullptr;
}

content::WebContents* TabStripModelExperimental::GetWebContentsAt(
    int index) const {
  NOTIMPLEMENTED();
  return nullptr;
}

int TabStripModelExperimental::GetIndexOfWebContents(
    const content::WebContents* contents) const {
  NOTIMPLEMENTED();
  return kNoTab;
}

void TabStripModelExperimental::UpdateWebContentsStateAt(
    int index,
    TabStripModelObserver::TabChangeType change_type) {
  NOTIMPLEMENTED();
}

void TabStripModelExperimental::SetTabNeedsAttentionAt(int index,
                                                       bool attention) {
  NOTIMPLEMENTED();
}

void TabStripModelExperimental::CloseAllTabs() {
  NOTIMPLEMENTED();
}

bool TabStripModelExperimental::TabsAreLoading() const {
  NOTIMPLEMENTED();
  return false;
}

content::WebContents* TabStripModelExperimental::GetOpenerOfWebContentsAt(
    int index) {
  NOTIMPLEMENTED();
  return nullptr;
}

void TabStripModelExperimental::SetOpenerOfWebContentsAt(
    int index,
    content::WebContents* opener) {
  NOTIMPLEMENTED();
}

void TabStripModelExperimental::TabNavigating(content::WebContents* contents,
                                              ui::PageTransition transition) {
  NOTIMPLEMENTED();
}

void TabStripModelExperimental::SetTabBlocked(int index, bool blocked) {
  NOTIMPLEMENTED();
}

void TabStripModelExperimental::SetTabPinned(int index, bool pinned) {
  NOTIMPLEMENTED();
}

bool TabStripModelExperimental::IsTabPinned(int index) const {
  NOTIMPLEMENTED();
  return false;
}

bool TabStripModelExperimental::IsTabBlocked(int index) const {
  NOTIMPLEMENTED();
  return false;
}

int TabStripModelExperimental::IndexOfFirstNonPinnedTab() const {
  NOTIMPLEMENTED();
  return 0;
}

void TabStripModelExperimental::ExtendSelectionTo(int index) {
  NOTIMPLEMENTED();
}

void TabStripModelExperimental::ToggleSelectionAt(int index) {
  NOTIMPLEMENTED();
}

void TabStripModelExperimental::AddSelectionFromAnchorTo(int index) {
  NOTIMPLEMENTED();
}

bool TabStripModelExperimental::IsTabSelected(int index) const {
  NOTIMPLEMENTED();
  return false;
}

void TabStripModelExperimental::SetSelectionFromModel(
    ui::ListSelectionModel source) {
  NOTIMPLEMENTED();
}

const ui::ListSelectionModel& TabStripModelExperimental::selection_model()
    const {
  return selection_model_;
}

void TabStripModelExperimental::AddWebContents(content::WebContents* contents,
                                               int index,
                                               ui::PageTransition transition,
                                               int add_types) {
  NOTIMPLEMENTED();
}

void TabStripModelExperimental::CloseSelectedTabs() {
  NOTIMPLEMENTED();
}

void TabStripModelExperimental::SelectNextTab() {
  NOTIMPLEMENTED();
}

void TabStripModelExperimental::SelectPreviousTab() {
  NOTIMPLEMENTED();
}

void TabStripModelExperimental::SelectLastTab() {
  NOTIMPLEMENTED();
}

void TabStripModelExperimental::MoveTabNext() {
  NOTIMPLEMENTED();
}

void TabStripModelExperimental::MoveTabPrevious() {
  NOTIMPLEMENTED();
}

bool TabStripModelExperimental::IsContextMenuCommandEnabled(
    int context_index,
    ContextMenuCommand command_id) const {
  NOTIMPLEMENTED();
  return false;
}

void TabStripModelExperimental::ExecuteContextMenuCommand(
    int context_index,
    ContextMenuCommand command_id) {
  NOTIMPLEMENTED();
}

std::vector<int> TabStripModelExperimental::GetIndicesClosedByCommand(
    int index,
    ContextMenuCommand id) const {
  NOTIMPLEMENTED();
  return std::vector<int>();
}

bool TabStripModelExperimental::WillContextMenuMute(int index) {
  NOTIMPLEMENTED();
  return false;
}

bool TabStripModelExperimental::WillContextMenuMuteSites(int index) {
  NOTIMPLEMENTED();
  return false;
}

bool TabStripModelExperimental::WillContextMenuPin(int index) {
  NOTIMPLEMENTED();
  return false;
}
