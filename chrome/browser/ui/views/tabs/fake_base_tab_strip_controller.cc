// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/fake_base_tab_strip_controller.h"

#include <utility>

#include "chrome/browser/ui/views/tabs/tab_renderer_data.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"

FakeBaseTabStripController::FakeBaseTabStripController() {}

FakeBaseTabStripController::~FakeBaseTabStripController() {
}

void FakeBaseTabStripController::AddTab(int index, bool is_active) {
  num_tabs_++;
  tab_strip_->AddTabAt(index, TabRendererData(), is_active);
  if (is_active)
    active_index_ = index;
}

void FakeBaseTabStripController::AddPinnedTab(int index, bool is_active) {
  TabRendererData data;
  data.pinned = true;
  num_tabs_++;
  tab_strip_->AddTabAt(index, std::move(data), is_active);
  if (is_active)
    active_index_ = index;
}

void FakeBaseTabStripController::RemoveTab(int index) {
  num_tabs_--;
  tab_strip_->RemoveTabAt(nullptr, index);
  if (active_index_ == index)
    active_index_ = -1;
}

const ui::ListSelectionModel&
FakeBaseTabStripController::GetSelectionModel() const {
  return selection_model_;
}

int FakeBaseTabStripController::GetCount() const {
  return num_tabs_;
}

bool FakeBaseTabStripController::IsValidIndex(int index) const {
  return index >= 0 && index < num_tabs_;
}

bool FakeBaseTabStripController::IsActiveTab(int index) const {
  if (!IsValidIndex(index))
    return false;
  return active_index_ == index;
}

int FakeBaseTabStripController::GetActiveIndex() const {
  return active_index_;
}

bool FakeBaseTabStripController::IsTabSelected(int index) const {
  return false;
}

bool FakeBaseTabStripController::IsTabPinned(int index) const {
  return false;
}

void FakeBaseTabStripController::SelectTab(int index) {
  if (!IsValidIndex(index) || active_index_ == index)
    return;
  ui::ListSelectionModel old_selection_model;
  old_selection_model.SetSelectedIndex(active_index_);
  active_index_ = index;
  selection_model_.SetSelectedIndex(active_index_);
  tab_strip_->SetSelection(old_selection_model, selection_model_);
}

void FakeBaseTabStripController::ExtendSelectionTo(int index) {
}

void FakeBaseTabStripController::ToggleSelected(int index) {
}

void FakeBaseTabStripController::AddSelectionFromAnchorTo(int index) {
}

void FakeBaseTabStripController::CloseTab(int index, CloseTabSource source) {
}

void FakeBaseTabStripController::ToggleTabAudioMute(int index) {
}

void FakeBaseTabStripController::ShowContextMenuForTab(
    Tab* tab,
    const gfx::Point& p,
    ui::MenuSourceType source_type) {
}

int FakeBaseTabStripController::HasAvailableDragActions() const {
  return 0;
}

void FakeBaseTabStripController::OnDropIndexUpdate(int index,
                                                   bool drop_before) {
}

void FakeBaseTabStripController::PerformDrop(bool drop_before,
                                             int index,
                                             const GURL& url) {
}

bool FakeBaseTabStripController::IsCompatibleWith(TabStrip* other) const {
  return false;
}

void FakeBaseTabStripController::CreateNewTab() {
}

void FakeBaseTabStripController::CreateNewTabWithLocation(
    const base::string16& location) {
}

bool FakeBaseTabStripController::IsIncognito() {
  return false;
}

void FakeBaseTabStripController::StackedLayoutMaybeChanged() {
}

void FakeBaseTabStripController::OnStartedDraggingTabs() {
}

void FakeBaseTabStripController::OnStoppedDraggingTabs() {
}

void FakeBaseTabStripController::CheckFileSupported(const GURL& url) {
  tab_strip_->FileSupported(url, true);
}

SkColor FakeBaseTabStripController::GetToolbarTopSeparatorColor() const {
  return SK_ColorBLACK;
}

base::string16 FakeBaseTabStripController::GetAccessibleTabName(
    const Tab* tab) const {
  return base::string16();
}

Profile* FakeBaseTabStripController::GetProfile() const {
  return nullptr;
}
