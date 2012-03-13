// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/detached_panel_strip.h"

#include <algorithm>
#include "base/logging.h"
#include "chrome/browser/ui/panels/panel_drag_controller.h"
#include "chrome/browser/ui/panels/panel_manager.h"

DetachedPanelStrip::DetachedPanelStrip(PanelManager* panel_manager)
    : PanelStrip(PanelStrip::DETACHED),
      panel_manager_(panel_manager) {
}

DetachedPanelStrip::~DetachedPanelStrip() {
  DCHECK(panels_.empty());
}

void DetachedPanelStrip::SetDisplayArea(const gfx::Rect& display_area) {
  if (display_area_ == display_area)
    return;
  display_area_ = display_area;

  if (panels_.empty())
    return;

  RefreshLayout();
}

void DetachedPanelStrip::RefreshLayout() {
  NOTIMPLEMENTED();
}

void DetachedPanelStrip::AddPanel(Panel* panel,
                                  PositioningMask positioning_mask) {
  // positioning_mask is ignored since the detached panel is free-floating.
  DCHECK_NE(this, panel->panel_strip());
  panel->SetPanelStrip(this);
  panels_.insert(panel);

  panel->SetAlwaysOnTop(false);
}

void DetachedPanelStrip::RemovePanel(Panel* panel) {
  DCHECK_EQ(this, panel->panel_strip());
  panel->SetPanelStrip(NULL);
  panels_.erase(panel);
}

void DetachedPanelStrip::CloseAll() {
  // Make a copy as closing panels can modify the iterator.
  Panels panels_copy = panels_;

  for (Panels::const_iterator iter = panels_copy.begin();
       iter != panels_copy.end(); ++iter)
    (*iter)->Close();
}

void DetachedPanelStrip::OnPanelAttentionStateChanged(Panel* panel) {
  DCHECK_EQ(this, panel->panel_strip());
  NOTIMPLEMENTED();
}

void DetachedPanelStrip::ResizePanelWindow(
    Panel* panel,
    const gfx::Size& preferred_window_size) {
  NOTIMPLEMENTED();
}

void DetachedPanelStrip::ActivatePanel(Panel* panel) {
  DCHECK_EQ(this, panel->panel_strip());
  NOTIMPLEMENTED();
}

void DetachedPanelStrip::MinimizePanel(Panel* panel) {
  DCHECK_EQ(this, panel->panel_strip());
  NOTIMPLEMENTED();
}

void DetachedPanelStrip::RestorePanel(Panel* panel) {
  DCHECK_EQ(this, panel->panel_strip());
  NOTIMPLEMENTED();
}

bool DetachedPanelStrip::IsPanelMinimized(const Panel* panel) const {
  DCHECK_EQ(this, panel->panel_strip());
  NOTIMPLEMENTED();
  return false;
}

bool DetachedPanelStrip::CanShowPanelAsActive(const Panel* panel) const {
  // All detached panels can be shown as active.
  return true;
}

void DetachedPanelStrip::SavePanelPlacement(Panel* panel) {
  DCHECK(!saved_panel_placement_.panel);
  saved_panel_placement_.panel = panel;
  saved_panel_placement_.position = panel->GetBounds().origin();
}

void DetachedPanelStrip::RestorePanelToSavedPlacement() {
  DCHECK(saved_panel_placement_.panel);

  gfx::Rect new_bounds(saved_panel_placement_.panel->GetBounds());
  new_bounds.set_origin(saved_panel_placement_.position);
  saved_panel_placement_.panel->SetPanelBounds(new_bounds);

  DiscardSavedPanelPlacement();
}

void DetachedPanelStrip::DiscardSavedPanelPlacement() {
  DCHECK(saved_panel_placement_.panel);
  saved_panel_placement_.panel = NULL;
}

bool DetachedPanelStrip::CanDragPanel(const Panel* panel) const {
  // All detached panels are draggable.
  return true;
}

void DetachedPanelStrip::StartDraggingPanelWithinStrip(Panel* panel) {
  DCHECK(HasPanel(panel));
}

void DetachedPanelStrip::DragPanelWithinStrip(Panel* panel,
                                              int delta_x,
                                              int delta_y) {
  gfx::Rect new_bounds(panel->GetBounds());
  new_bounds.Offset(delta_x, delta_y);
  panel->SetPanelBoundsInstantly(new_bounds);
}

void DetachedPanelStrip::EndDraggingPanelWithinStrip(Panel* panel,
                                                     bool aborted) {
}

bool DetachedPanelStrip::HasPanel(Panel* panel) const {
  return panels_.find(panel) != panels_.end();
}
