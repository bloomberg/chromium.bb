// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/detached_panel_strip.h"

#include <algorithm>
#include "base/logging.h"
#include "chrome/browser/ui/panels/panel_drag_controller.h"
#include "chrome/browser/ui/panels/panel_manager.h"

namespace {
// How much horizontal and vertical offset there is between newly opened
// detached panels.
const int kPanelTilePixels = 10;
}  // namespace

DetachedPanelStrip::DetachedPanelStrip(PanelManager* panel_manager)
    : PanelStrip(PanelStrip::DETACHED),
      panel_manager_(panel_manager) {
}

DetachedPanelStrip::~DetachedPanelStrip() {
  DCHECK(panels_.empty());
}

gfx::Rect DetachedPanelStrip::GetDisplayArea() const  {
  return display_area_;
}

void DetachedPanelStrip::SetDisplayArea(const gfx::Rect& display_area) {
  if (display_area_ == display_area)
    return;
  gfx::Rect old_display_area = display_area_;
  display_area_ = display_area;

  if (panels_.empty())
    return;

  for (Panels::const_iterator iter = panels_.begin();
       iter != panels_.end(); ++iter) {
    Panel* panel = *iter;

    // If the detached panel is outside the main display area, don't change it.
    if (!old_display_area.Intersects(panel->GetBounds()))
      continue;

    // Update size if needed.
    panel->LimitSizeToDisplayArea(display_area_);

    // Update bounds if needed.
    gfx::Rect bounds = panel->GetBounds();
    if (panel->full_size() != bounds.size()) {
      bounds.set_size(panel->full_size());
      if (bounds.right() > display_area_.right())
        bounds.set_x(display_area_.right() - bounds.width());
      if (bounds.bottom() > display_area_.bottom())
        bounds.set_y(display_area_.bottom() - bounds.height());
      panel->SetPanelBoundsInstantly(bounds);
    }
  }
}

void DetachedPanelStrip::RefreshLayout() {
  // Nothing needds to be done here: detached panels always stay
  // where the user dragged them.
}

void DetachedPanelStrip::AddPanel(Panel* panel,
                                  PositioningMask positioning_mask) {
  // positioning_mask is ignored since the detached panel is free-floating.
  DCHECK_NE(this, panel->panel_strip());
  panel->set_panel_strip(this);
  panels_.insert(panel);

  // Offset the default position of the next detached panel if the current
  // default position is used.
  if (panel->GetBounds().origin() == default_panel_origin_)
    ComputeNextDefaultPanelOrigin();
}

void DetachedPanelStrip::RemovePanel(Panel* panel) {
  DCHECK_EQ(this, panel->panel_strip());
  panel->set_panel_strip(NULL);
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
  // Nothing to do.
}

void DetachedPanelStrip::OnPanelTitlebarClicked(Panel* panel,
                                                panel::ClickModifier modifier) {
  DCHECK_EQ(this, panel->panel_strip());
  // Click on detached panel titlebars does not do anything.
}

void DetachedPanelStrip::ResizePanelWindow(
    Panel* panel,
    const gfx::Size& preferred_window_size) {
  // We should get this call only of we have the panel.
  DCHECK_EQ(this, panel->panel_strip());

  // Make sure the new size does not violate panel's size restrictions.
  gfx::Size new_size(preferred_window_size.width(),
                     preferred_window_size.height());
  new_size = panel->ClampSize(new_size);

  // Update restored size.
  if (new_size != panel->full_size())
    panel->set_full_size(new_size);

  gfx::Rect bounds = panel->GetBounds();

  // When we resize a detached panel, its origin does not move.
  // So we set height and width only.
  bounds.set_size(new_size);

  if (bounds != panel->GetBounds())
    panel->SetPanelBounds(bounds);
}

void DetachedPanelStrip::ActivatePanel(Panel* panel) {
  DCHECK_EQ(this, panel->panel_strip());
  // No change in panel's appearance.
}

void DetachedPanelStrip::MinimizePanel(Panel* panel) {
  DCHECK_EQ(this, panel->panel_strip());
  // Detached panels do not minimize. However, extensions may call this API
  // regardless of which strip the panel is in. So we just quietly return.
}

void DetachedPanelStrip::RestorePanel(Panel* panel) {
  DCHECK_EQ(this, panel->panel_strip());
  // Detached panels do not minimize. However, extensions may call this API
  // regardless of which strip the panel is in. So we just quietly return.
}

void DetachedPanelStrip::MinimizeAll() {
  // Detached panels do not minimize.
  NOTREACHED();
}

void DetachedPanelStrip::RestoreAll() {
  // Detached panels do not minimize.
  NOTREACHED();
}

bool DetachedPanelStrip::CanMinimizePanel(const Panel* panel) const {
  DCHECK_EQ(this, panel->panel_strip());
  // Detached panels do not minimize.
  return false;
}

bool DetachedPanelStrip::IsPanelMinimized(const Panel* panel) const {
  DCHECK_EQ(this, panel->panel_strip());
  // Detached panels do not minimize.
  return false;
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

void DetachedPanelStrip::StartDraggingPanelWithinStrip(Panel* panel) {
  DCHECK(HasPanel(panel));
}

void DetachedPanelStrip::DragPanelWithinStrip(
    Panel* panel,
    const gfx::Point& target_position) {
  gfx::Rect new_bounds(panel->GetBounds());
  new_bounds.set_origin(target_position);
  panel->SetPanelBoundsInstantly(new_bounds);
}

void DetachedPanelStrip::EndDraggingPanelWithinStrip(Panel* panel,
                                                     bool aborted) {
}

void DetachedPanelStrip::ClearDraggingStateWhenPanelClosed() {
}


panel::Resizability DetachedPanelStrip::GetPanelResizability(
    const Panel* panel) const {
  return panel::RESIZABLE_ALL_SIDES;
}

void DetachedPanelStrip::OnPanelResizedByMouse(Panel* panel,
                                               const gfx::Rect& new_bounds) {
  DCHECK_EQ(this, panel->panel_strip());
  panel->set_full_size(new_bounds.size());

  panel->SetPanelBoundsInstantly(new_bounds);
}

bool DetachedPanelStrip::HasPanel(Panel* panel) const {
  return panels_.find(panel) != panels_.end();
}

void DetachedPanelStrip::UpdatePanelOnStripChange(Panel* panel) {
  panel->set_attention_mode(
      static_cast<Panel::AttentionMode>(Panel::USE_PANEL_ATTENTION |
                                        Panel::USE_SYSTEM_ATTENTION));
  panel->SetAlwaysOnTop(false);
  panel->EnableResizeByMouse(true);
  panel->UpdateMinimizeRestoreButtonVisibility();
}

void DetachedPanelStrip::OnPanelActiveStateChanged(Panel* panel) {
}

gfx::Point DetachedPanelStrip::GetDefaultPanelOrigin() {
  if (!default_panel_origin_.x() && !default_panel_origin_.y()) {
    gfx::Rect display_area =
        panel_manager_->display_settings_provider()->GetDisplayArea();
    default_panel_origin_.SetPoint(kPanelTilePixels + display_area.x(),
                                   kPanelTilePixels + display_area.y());
  }
  return default_panel_origin_;
}

void DetachedPanelStrip::ComputeNextDefaultPanelOrigin() {
  default_panel_origin_.Offset(kPanelTilePixels, kPanelTilePixels);
  gfx::Rect display_area =
      panel_manager_->display_settings_provider()->GetDisplayArea();
  if (!display_area.Contains(default_panel_origin_)) {
    default_panel_origin_.SetPoint(kPanelTilePixels + display_area.x(),
                                   kPanelTilePixels + display_area.y());
  }
}
