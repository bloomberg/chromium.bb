// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/panel_manager.h"

#include <algorithm>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
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

// Occasionally some system, like Windows, might not bring up or down the bottom
// bar when the mouse enters or leaves the bottom screen area. This is the
// maximum time we will wait for the bottom bar visibility change notification.
// After the time expires, we bring up/down the titlebars as planned.
const int kMaxMillisecondsWaitForBottomBarVisibilityChange = 1000;

// Single instance of PanelManager.
scoped_refptr<PanelManager> panel_instance;
}  // namespace

// static
PanelManager* PanelManager::GetInstance() {
  if (!panel_instance.get())
    panel_instance = new PanelManager();
  return panel_instance.get();
}

PanelManager::PanelManager()
    : dragging_panel_index_(kInvalidPanelIndex),
      dragging_panel_original_x_(0),
      delayed_titlebar_action_(NO_ACTION),
      ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)) {
  auto_hiding_desktop_bar_ = AutoHidingDesktopBar::Create(this);
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

  auto_hiding_desktop_bar_->UpdateWorkArea(work_area_);
  AdjustWorkAreaForAutoHidingDesktopBars();

  Rearrange(panels_.begin(), adjusted_work_area_.right());
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
  // Adjust the width and height to fit into our constraint.
  int width = browser->override_bounds().width();
  int height = browser->override_bounds().height();

  if (width == 0 && height == 0) {
    width = kPanelDefaultWidthPixels;
    height = kPanelDefaultHeightPixels;
  }

  int max_panel_width =
      static_cast<int>(adjusted_work_area_.width() * kPanelMaxWidthFactor);
  int max_panel_height =
      static_cast<int>(adjusted_work_area_.height() * kPanelMaxHeightFactor);

  if (width < kPanelMinWidthPixels)
    width = kPanelMinWidthPixels;
  else if (width > max_panel_width)
    width = max_panel_width;

  if (height < kPanelMinHeightPixels)
    height = kPanelMinHeightPixels;
  else if (height > max_panel_height)
    height = max_panel_height;

  // Compute the origin. Ensure that it falls within the adjusted work area by
  // closing other panels if needed.
  int y = adjusted_work_area_.bottom() - height;

  const Extension* extension = NULL;
  int x;
  while ((x = GetRightMostAvaialblePosition() - width) <
         adjusted_work_area_.x() ) {
    if (!extension)
      extension = Panel::GetExtension(browser);
    FindAndClosePanelOnOverflow(extension);
  }

  // Now create the panel with the computed bounds.
  Panel* panel = new Panel(browser, gfx::Rect(x, y, width, height));
  panels_.push_back(panel);

  return panel;
}

int PanelManager::GetRightMostAvaialblePosition() const {
  return panels_.empty() ? adjusted_work_area_.right() :
      (panels_.back()->GetBounds().x() - kPanelsHorizontalSpacing);
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
  Rearrange(panels_.erase(iter), bounds.right());
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

bool PanelManager::ShouldBringUpTitlebars(int mouse_x, int mouse_y) const {
  // We should always bring up the titlebar if the mouse is over the
  // visible auto-hiding bottom bar.
  if (auto_hiding_desktop_bar_->IsEnabled(AutoHidingDesktopBar::ALIGN_BOTTOM) &&
      auto_hiding_desktop_bar_->GetVisibility(
              AutoHidingDesktopBar::ALIGN_BOTTOM) ==
          AutoHidingDesktopBar::VISIBLE &&
      mouse_y >= adjusted_work_area_.bottom())
    return true;

  for (Panels::const_iterator iter = panels_.begin();
       iter != panels_.end(); ++iter) {
    if ((*iter)->ShouldBringUpTitlebar(mouse_x, mouse_y))
      return true;
  }
  return false;
}

void PanelManager::BringUpOrDownTitlebars(bool bring_up) {
  // If the auto-hiding bottom bar exists, delay the action until the bottom
  // bar is fully visible or hidden. We do not want both bottom bar and panel
  // titlebar to move at the same time but with different speeds.
  if (auto_hiding_desktop_bar_->IsEnabled(AutoHidingDesktopBar::ALIGN_BOTTOM)) {
    AutoHidingDesktopBar::Visibility visibility = auto_hiding_desktop_bar_->
        GetVisibility(AutoHidingDesktopBar::ALIGN_BOTTOM);
    if (visibility != (bring_up ? AutoHidingDesktopBar::VISIBLE
                                : AutoHidingDesktopBar::HIDDEN)) {
      // OnAutoHidingDesktopBarVisibilityChanged will handle this.
      delayed_titlebar_action_ = bring_up ? BRING_UP : BRING_DOWN;

      // Occasionally some system, like Windows, might not bring up or down the
      // bottom bar when the mouse enters or leaves the bottom screen area.
      // Thus, we schedule a delayed task to do the work if we do not receive
      // the bottom bar visibility change notification within a certain period
      // of time.
      MessageLoop::current()->PostDelayedTask(
          FROM_HERE,
          method_factory_.NewRunnableMethod(
              &PanelManager::DelayedBringUpOrDownTitlebarsCheck),
          kMaxMillisecondsWaitForBottomBarVisibilityChange);

      return;
    }
  }

  DoBringUpOrDownTitlebars(bring_up);
}

void PanelManager::DelayedBringUpOrDownTitlebarsCheck() {
  if (delayed_titlebar_action_ == NO_ACTION)
    return;

  DoBringUpOrDownTitlebars(delayed_titlebar_action_ == BRING_UP);
  delayed_titlebar_action_ = NO_ACTION;
}

void PanelManager::DoBringUpOrDownTitlebars(bool bring_up) {
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

void PanelManager::AdjustWorkAreaForAutoHidingDesktopBars() {
  // Note that we do not care about the desktop bar aligned to the top edge
  // since panels could not reach so high due to size constraint.
  adjusted_work_area_ = work_area_;
  if (auto_hiding_desktop_bar_->IsEnabled(AutoHidingDesktopBar::ALIGN_BOTTOM)) {
    int space = auto_hiding_desktop_bar_->GetThickness(
        AutoHidingDesktopBar::ALIGN_BOTTOM);
    adjusted_work_area_.set_height(adjusted_work_area_.height() - space);
  }
  if (auto_hiding_desktop_bar_->IsEnabled(AutoHidingDesktopBar::ALIGN_LEFT)) {
    int space = auto_hiding_desktop_bar_->GetThickness(
        AutoHidingDesktopBar::ALIGN_LEFT);
    adjusted_work_area_.set_x(adjusted_work_area_.x() + space);
    adjusted_work_area_.set_width(adjusted_work_area_.width() - space);
  }
  if (auto_hiding_desktop_bar_->IsEnabled(AutoHidingDesktopBar::ALIGN_RIGHT)) {
    int space = auto_hiding_desktop_bar_->GetThickness(
        AutoHidingDesktopBar::ALIGN_RIGHT);
    adjusted_work_area_.set_width(adjusted_work_area_.width() - space);
  }
}

int PanelManager::GetBottomPositionForExpansionState(
    Panel::ExpansionState expansion_state) const {
  // If there is an auto-hiding desktop bar aligned to the bottom edge, we need
  // to move the minimize panel down to the bottom edge.
  int bottom = adjusted_work_area_.bottom();
  if (expansion_state == Panel::MINIMIZED &&
      auto_hiding_desktop_bar_->IsEnabled(AutoHidingDesktopBar::ALIGN_BOTTOM)) {
    bottom += auto_hiding_desktop_bar_->GetThickness(
        AutoHidingDesktopBar::ALIGN_BOTTOM);
  }
  return bottom;
}

void PanelManager::OnAutoHidingDesktopBarThicknessChanged() {
  AdjustWorkAreaForAutoHidingDesktopBars();
  Rearrange(panels_.begin(), adjusted_work_area_.right());
}

void PanelManager::OnAutoHidingDesktopBarVisibilityChanged(
    AutoHidingDesktopBar::Alignment alignment,
    AutoHidingDesktopBar::Visibility visibility) {
  if (delayed_titlebar_action_ == NO_ACTION)
    return;

  AutoHidingDesktopBar::Visibility expected_visibility =
      delayed_titlebar_action_ == BRING_UP ? AutoHidingDesktopBar::VISIBLE
                                           : AutoHidingDesktopBar::HIDDEN;
  if (visibility != expected_visibility)
    return;

  DoBringUpOrDownTitlebars(delayed_titlebar_action_ == BRING_UP);
  delayed_titlebar_action_ = NO_ACTION;
}

void PanelManager::Rearrange(Panels::iterator iter_to_start,
                             int rightmost_position) {
  if (iter_to_start == panels_.end())
    return;

  for (Panels::iterator iter = iter_to_start; iter != panels_.end(); ++iter) {
    Panel* panel = *iter;

    gfx::Rect new_bounds(panel->GetBounds());
    new_bounds.set_x(rightmost_position - new_bounds.width());
    new_bounds.set_y(adjusted_work_area_.bottom() - new_bounds.height());
    if (new_bounds != panel->GetBounds())
      panel->SetPanelBounds(new_bounds);

    rightmost_position = new_bounds.x() - kPanelsHorizontalSpacing;
  }
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
