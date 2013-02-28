// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/detached_panel_collection.h"

#include <algorithm>
#include "base/logging.h"
#include "chrome/browser/ui/panels/panel_drag_controller.h"
#include "chrome/browser/ui/panels/panel_manager.h"

namespace {
// How much horizontal and vertical offset there is between newly opened
// detached panels.
const int kPanelTilePixels = 10;
}  // namespace

DetachedPanelCollection::DetachedPanelCollection(PanelManager* panel_manager)
    : PanelCollection(PanelCollection::DETACHED),
      panel_manager_(panel_manager) {
}

DetachedPanelCollection::~DetachedPanelCollection() {
  DCHECK(panels_.empty());
}

void DetachedPanelCollection::OnDisplayAreaChanged(
    const gfx::Rect& old_display_area) {
  const gfx::Rect display_area = panel_manager_->display_area();

  for (Panels::const_iterator iter = panels_.begin();
       iter != panels_.end(); ++iter) {
    Panel* panel = *iter;

    // If the detached panel is outside the main display area, don't change it.
    if (!old_display_area.Intersects(panel->GetBounds()))
      continue;

    // Update size if needed.
    panel->LimitSizeToDisplayArea(display_area);

    // Update bounds if needed.
    gfx::Rect bounds = panel->GetBounds();
    if (panel->full_size() != bounds.size()) {
      bounds.set_size(panel->full_size());
      if (bounds.right() > display_area.right())
        bounds.set_x(display_area.right() - bounds.width());
      if (bounds.bottom() > display_area.bottom())
        bounds.set_y(display_area.bottom() - bounds.height());
      panel->SetPanelBoundsInstantly(bounds);
    }
  }
}

void DetachedPanelCollection::RefreshLayout() {
  // A detached panel would still maintain its minimized state when it was
  // moved out the stack and the drag has not ended. When the drag ends, it
  // needs to be expanded. This could occur in the following scenarios:
  // 1) It was originally a minimized panel that was dragged out of a stack.
  // 2) It was originally a minimized panel that was the top panel in a stack.
  //    The panel below it was dragged out of the stack which also caused
  //    the top panel became detached.
  for (Panels::const_iterator iter = panels_.begin();
       iter != panels_.end(); ++iter) {
    Panel* panel = *iter;
    if (!panel->in_preview_mode() &&
        panel->expansion_state() != Panel::EXPANDED)
      panel->SetExpansionState(Panel::EXPANDED);
  }
}

void DetachedPanelCollection::AddPanel(Panel* panel,
                                  PositioningMask positioning_mask) {
  // positioning_mask is ignored since the detached panel is free-floating.
  DCHECK_NE(this, panel->collection());
  panel->set_collection(this);
  panels_.push_back(panel);

  // Offset the default position of the next detached panel if the current
  // default position is used.
  if (panel->GetBounds().origin() == default_panel_origin_)
    ComputeNextDefaultPanelOrigin();
}

void DetachedPanelCollection::RemovePanel(Panel* panel, RemovalReason reason) {
  DCHECK_EQ(this, panel->collection());
  panel->set_collection(NULL);
  panels_.remove(panel);
}

void DetachedPanelCollection::CloseAll() {
  // Make a copy as closing panels can modify the iterator.
  Panels panels_copy = panels_;

  for (Panels::const_iterator iter = panels_copy.begin();
       iter != panels_copy.end(); ++iter)
    (*iter)->Close();
}

void DetachedPanelCollection::OnPanelAttentionStateChanged(Panel* panel) {
  DCHECK_EQ(this, panel->collection());
  // Nothing to do.
}

void DetachedPanelCollection::OnPanelTitlebarClicked(Panel* panel,
                                                panel::ClickModifier modifier) {
  DCHECK_EQ(this, panel->collection());
  // Click on detached panel titlebars does not do anything.
}

void DetachedPanelCollection::ResizePanelWindow(
    Panel* panel,
    const gfx::Size& preferred_window_size) {
  // We should get this call only of we have the panel.
  DCHECK_EQ(this, panel->collection());

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

void DetachedPanelCollection::ActivatePanel(Panel* panel) {
  DCHECK_EQ(this, panel->collection());
  // No change in panel's appearance.
}

void DetachedPanelCollection::MinimizePanel(Panel* panel) {
  DCHECK_EQ(this, panel->collection());
  // Detached panels do not minimize. However, extensions may call this API
  // regardless of which collection the panel is in. So we just quietly return.
}

void DetachedPanelCollection::RestorePanel(Panel* panel) {
  DCHECK_EQ(this, panel->collection());
  // Detached panels do not minimize. However, extensions may call this API
  // regardless of which collection the panel is in. So we just quietly return.
}

void DetachedPanelCollection::OnMinimizeButtonClicked(
    Panel* panel, panel::ClickModifier modifier) {
  // Detached panels do not minimize.
  NOTREACHED();
}

void DetachedPanelCollection::OnRestoreButtonClicked(
    Panel* panel, panel::ClickModifier modifier) {
  // Detached panels do not minimize.
  NOTREACHED();
}

bool DetachedPanelCollection::CanShowMinimizeButton(const Panel* panel) const {
  // Detached panels do not minimize.
  return false;
}

bool DetachedPanelCollection::CanShowRestoreButton(const Panel* panel) const {
  // Detached panels do not minimize.
  return false;
}

bool DetachedPanelCollection::IsPanelMinimized(const Panel* panel) const {
  DCHECK_EQ(this, panel->collection());
  // Detached panels do not minimize.
  return false;
}

void DetachedPanelCollection::SavePanelPlacement(Panel* panel) {
  DCHECK(!saved_panel_placement_.panel);
  saved_panel_placement_.panel = panel;
  saved_panel_placement_.position = panel->GetBounds().origin();
}

void DetachedPanelCollection::RestorePanelToSavedPlacement() {
  DCHECK(saved_panel_placement_.panel);

  gfx::Rect new_bounds(saved_panel_placement_.panel->GetBounds());
  new_bounds.set_origin(saved_panel_placement_.position);
  saved_panel_placement_.panel->SetPanelBounds(new_bounds);

  DiscardSavedPanelPlacement();
}

void DetachedPanelCollection::DiscardSavedPanelPlacement() {
  DCHECK(saved_panel_placement_.panel);
  saved_panel_placement_.panel = NULL;
}

panel::Resizability DetachedPanelCollection::GetPanelResizability(
    const Panel* panel) const {
  return panel::RESIZABLE_ALL;
}

void DetachedPanelCollection::OnPanelResizedByMouse(
    Panel* panel, const gfx::Rect& new_bounds) {
  DCHECK_EQ(this, panel->collection());
  panel->set_full_size(new_bounds.size());
}

bool DetachedPanelCollection::HasPanel(Panel* panel) const {
  return std::find(panels_.begin(), panels_.end(), panel) != panels_.end();
}

void DetachedPanelCollection::SortPanels(PanelsComparer comparer) {
  panels_.sort(comparer);
}

void DetachedPanelCollection::UpdatePanelOnCollectionChange(Panel* panel) {
  panel->set_attention_mode(
      static_cast<Panel::AttentionMode>(Panel::USE_PANEL_ATTENTION |
                                        Panel::USE_SYSTEM_ATTENTION));
  panel->SetAlwaysOnTop(false);
  panel->EnableResizeByMouse(true);
  panel->UpdateMinimizeRestoreButtonVisibility();
  panel->SetWindowCornerStyle(panel::ALL_ROUNDED);
}

void DetachedPanelCollection::OnPanelExpansionStateChanged(Panel* panel) {
  // This should only be reached when a minimized stacked panel is dragged out
  // of the stack to become detached. For this case, the panel needs to be
  // restored.
  DCHECK_EQ(Panel::EXPANDED, panel->expansion_state());

  gfx::Rect bounds = panel->GetBounds();
  bounds.set_height(panel->full_size().height());
  panel->SetPanelBounds(bounds);
}

void DetachedPanelCollection::OnPanelActiveStateChanged(Panel* panel) {
}

gfx::Point DetachedPanelCollection::GetDefaultPanelOrigin() {
  if (!default_panel_origin_.x() && !default_panel_origin_.y()) {
    gfx::Rect display_area =
        panel_manager_->display_settings_provider()->GetDisplayArea();
    default_panel_origin_.SetPoint(kPanelTilePixels + display_area.x(),
                                   kPanelTilePixels + display_area.y());
  }
  return default_panel_origin_;
}

void DetachedPanelCollection::ComputeNextDefaultPanelOrigin() {
  default_panel_origin_.Offset(kPanelTilePixels, kPanelTilePixels);
  gfx::Rect display_area =
      panel_manager_->display_settings_provider()->GetDisplayArea();
  if (!display_area.Contains(default_panel_origin_)) {
    default_panel_origin_.SetPoint(kPanelTilePixels + display_area.x(),
                                   kPanelTilePixels + display_area.y());
  }
}
