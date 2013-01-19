// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/stacked_panel_collection.h"

#include <algorithm>
#include "base/logging.h"
#include "chrome/browser/ui/panels/detached_panel_collection.h"
#include "chrome/browser/ui/panels/native_panel_stack.h"
#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/panels/panel_manager.h"

StackedPanelCollection::StackedPanelCollection(PanelManager* panel_manager)
    : PanelCollection(PanelCollection::STACKED),
      panel_manager_(panel_manager),
      native_stack_(NULL) {
  native_stack_ = NativePanelStack::Create(make_scoped_ptr(this).Pass());
}

StackedPanelCollection::~StackedPanelCollection() {
  DCHECK(panels_.empty());
}

void StackedPanelCollection::OnDisplayAreaChanged(
    const gfx::Rect& old_display_area) {
}

void StackedPanelCollection::RefreshLayout() {
  if (panels_.empty())
    return;

  Panels::const_iterator iter = panels_.begin();
  Panel* top_panel = *iter;
  gfx::Rect top_panel_bounds = top_panel->GetBounds();
  int common_width = top_panel_bounds.width();
  int common_x = top_panel_bounds.x();
  int y = top_panel_bounds.bottom();

  ++iter;
  for (; iter != panels_.end(); ++iter) {
    Panel* panel = *iter;

    // Don't update the stacked panel that is in preview mode.
    gfx::Rect bounds = panel->GetBounds();
    if (!panel->in_preview_mode()) {
      bounds.set_x(common_x);
      bounds.set_y(y);
      bounds.set_width(common_width);
      panel->SetPanelBounds(bounds);
      gfx::Size full_size = bounds.size();
      full_size.set_width(common_width);
      panel->set_full_size(full_size);
    }

    y += bounds.height();
  }

  // Compute and apply the enclosing bounds.
  gfx::Rect enclosing_bounds = top_panel->GetBounds();
  enclosing_bounds.set_height(
      bottom_panel()->GetBounds().bottom() - enclosing_bounds.y());
  native_stack_->SetBounds(enclosing_bounds);
}

void StackedPanelCollection::AddPanel(Panel* panel,
                                      PositioningMask positioning_mask) {
  DCHECK_NE(this, panel->collection());
  panel->set_collection(this);
  if (positioning_mask & PanelCollection::TOP_POSITION)
    panels_.push_front(panel);
  else
    panels_.push_back(panel);

  if ((positioning_mask & NO_LAYOUT_REFRESH) == 0)
    RefreshLayout();

  native_stack_->OnPanelAddedOrRemoved(panel);
}

void StackedPanelCollection::RemovePanel(Panel* panel) {
  panel->set_collection(NULL);
  panels_.remove(panel);

  RefreshLayout();

  native_stack_->OnPanelAddedOrRemoved(panel);
}

void StackedPanelCollection::CloseAll() {
  // Make a copy as closing panels can modify the iterator.
  Panels panels_copy = panels_;

  for (Panels::const_iterator iter = panels_copy.begin();
       iter != panels_copy.end(); ++iter)
    (*iter)->Close();

  native_stack_->Close();
}

void StackedPanelCollection::OnPanelAttentionStateChanged(Panel* panel) {
}

void StackedPanelCollection::OnPanelTitlebarClicked(
    Panel* panel, panel::ClickModifier modifier) {
}

void StackedPanelCollection::ResizePanelWindow(
    Panel* panel,
    const gfx::Size& preferred_window_size) {
}

void StackedPanelCollection::ActivatePanel(Panel* panel) {
}

void StackedPanelCollection::MinimizePanel(Panel* panel) {
}

void StackedPanelCollection::RestorePanel(Panel* panel) {
}

void StackedPanelCollection::MinimizeAll() {
}

void StackedPanelCollection::RestoreAll() {
}

bool StackedPanelCollection::CanMinimizePanel(const Panel* panel) const {
  return true;
}

bool StackedPanelCollection::IsPanelMinimized(const Panel* panel) const {
  return false;
}

void StackedPanelCollection::SavePanelPlacement(Panel* panel) {
  DCHECK(!saved_panel_placement_.panel);
  saved_panel_placement_.panel = panel;

  if (top_panel() != panel)
    saved_panel_placement_.top_panel = top_panel();
  else
    saved_panel_placement_.position = panel->GetBounds().origin();

  saved_panel_placement_.top_panel = top_panel() != panel ? top_panel() : NULL;
}

void StackedPanelCollection::RestorePanelToSavedPlacement() {
  DCHECK(saved_panel_placement_.panel);

  if (saved_panel_placement_.top_panel) {
    // Restore the top panel if it has been moved out of the stack. This could
    // happen when there're 2 panels in the stack and the bottom panel is being
    // dragged out of the stack and thus cause both panels become detached.
    if (saved_panel_placement_.top_panel->stack() != this) {
      DCHECK_EQ(PanelCollection::DETACHED,
                saved_panel_placement_.top_panel->collection()->type());
      panel_manager_->MovePanelToCollection(saved_panel_placement_.top_panel,
                                            this,
                                            PanelCollection::TOP_POSITION);
    }
  } else {
    // Restore the position when the top panel is being dragged.
    DCHECK_EQ(top_panel(), saved_panel_placement_.panel);
    gfx::Rect new_bounds(saved_panel_placement_.panel->GetBounds());
    new_bounds.set_origin(saved_panel_placement_.position);
    saved_panel_placement_.panel->SetPanelBounds(new_bounds);
  }

  RefreshLayout();

  DiscardSavedPanelPlacement();
}

void StackedPanelCollection::DiscardSavedPanelPlacement() {
  DCHECK(saved_panel_placement_.panel);
  saved_panel_placement_.panel = NULL;
  saved_panel_placement_.top_panel = NULL;
}

panel::Resizability StackedPanelCollection::GetPanelResizability(
    const Panel* panel) const {
  return panel::RESIZABLE_ALL_SIDES;
}

void StackedPanelCollection::OnPanelResizedByMouse(
    Panel* panel, const gfx::Rect& new_bounds) {
}

bool StackedPanelCollection::HasPanel(Panel* panel) const {
  return std::find(panels_.begin(), panels_.end(), panel) != panels_.end();
}

void StackedPanelCollection::UpdatePanelOnCollectionChange(Panel* panel) {
  panel->set_attention_mode(
      static_cast<Panel::AttentionMode>(Panel::USE_PANEL_ATTENTION |
                                        Panel::USE_SYSTEM_ATTENTION));
  panel->SetAlwaysOnTop(false);
  panel->EnableResizeByMouse(true);
  panel->UpdateMinimizeRestoreButtonVisibility();
}

void StackedPanelCollection::OnPanelActiveStateChanged(Panel* panel) {
}

Panel* StackedPanelCollection::GetPanelAbove(Panel* panel) const {
  DCHECK(panel);

  if (panels_.size() < 2)
    return NULL;
  Panels::const_iterator iter = panels_.begin();
  Panel* above_panel = *iter;
  for (; iter != panels_.end(); ++iter) {
    if (*iter == panel)
      return above_panel;
    above_panel = *iter;
  }
  return NULL;
}

void StackedPanelCollection::MoveAllDraggingPanelsInstantly(
    const gfx::Vector2d& delta_origin) {
  for (Panels::const_iterator iter = panels_.begin();
       iter != panels_.end(); iter++) {
    Panel* panel = *iter;
    if (panel->in_preview_mode())
      panel->MoveByInstantly(delta_origin);
  }
}
