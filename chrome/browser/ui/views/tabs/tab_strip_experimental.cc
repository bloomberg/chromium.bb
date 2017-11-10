// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_strip_experimental.h"

#include "base/logging.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/tab_data_experimental.h"
#include "chrome/browser/ui/tabs/tab_strip_model_experimental.h"
#include "chrome/browser/ui/views/tabs/tab_experimental.h"

TabStripExperimental::TabStripExperimental(TabStripModel* model)
    : model_(static_cast<TabStripModelExperimental*>(model)),
      bounds_animator_(this) {
  model_->AddExperimentalObserver(this);
}

TabStripExperimental::~TabStripExperimental() {
  model_->RemoveExperimentalObserver(this);
}

gfx::Size TabStripExperimental::CalculatePreferredSize() const {
  return gfx::Size(100, GetLayoutConstant(TAB_HEIGHT));
}

void TabStripExperimental::Layout() {
  static int kWidth = 100;
  for (int i = 0; i < tabs_.view_size(); i++) {
    TabExperimental* tab = tabs_.view_at(i);

    gfx::Rect ideal_bounds(i * kWidth, 0, kWidth, height());
    tabs_.set_ideal_bounds(i, ideal_bounds);
    bounds_animator_.AnimateViewTo(tab, tabs_.ideal_bounds(i));
    // TODO(brettw): Figure out SetAnimationDelegate.
  }
}

TabStripImpl* TabStripExperimental::AsTabStripImpl() {
  return nullptr;
}

int TabStripExperimental::GetMaxX() const {
  NOTIMPLEMENTED();
  return 100;
}

void TabStripExperimental::SetBackgroundOffset(const gfx::Point& offset) {
  NOTIMPLEMENTED();
}

bool TabStripExperimental::IsRectInWindowCaption(const gfx::Rect& rect) {
  NOTIMPLEMENTED();
  return false;
}

bool TabStripExperimental::IsPositionInWindowCaption(const gfx::Point& point) {
  NOTIMPLEMENTED();
  return false;
}

bool TabStripExperimental::IsTabStripCloseable() const {
  NOTIMPLEMENTED();
  return true;
}

bool TabStripExperimental::IsTabStripEditable() const {
  NOTIMPLEMENTED();
  return true;
}

bool TabStripExperimental::IsTabCrashed(int tab_index) const {
  NOTIMPLEMENTED();
  return false;
}

bool TabStripExperimental::TabHasNetworkError(int tab_index) const {
  NOTIMPLEMENTED();
  return false;
}

TabAlertState TabStripExperimental::GetTabAlertState(int tab_index) const {
  NOTIMPLEMENTED();
  return TabAlertState::NONE;
}

void TabStripExperimental::UpdateLoadingAnimations() {
  NOTIMPLEMENTED();
}

void TabStripExperimental::TabInsertedAt(int index) {
  TabExperimental* tab = new TabExperimental;
  tab->SetVisible(true);
  AddChildView(tab);
  tabs_.Add(tab, index);

  Layout();
}

void TabStripExperimental::TabClosedAt(int index) {
  tabs_.Remove(index);
  Layout();
}

void TabStripExperimental::TabChangedAt(
    int index,
    const TabDataExperimental& data,
    TabStripModelObserver::TabChangeType type) {
  TabExperimental* tab = tabs_.view_at(index);
  if (type == TabStripModelObserver::TITLE_NOT_LOADING ||
      type == TabStripModelObserver::ALL)
    tab->SetTitle(data.GetTitle());
}
