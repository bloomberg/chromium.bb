// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/tabs/side_tab_strip.h"

#include "chrome/browser/views/tabs/side_tab.h"
#include "chrome/browser/view_ids.h"
#include "views/background.h"

namespace {
const int kVerticalTabSpacing = 2;
const int kTabStripWidth = 140;
const SkColor kBackgroundColor = SkColorSetARGB(255, 209, 220, 248);
}

// static
const int SideTabStrip::kTabStripInset = 3;

////////////////////////////////////////////////////////////////////////////////
// SideTabStrip, public:

SideTabStrip::SideTabStrip(TabStripController* controller)
    : BaseTabStrip(controller, BaseTabStrip::VERTICAL_TAB_STRIP) {
  SetID(VIEW_ID_TAB_STRIP);
  set_background(views::Background::CreateSolidBackground(kBackgroundColor));
}

SideTabStrip::~SideTabStrip() {
  DestroyDragController();
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

TabStrip* SideTabStrip::AsTabStrip() {
  return NULL;
}

void SideTabStrip::StartHighlight(int model_index) {
}

void SideTabStrip::StopAllHighlighting() {
}

BaseTab* SideTabStrip::CreateTabForDragging() {
  SideTab* tab = new SideTab(NULL);
  // Make sure the dragged tab shares our theme provider. We need to explicitly
  // do this as during dragging there isn't a theme provider.
  tab->set_theme_provider(GetThemeProvider());
  return tab;
}

void SideTabStrip::RemoveTabAt(int model_index, bool initiated_close) {
  StartRemoveTabAnimation(model_index);
}

void SideTabStrip::SelectTabAt(int old_model_index, int new_model_index) {
  GetBaseTabAtModelIndex(new_model_index)->SchedulePaint();
}

void SideTabStrip::TabTitleChangedNotLoading(int model_index) {
}

void SideTabStrip::SetTabData(int model_index, const TabRendererData& data) {
  BaseTab* tab = GetBaseTabAtModelIndex(model_index);
  tab->SetData(data);
  tab->SchedulePaint();
}

gfx::Size SideTabStrip::GetPreferredSize() {
  return gfx::Size(kTabStripWidth, 0);
}

void SideTabStrip::PaintChildren(gfx::Canvas* canvas) {
  // Make sure the dragged tab appears on top of all others by paint it last.
  BaseTab* dragging_tab = NULL;

  for (int i = tab_count() - 1; i >= 0; --i) {
    BaseTab* tab = base_tab_at_tab_index(i);
    if (tab->dragging())
      dragging_tab = tab;
    else
      tab->ProcessPaint(canvas);
  }

  if (dragging_tab)
    dragging_tab->ProcessPaint(canvas);
}

BaseTab* SideTabStrip::CreateTab() {
  return new SideTab(this);
}

void SideTabStrip::GenerateIdealBounds() {
  gfx::Rect layout_rect = GetLocalBounds(false);
  layout_rect.Inset(kTabStripInset, kTabStripInset);

  int y = layout_rect.y();
  for (int i = 0; i < tab_count(); ++i) {
    BaseTab* tab = base_tab_at_tab_index(i);
    if (!tab->closing()) {
      gfx::Rect bounds = gfx::Rect(layout_rect.x(), y, layout_rect.width(),
                                   tab->GetPreferredSize().height());
      set_ideal_bounds(i, bounds);
      y = bounds.bottom() + kVerticalTabSpacing;
    }
  }
}

void SideTabStrip::StartInsertTabAnimation(int model_index, bool foreground) {
  PrepareForAnimation();

  GenerateIdealBounds();

  int tab_data_index = ModelIndexToTabIndex(model_index);
  BaseTab* tab = base_tab_at_tab_index(tab_data_index);
  if (model_index == 0) {
    tab->SetBounds(ideal_bounds(tab_data_index).x(), 0,
                   ideal_bounds(tab_data_index).width(), 0);
  } else {
    BaseTab* last_tab = base_tab_at_tab_index(tab_data_index - 1);
    tab->SetBounds(last_tab->x(), last_tab->bounds().bottom(),
                   ideal_bounds(tab_data_index).width(), 0);
  }

  AnimateToIdealBounds();
}

void SideTabStrip::StartMoveTabAnimation() {
  PrepareForAnimation();

  GenerateIdealBounds();
  AnimateToIdealBounds();
}

void SideTabStrip::StopAnimating(bool layout) {
  if (!IsAnimating())
    return;

  bounds_animator().Cancel();

  if (layout)
    Layout();
}

void SideTabStrip::AnimateToIdealBounds() {
  for (int i = 0; i < tab_count(); ++i) {
    BaseTab* tab = base_tab_at_tab_index(i);
    if (!tab->closing() && !tab->dragging())
      bounds_animator().AnimateViewTo(tab, ideal_bounds(i));
  }
}
