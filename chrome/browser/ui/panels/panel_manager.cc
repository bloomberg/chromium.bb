// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/panel_manager.h"

#include <algorithm>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/window_sizer.h"

namespace {
// Invalid panel index.
const size_t kInvalidPanelIndex = static_cast<size_t>(-1);

// Minimum width and height of a panel.
const int kPanelMinWidthPixels = 64;
const int kPanelMinHeightPixels = 24;

// Default width and height of a panel.
const int kPanelDefaultWidthPixels = 240;
const int kPanelDefaultHeightPixels = 290;

// Maxmium width and height of a panel based on the factor of the working
// area.
const double kPanelMaxWidthFactor = 1.0;
const double kPanelMaxHeightFactor = 0.5;

// Single instance of PanelManager.
scoped_ptr<PanelManager> panel_instance;
}  // namespace

// static
PanelManager* PanelManager::GetInstance() {
  if (!panel_instance.get()) {
    panel_instance.reset(new PanelManager());
  }
  return panel_instance.get();
}

PanelManager::PanelManager()
    : max_width_(0),
      max_height_(0),
      min_x_(0),
      current_x_(0),
      bottom_edge_y_(0),
      dragging_panel_index_(kInvalidPanelIndex),
      dragging_panel_original_x_(0) {
  OnDisplayChanged();
}

PanelManager::~PanelManager() {
  DCHECK(panels_.empty());
  DCHECK(panels_pending_to_remove_.empty());
}

void PanelManager::OnDisplayChanged() {
  scoped_ptr<WindowSizer::MonitorInfoProvider> info_provider(
      WindowSizer::CreateDefaultMonitorInfoProvider());
  SetWorkArea(info_provider->GetPrimaryMonitorWorkArea());
}

void PanelManager::SetWorkArea(const gfx::Rect& work_area) {
  if (work_area == work_area_)
    return;
  work_area_ = work_area;

  min_x_ = work_area.x();
  current_x_ = work_area.right();
  bottom_edge_y_ = work_area.bottom();
  max_width_ = static_cast<int>(work_area.width() * kPanelMaxWidthFactor);
  max_height_ = static_cast<int>(work_area.height() * kPanelMaxHeightFactor);

  Rearrange(panels_.begin());
}

void PanelManager::FindAndClosePanelOnOverflow(const Extension* extension) {
  Panel* panel_to_close = NULL;

  // Try to find the left-most panel invoked from the same extension and close
  // it.
  for (Panels::reverse_iterator iter = panels_.rbegin();
       iter != panels_.rend(); ++iter) {
    if (extension == Panel::GetExtension((*iter)->browser())) {
      panel_to_close = *iter;
      break;
    }
  }

  // If none is found, just pick the left-most panel.
  if (!panel_to_close)
    panel_to_close = panels_.back();

  panel_to_close->Close();
}

Panel* PanelManager::CreatePanel(Browser* browser) {
  const Extension* extension = NULL;
  gfx::Rect bounds = browser->override_bounds();
  while (!ComputeBoundsForNextPanel(&bounds, true)) {
    if (!extension)
      extension = Panel::GetExtension(browser);
    FindAndClosePanelOnOverflow(extension);
  }

  Panel* panel = new Panel(browser, bounds);
  panels_.push_back(panel);

  return panel;
}

void PanelManager::Remove(Panel* panel) {
  // If we're in the process of dragging, delay the removal.
  if (dragging_panel_index_ != kInvalidPanelIndex) {
    panels_pending_to_remove_.push_back(panel);
    return;
  }

  DoRemove(panel);
}

void PanelManager::DelayedRemove() {
  for (size_t i = 0; i < panels_pending_to_remove_.size(); ++i)
    DoRemove(panels_pending_to_remove_[i]);
  panels_pending_to_remove_.clear();
}

void PanelManager::DoRemove(Panel* panel) {
  Panels::iterator iter = find(panels_.begin(), panels_.end(), panel);
  if (iter == panels_.end())
    return;

  gfx::Rect bounds = (*iter)->GetBounds();
  current_x_ = bounds.x() + bounds.width();
  Rearrange(panels_.erase(iter));
}

void PanelManager::StartDragging(Panel* panel) {
  for (size_t i = 0; i < panels_.size(); ++i) {
    if (panels_[i] == panel) {
      dragging_panel_index_ = i;
      dragging_panel_bounds_ = panel->GetBounds();
      dragging_panel_original_x_ = dragging_panel_bounds_.x();
      break;
    }
  }
}

void PanelManager::Drag(int delta_x) {
  DCHECK(dragging_panel_index_ != kInvalidPanelIndex);

  if (!delta_x)
    return;

  // Moves this panel to the dragging position.
  Panel* dragging_panel = panels_[dragging_panel_index_];
  gfx::Rect new_bounds(dragging_panel->GetBounds());
  new_bounds.set_x(new_bounds.x() + delta_x);
  dragging_panel->SetPanelBounds(new_bounds);

  // Checks and processes other affected panels.
  if (delta_x > 0)
    DragRight();
  else
    DragLeft();
}

void PanelManager::DragLeft() {
  Panel* dragging_panel = panels_[dragging_panel_index_];

  // This is the left corner of the dragging panel. We use it to check against
  // all the panels on its left.
  int dragging_panel_left_boundary = dragging_panel->GetBounds().x();

  // This is the right corner which a panel will be moved to.
  int current_panel_right_boundary =
      dragging_panel_bounds_.x() + dragging_panel_bounds_.width();

  // Checks the panels to the left of the dragging panel.
  size_t current_panel_index = dragging_panel_index_ + 1;
  for (; current_panel_index < panels_.size(); ++current_panel_index) {
    Panel* current_panel = panels_[current_panel_index];

    // Current panel will only be affected if the left corner of dragging
    // panel goes beyond the middle position of the current panel.
    if (dragging_panel_left_boundary > current_panel->GetBounds().x() +
            current_panel->GetBounds().width() / 2)
      break;

    // Moves current panel to the new position.
    gfx::Rect bounds(current_panel->GetBounds());
    bounds.set_x(current_panel_right_boundary - bounds.width());
    current_panel_right_boundary -= bounds.width() + kPanelsHorizontalSpacing;
    current_panel->SetPanelBounds(bounds);

    // Updates the index of current panel since it has been moved to the
    // position of previous panel.
    panels_[current_panel_index - 1] = current_panel;
  }

  // Updates the position and index of dragging panel as the result of moving
  // other affected panels.
  if (current_panel_index != dragging_panel_index_ + 1) {
    dragging_panel_bounds_.set_x(current_panel_right_boundary -
                                 dragging_panel_bounds_.width());
    dragging_panel_index_ = current_panel_index - 1;
    panels_[dragging_panel_index_] = dragging_panel;
  }
}

void PanelManager::DragRight() {
  Panel* dragging_panel = panels_[dragging_panel_index_];

  // This is the right corner of the dragging panel. We use it to check against
  // all the panels on its right.
  int dragging_panel_right_boundary = dragging_panel->GetBounds().x() +
      dragging_panel->GetBounds().width() - 1;

  // This is the left corner which a panel will be moved to.
  int current_panel_left_boundary = dragging_panel_bounds_.x();

  // Checks the panels to the right of the dragging panel.
  int current_panel_index = static_cast<int>(dragging_panel_index_) - 1;
  for (; current_panel_index >= 0; --current_panel_index) {
    Panel* current_panel = panels_[current_panel_index];

    // Current panel will only be affected if the right corner of dragging
    // panel goes beyond the middle position of the current panel.
    if (dragging_panel_right_boundary < current_panel->GetBounds().x() +
            current_panel->GetBounds().width() / 2)
      break;

    // Moves current panel to the new position.
    gfx::Rect bounds(current_panel->GetBounds());
    bounds.set_x(current_panel_left_boundary);
    current_panel_left_boundary += bounds.width() + kPanelsHorizontalSpacing;
    current_panel->SetPanelBounds(bounds);

    // Updates the index of current panel since it has been moved to the
    // position of previous panel.
    panels_[current_panel_index + 1] = current_panel;
  }

  // Updates the position and index of dragging panel as the result of moving
  // other affected panels.
  if (current_panel_index != static_cast<int>(dragging_panel_index_) - 1) {
    dragging_panel_bounds_.set_x(current_panel_left_boundary);
    dragging_panel_index_ = current_panel_index + 1;
    panels_[dragging_panel_index_] = dragging_panel;
  }
}

void PanelManager::EndDragging(bool cancelled) {
  DCHECK(dragging_panel_index_ != kInvalidPanelIndex);

  if (cancelled) {
    Drag(dragging_panel_original_x_ -
         panels_[dragging_panel_index_]->GetBounds().x());
  } else {
    panels_[dragging_panel_index_]->SetPanelBounds(
        dragging_panel_bounds_);
  }

  dragging_panel_index_ = kInvalidPanelIndex;

  DelayedRemove();
}

bool PanelManager::ShouldBringUpTitlebarForAllMinimizedPanels(
    int mouse_x, int mouse_y) const {
  for (Panels::const_iterator iter = panels_.begin();
       iter != panels_.end(); ++iter) {
    if ((*iter)->ShouldBringUpTitlebar(mouse_x, mouse_y))
      return true;
  }
  return false;
}

void PanelManager::BringUpOrDownTitlebarForAllMinimizedPanels(bool bring_up) {
  for (Panels::const_iterator iter = panels_.begin();
       iter != panels_.end(); ++iter) {
    Panel* panel = *iter;

    // Skip any panel that is drawing the attention.
    if (panel->IsDrawingAttention())
      continue;

    if (bring_up) {
      if (panel->expansion_state() == Panel::MINIMIZED)
        panel->SetExpansionState(Panel::TITLE_ONLY);
    } else {
      if (panel->expansion_state() == Panel::TITLE_ONLY)
        panel->SetExpansionState(Panel::MINIMIZED);
    }
  }
}

void PanelManager::Rearrange(Panels::iterator iter_to_start) {
  if (iter_to_start == panels_.end())
    return;

  for (Panels::iterator iter = iter_to_start; iter != panels_.end(); ++iter) {
    gfx::Rect new_bounds((*iter)->GetBounds());
    ComputeBoundsForNextPanel(&new_bounds, false);
    if (new_bounds != (*iter)->GetBounds())
      (*iter)->SetPanelBounds(new_bounds);
  }
}

bool PanelManager::ComputeBoundsForNextPanel(gfx::Rect* bounds,
                                             bool allow_size_change) {
  int width = bounds->width();
  int height = bounds->height();

  // Update the width and/or height to fit into our constraint.
  if (allow_size_change) {
    if (width == 0 && height == 0) {
      width = kPanelDefaultWidthPixels;
      height = kPanelDefaultHeightPixels;
    }

    if (width < kPanelMinWidthPixels)
      width = kPanelMinWidthPixels;
    else if (width > max_width_)
      width = max_width_;

    if (height < kPanelMinHeightPixels)
      height = kPanelMinHeightPixels;
    else if (height > max_height_)
      height = max_height_;
  }

  int x = current_x_ - width;
  int y = bottom_edge_y_ - height;

  if (x < min_x_)
    return false;

  current_x_ -= width + kPanelsHorizontalSpacing;

  bounds->SetRect(x, y, width, height);
  return true;
}

void PanelManager::RemoveAll() {
  // This should not be called when we're in the process of dragging.
  DCHECK(dragging_panel_index_ == kInvalidPanelIndex);

  // Make a copy of the iterator as closing panels can modify the vector.
  Panels panels_copy = panels_;

  // Start from the bottom to avoid reshuffling.
  for (Panels::reverse_iterator iter = panels_copy.rbegin();
       iter != panels_copy.rend(); ++iter)
    (*iter)->Close();
}

bool PanelManager::is_dragging_panel() const {
  return dragging_panel_index_ != kInvalidPanelIndex;
}
