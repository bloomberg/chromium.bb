// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/fake_base_tab_strip_controller.h"

#include "chrome/browser/ui/views/tabs/tab_renderer_data.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"

FakeBaseTabStripController::FakeBaseTabStripController()
    : tab_strip_(NULL),
      num_tabs_(0) {
}

FakeBaseTabStripController::~FakeBaseTabStripController() {
}

void FakeBaseTabStripController::AddTab(int index) {
  num_tabs_++;
  tab_strip_->AddTabAt(index, TabRendererData(), false);
}

void FakeBaseTabStripController::RemoveTab(int index) {
  num_tabs_--;
  tab_strip_->RemoveTabAt(index);
}

const ui::ListSelectionModel& FakeBaseTabStripController::GetSelectionModel() {
  return selection_model_;
}

int FakeBaseTabStripController::GetCount() const {
  return num_tabs_;
}

bool FakeBaseTabStripController::IsValidIndex(int index) const {
  return index >= 0 && index < num_tabs_;
}

bool FakeBaseTabStripController::IsActiveTab(int index) const {
  return false;
}

int FakeBaseTabStripController::GetActiveIndex() const {
  return -1;
}

bool FakeBaseTabStripController::IsTabSelected(int index) const {
  return false;
}

bool FakeBaseTabStripController::IsTabPinned(int index) const {
  return false;
}

bool FakeBaseTabStripController::IsNewTabPage(int index) const {
  return false;
}

void FakeBaseTabStripController::SelectTab(int index) {
}

void FakeBaseTabStripController::ExtendSelectionTo(int index) {
}

void FakeBaseTabStripController::ToggleSelected(int index) {
}

void FakeBaseTabStripController::AddSelectionFromAnchorTo(int index) {
}

void FakeBaseTabStripController::CloseTab(int index, CloseTabSource source) {
}

void FakeBaseTabStripController::ShowContextMenuForTab(Tab* tab,
                                                       const gfx::Point& p) {
}

void FakeBaseTabStripController::UpdateLoadingAnimations() {
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

bool FakeBaseTabStripController::IsIncognito() {
  return false;
}

void FakeBaseTabStripController::LayoutTypeMaybeChanged() {
}
