// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/fake_base_tab_strip_controller.h"

FakeBaseTabStripController::FakeBaseTabStripController() {
}

FakeBaseTabStripController::~FakeBaseTabStripController() {
}

const TabStripSelectionModel& FakeBaseTabStripController::GetSelectionModel() {
  return selection_model_;
}

int FakeBaseTabStripController::GetCount() const {
  return 0;
}

bool FakeBaseTabStripController::IsValidIndex(int index) const {
  return false;
}

bool FakeBaseTabStripController::IsActiveTab(int index) const {
  return false;
}

bool FakeBaseTabStripController::IsTabSelected(int index) const {
  return false;
}

bool FakeBaseTabStripController::IsTabPinned(int index) const {
  return false;
}

bool FakeBaseTabStripController::IsTabCloseable(int index) const {
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

void FakeBaseTabStripController::CloseTab(int index) {
}

void FakeBaseTabStripController::ShowContextMenuForTab(BaseTab* tab,
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

bool FakeBaseTabStripController::IsCompatibleWith(BaseTabStrip* other) const {
  return false;
}

void FakeBaseTabStripController::CreateNewTab() {
}

void FakeBaseTabStripController::ClickActiveTab(int index) {
}
