// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/tabs/base_tab_strip.h"

#include "chrome/browser/views/tabs/tab_strip_controller.h"

BaseTabStrip::BaseTabStrip(TabStripController* controller)
    : controller_(controller) {
}

BaseTabStrip::~BaseTabStrip() {
}

void BaseTabStrip::UpdateLoadingAnimations() {
  controller_->UpdateLoadingAnimations();
}

BaseTabRenderer* BaseTabStrip::GetSelectedBaseTab() const {
  return GetBaseTabAtModelIndex(controller_->GetSelectedIndex());
}

int BaseTabStrip::GetModelCount() const {
  return controller_->GetCount();
}

bool BaseTabStrip::IsValidModelIndex(int model_index) const {
  return controller_->IsValidIndex(model_index);
}

void BaseTabStrip::SelectTab(BaseTabRenderer* tab) {
  int model_index = GetModelIndexOfBaseTab(tab);
  if (IsValidModelIndex(model_index))
    controller_->SelectTab(model_index);
}

void BaseTabStrip::CloseTab(BaseTabRenderer* tab) {
  int model_index = GetModelIndexOfBaseTab(tab);
  if (IsValidModelIndex(model_index))
    controller_->CloseTab(model_index);
}

void BaseTabStrip::ShowContextMenu(BaseTabRenderer* tab, const gfx::Point& p) {
  controller_->ShowContextMenu(tab, p);
}

bool BaseTabStrip::IsTabSelected(const BaseTabRenderer* tab) const {
  int model_index = GetModelIndexOfBaseTab(tab);
  return IsValidModelIndex(model_index) &&
      controller_->IsTabSelected(model_index);
}

bool BaseTabStrip::IsTabPinned(const BaseTabRenderer* tab) const {
  int model_index = GetModelIndexOfBaseTab(tab);
  return IsValidModelIndex(model_index) &&
      controller_->IsTabPinned(model_index);
}
