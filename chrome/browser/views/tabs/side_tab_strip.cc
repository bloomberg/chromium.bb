// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/tabs/side_tab_strip.h"

#include "chrome/browser/views/tabs/side_tab.h"
#include "chrome/browser/view_ids.h"

namespace {
const int kVerticalTabSpacing = 2;
const int kTabStripWidth = 140;
const int kTabStripInset = 3;
}

////////////////////////////////////////////////////////////////////////////////
// SideTabStrip, public:

SideTabStrip::SideTabStrip(TabStripController* controller)
    : BaseTabStrip(controller) {
  SetID(VIEW_ID_TAB_STRIP);
}

SideTabStrip::~SideTabStrip() {
}

////////////////////////////////////////////////////////////////////////////////
// SideTabStrip, BaseTabStrip implementation:

int SideTabStrip::GetPreferredHeight() {
  return 0;
}

void SideTabStrip::SetBackgroundOffset(const gfx::Point& offset) {
}

bool SideTabStrip::IsPositionInWindowCaption(const gfx::Point& point) {
  return GetViewForPoint(point) == this;
}

void SideTabStrip::SetDraggedTabBounds(int tab_index,
                                       const gfx::Rect& tab_bounds) {
}

bool SideTabStrip::IsDragSessionActive() const {
  return false;
}

bool SideTabStrip::IsAnimating() const {
  return false;
}

TabStrip* SideTabStrip::AsTabStrip() {
  return NULL;
}

void SideTabStrip::StartHighlight(int model_index) {
}

void SideTabStrip::StopAllHighlighting() {
}

BaseTabRenderer* SideTabStrip::GetBaseTabAtModelIndex(int model_index) const {
  return static_cast<BaseTabRenderer*>(GetChildViewAt(model_index));
}

BaseTabRenderer* SideTabStrip::GetBaseTabAtTabIndex(int tab_index) const {
  return static_cast<BaseTabRenderer*>(GetChildViewAt(tab_index));
}

int SideTabStrip::GetTabCount() const {
  return GetChildViewCount();
}

BaseTabRenderer* SideTabStrip::CreateTabForDragging() {
  return new SideTab(NULL);
}

int SideTabStrip::GetModelIndexOfBaseTab(const BaseTabRenderer* tab) const {
  return GetChildIndex(tab);
}

void SideTabStrip::AddTabAt(int model_index,
                            bool foreground,
                            const TabRendererData& data) {
  SideTab* tab = new SideTab(this);
  AddChildView(tab);
  Layout();
}

void SideTabStrip::RemoveTabAt(int index) {
  View* v = GetChildViewAt(index);
  RemoveChildView(v);
  delete v;
  Layout();
}

void SideTabStrip::SelectTabAt(int old_model_index, int new_model_index) {
  GetChildViewAt(new_model_index)->SchedulePaint();
}

void SideTabStrip::MoveTab(int from_model_index, int to_model_index) {
}

void SideTabStrip::TabTitleChangedNotLoading(int model_index) {
}

void SideTabStrip::SetTabData(int model_index, const TabRendererData& data) {
}

void SideTabStrip::MaybeStartDrag(BaseTabRenderer* tab,
                                  const views::MouseEvent& event) {
}

void SideTabStrip::ContinueDrag(const views::MouseEvent& event) {
}

bool SideTabStrip::EndDrag(bool canceled) {
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// SideTabStrip, views::View overrides:

void SideTabStrip::Layout() {
  gfx::Rect layout_rect = GetLocalBounds(false);
  layout_rect.Inset(kTabStripInset, kTabStripInset);
  int y = layout_rect.y();
  for (int c = GetChildViewCount(), i = 0; i < c; ++i) {
    views::View* child = GetChildViewAt(i);
    child->SetBounds(layout_rect.x(), y, layout_rect.width(),
                     child->GetPreferredSize().height());
    y = child->bounds().bottom() + kVerticalTabSpacing;
  }
}

gfx::Size SideTabStrip::GetPreferredSize() {
  return gfx::Size(kTabStripWidth, 0);
}
