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

void DetachedPanelStrip::AddPanel(Panel* panel) {
  DCHECK_EQ(this, panel->panel_strip());
  panels_.insert(panel);
}

bool DetachedPanelStrip::RemovePanel(Panel* panel) {
  return panels_.erase(panel) != 0;
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

bool DetachedPanelStrip::IsPanelMinimized(Panel* panel) const {
  DCHECK_EQ(this, panel->panel_strip());
  NOTIMPLEMENTED();
  return false;
}

bool DetachedPanelStrip::CanShowPanelAsActive(const Panel* panel) const {
  // All detached panels can be shown as active.
  return true;
}

bool DetachedPanelStrip::CanDragPanel(const Panel* panel) const {
  // All detached panels are draggable.
  return true;
}

void DetachedPanelStrip::StartDraggingPanel(Panel* panel) {
}

void DetachedPanelStrip::DragPanel(Panel* panel, int delta_x, int delta_y) {
  gfx::Rect new_bounds(panel->GetBounds());
  new_bounds.Offset(delta_x, delta_y);
  panel->SetPanelBounds(new_bounds);
}

void DetachedPanelStrip::EndDraggingPanel(Panel* panel, bool cancelled) {
  if (cancelled) {
    gfx::Rect new_bounds(panel->GetBounds());
    new_bounds.set_origin(
        panel_manager_->drag_controller()->dragging_panel_original_position());
    panel->SetPanelBounds(new_bounds);
  }
}

bool DetachedPanelStrip::HasPanel(Panel* panel) const {
  return panels_.find(panel) != panels_.end();
}
