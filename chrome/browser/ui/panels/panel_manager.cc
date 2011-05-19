// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/panel_manager.h"

#include <algorithm>
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/panels/panel.h"
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

// Horizontal spacing between two panels.
const int kPanelsHorizontalSpacing = 4;

// Single instance of PanelManager.
scoped_ptr<PanelManager> panel_instance;
} // namespace

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
  DCHECK(active_panels_.empty());
  DCHECK(pending_panels_.empty());
  DCHECK(panels_pending_to_remove_.empty());
}

void PanelManager::OnDisplayChanged() {
  scoped_ptr<WindowSizer::MonitorInfoProvider> info_provider(
      WindowSizer::CreateDefaultMonitorInfoProvider());
  gfx::Rect work_area = info_provider->GetPrimaryMonitorWorkArea();

  min_x_ = work_area.x();
  current_x_ = work_area.right();
  bottom_edge_y_ = work_area.bottom();
  max_width_ = static_cast<int>(work_area.width() * kPanelMaxWidthFactor);
  max_height_ = static_cast<int>(work_area.height() * kPanelMaxHeightFactor);

  Rearrange(active_panels_.begin());
}

Panel* PanelManager::CreatePanel(Browser* browser) {
  gfx::Rect bounds = browser->override_bounds();
  bool is_within_bounds = ComputeBoundsForNextPanel(&bounds, true);

  Panel* panel = new Panel(browser, bounds);
  if (is_within_bounds)
    active_panels_.push_back(panel);
  else
    pending_panels_.push_back(panel);

  return panel;
}

void PanelManager::ProcessPending() {
  while (!pending_panels_.empty()) {
    Panel* panel = pending_panels_.front();
    gfx::Rect bounds = panel->GetRestoredBounds();
    if (ComputeBoundsForNextPanel(&bounds, true)) {
      // TODO(jianli): More work to support displaying pending panels.
      active_panels_.push_back(panel);
      pending_panels_.pop_front();
    }
  }
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
  // Checks the active panel list.
  ActivePanels::iterator iter =
      find(active_panels_.begin(), active_panels_.end(), panel);
  if (iter == active_panels_.end()) {
    // Checks the pending panel list.
    PendingPanels::iterator iter2 =
        find(pending_panels_.begin(), pending_panels_.end(), panel);
    if (iter2 != pending_panels_.end())
      pending_panels_.erase(iter2);
    return;
  }

  gfx::Rect bounds = (*iter)->GetRestoredBounds();
  current_x_ = bounds.x() + bounds.width();
  Rearrange(active_panels_.erase(iter));

  ProcessPending();
}

void PanelManager::StartDragging(Panel* panel) {
  for (size_t i = 0; i < active_panels_.size(); ++i) {
    if (active_panels_[i] == panel) {
      dragging_panel_index_ = i;
      dragging_panel_bounds_ = panel->GetRestoredBounds();
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
  Panel* dragging_panel = active_panels_[dragging_panel_index_];
  gfx::Rect new_bounds(dragging_panel->GetRestoredBounds());
  new_bounds.set_x(new_bounds.x() + delta_x);
  dragging_panel->SetPanelBounds(new_bounds);

  // Checks and processes other affected panels.
  if (delta_x > 0)
    DragRight();
  else
    DragLeft();
}

void PanelManager::DragLeft() {
  Panel* dragging_panel = active_panels_[dragging_panel_index_];

  // This is the left corner of the dragging panel. We use it to check against
  // all the panels on its left.
  int dragging_panel_left_boundary = dragging_panel->GetRestoredBounds().x();

  // This is the right corner which a panel will be moved to.
  int current_panel_right_boundary =
      dragging_panel_bounds_.x() + dragging_panel_bounds_.width();

  // Checks the panels to the left of the dragging panel.
  size_t current_panel_index = dragging_panel_index_ + 1;
  for (; current_panel_index < active_panels_.size(); ++current_panel_index) {
    Panel* current_panel = active_panels_[current_panel_index];

    // Current panel will only be affected if the left corner of dragging
    // panel goes beyond the middle position of the current panel.
    if (dragging_panel_left_boundary > current_panel->GetRestoredBounds().x() +
            current_panel->GetRestoredBounds().width() / 2)
      break;

    // Moves current panel to the new position.
    gfx::Rect bounds(current_panel->GetRestoredBounds());
    bounds.set_x(current_panel_right_boundary - bounds.width());
    current_panel_right_boundary -= bounds.width() + kPanelsHorizontalSpacing;
    current_panel->SetPanelBounds(bounds);

    // Updates the index of current panel since it has been moved to the
    // position of previous panel.
    active_panels_[current_panel_index - 1] = current_panel;
  }

  // Updates the position and index of dragging panel as the result of moving
  // other affected panels.
  if (current_panel_index != dragging_panel_index_ + 1) {
    dragging_panel_bounds_.set_x(current_panel_right_boundary -
                                 dragging_panel_bounds_.width());
    dragging_panel_index_ = current_panel_index - 1;
    active_panels_[dragging_panel_index_] = dragging_panel;
  }
}

void PanelManager::DragRight() {
  Panel* dragging_panel = active_panels_[dragging_panel_index_];

  // This is the right corner of the dragging panel. We use it to check against
  // all the panels on its right.
  int dragging_panel_right_boundary = dragging_panel->GetRestoredBounds().x() +
      dragging_panel->GetRestoredBounds().width() - 1;

  // This is the left corner which a panel will be moved to.
  int current_panel_left_boundary = dragging_panel_bounds_.x();

  // Checks the panels to the right of the dragging panel.
  int current_panel_index = static_cast<int>(dragging_panel_index_) - 1;
  for (; current_panel_index >= 0; --current_panel_index) {
    Panel* current_panel = active_panels_[current_panel_index];

    // Current panel will only be affected if the right corner of dragging
    // panel goes beyond the middle position of the current panel.
    if (dragging_panel_right_boundary < current_panel->GetRestoredBounds().x() +
            current_panel->GetRestoredBounds().width() / 2)
      break;

    // Moves current panel to the new position.
    gfx::Rect bounds(current_panel->GetRestoredBounds());
    bounds.set_x(current_panel_left_boundary);
    current_panel_left_boundary += bounds.width() + kPanelsHorizontalSpacing;
    current_panel->SetPanelBounds(bounds);

    // Updates the index of current panel since it has been moved to the
    // position of previous panel.
    active_panels_[current_panel_index + 1] = current_panel;
  }

  // Updates the position and index of dragging panel as the result of moving
  // other affected panels.
  if (current_panel_index != static_cast<int>(dragging_panel_index_) - 1) {
    dragging_panel_bounds_.set_x(current_panel_left_boundary);
    dragging_panel_index_ = current_panel_index + 1;
    active_panels_[dragging_panel_index_] = dragging_panel;
  }
}

void PanelManager::EndDragging(bool cancelled) {
  DCHECK(dragging_panel_index_ != kInvalidPanelIndex);

  if (cancelled) {
    Drag(dragging_panel_original_x_ -
         active_panels_[dragging_panel_index_]->GetRestoredBounds().x());
  } else {
    active_panels_[dragging_panel_index_]->SetPanelBounds(
        dragging_panel_bounds_);
  }

  dragging_panel_index_ = kInvalidPanelIndex;

  DelayedRemove();
}

void PanelManager::Rearrange(ActivePanels::iterator iter_to_start) {
  if (iter_to_start == active_panels_.end())
    return;

  for (ActivePanels::iterator iter = iter_to_start;
       iter != active_panels_.end(); ++iter) {
    gfx::Rect new_bounds((*iter)->GetRestoredBounds());
    ComputeBoundsForNextPanel(&new_bounds, false);
    if (new_bounds != (*iter)->GetRestoredBounds())
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

void PanelManager::MinimizeAll() {
  for (ActivePanels::const_iterator iter = active_panels_.begin();
       iter != active_panels_.end(); ++iter) {
    (*iter)->Minimize();
  }
}

void PanelManager::RestoreAll() {
  for (ActivePanels::const_iterator iter = active_panels_.begin();
       iter != active_panels_.end(); ++iter) {
    (*iter)->Restore();
  }
}

void PanelManager::RemoveAllActive() {
  // This should not be called when we're in the process of dragging.
  DCHECK(dragging_panel_index_ == kInvalidPanelIndex);

  // Start from the bottom to avoid reshuffling.
  for (int i = static_cast<int>(active_panels_.size()) -1; i >= 0; --i)
    active_panels_[i]->Close();

  ProcessPending();
}
