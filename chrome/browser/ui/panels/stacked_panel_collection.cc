// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/stacked_panel_collection.h"

#include <algorithm>
#include "base/auto_reset.h"
#include "base/logging.h"
#include "chrome/browser/ui/panels/detached_panel_collection.h"
#include "chrome/browser/ui/panels/native_panel_stack.h"
#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/panels/panel_constants.h"
#include "chrome/browser/ui/panels/panel_manager.h"

StackedPanelCollection::StackedPanelCollection(PanelManager* panel_manager)
    : PanelCollection(PanelCollection::STACKED),
      panel_manager_(panel_manager),
      native_stack_(NULL),
      minimizing_all_(false) {
  native_stack_ = NativePanelStack::Create(make_scoped_ptr(this).Pass());
}

StackedPanelCollection::~StackedPanelCollection() {
  DCHECK(panels_.empty());
  DCHECK(most_recently_active_panels_.empty());
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
  top_panel->UpdateMinimizeRestoreButtonVisibility();

  ++iter;
  for (; iter != panels_.end(); ++iter) {
    Panel* panel = *iter;

    // The visibility of minimize button might need to be updated due to that
    // top panel might change when a panel is being added or removed from
    // the stack.
    panel->UpdateMinimizeRestoreButtonVisibility();

    // Don't update the stacked panel that is in preview mode.
    gfx::Rect bounds = panel->GetBounds();
    if (!panel->in_preview_mode()) {
      bounds.set_x(common_x);
      bounds.set_y(y);
      bounds.set_width(common_width);
      panel->SetPanelBounds(bounds);
      gfx::Size full_size = panel->full_size();
      full_size.set_width(common_width);
      panel->set_full_size(full_size);
    }

    y += bounds.height();
  }

  UpdateNativeStackBounds();
}

void StackedPanelCollection::UpdateNativeStackBounds() {
  // Compute and apply the enclosing bounds.
  gfx::Rect enclosing_bounds = top_panel()->GetBounds();
  enclosing_bounds.set_height(
      bottom_panel()->GetBounds().bottom() - enclosing_bounds.y());
  native_stack_->SetBounds(enclosing_bounds);
}

int StackedPanelCollection::MinimizePanelsForSpace(int needed_space) {
  int available_space = GetCurrentAvailableBottomSpace();
  for (Panels::const_reverse_iterator iter =
            most_recently_active_panels_.rbegin();
        iter != most_recently_active_panels_.rend() &&
            available_space < needed_space;
        ++iter) {
    Panel* current_panel = *iter;
    if (!current_panel->IsActive() && !IsPanelMinimized(current_panel)) {
      available_space +=
          current_panel->GetBounds().height() - panel::kTitlebarHeight;
      MinimizePanel(current_panel);
    }
  }
  return available_space;
}

void StackedPanelCollection::AddPanel(Panel* panel,
                                      PositioningMask positioning_mask) {
  DCHECK_NE(this, panel->collection());
  panel->set_collection(this);
  Panel* adjacent_panel = NULL;
  if (positioning_mask & PanelCollection::TOP_POSITION) {
    adjacent_panel = top_panel();
    panels_.push_front(panel);
  } else {
    // To fit the new panel within the working area, collapse unfocused panels
    // in the least recent active order until there is enough space.
    if (positioning_mask & PanelCollection::COLLAPSE_TO_FIT) {
      int needed_space = panel->GetBounds().height();
      int available_space = MinimizePanelsForSpace(needed_space);
      DCHECK(available_space >= needed_space);
    }

    adjacent_panel = bottom_panel();
    panels_.push_back(panel);
  }

  if (panel->IsActive())
    most_recently_active_panels_.push_front(panel);
  else
    most_recently_active_panels_.push_back(panel);

  if (adjacent_panel)
    UpdatePanelCornerStyle(adjacent_panel);

  if ((positioning_mask & NO_LAYOUT_REFRESH) == 0)
    RefreshLayout();

  native_stack_->OnPanelAddedOrRemoved(panel);
}

void StackedPanelCollection::RemovePanel(Panel* panel, RemovalReason reason) {
  bool is_top = panel == top_panel();
  bool is_bottom = panel == bottom_panel();

  // If the top panel is being closed, all panels below it should move up. To
  // do this, the top y position of top panel needs to be tracked first.
  bool top_panel_closed = false;
  int top_y = 0;
  if (reason == PanelCollection::PANEL_CLOSED && is_top) {
    top_panel_closed = true;
    top_y = panel->GetBounds().y();
  }

  panel->set_collection(NULL);
  panels_.remove(panel);
  most_recently_active_panels_.remove(panel);

  if (is_top) {
    Panel* new_top_panel = top_panel();
    if (new_top_panel)
      UpdatePanelCornerStyle(new_top_panel);
  } else if (is_bottom) {
    Panel* new_bottom_panel = bottom_panel();
    if (new_bottom_panel)
      UpdatePanelCornerStyle(new_bottom_panel);
  }

  // Now move the new top panel up to be at the same y position of the old
  // top panel. The subsequent RefreshLayout call will take care of moving all
  // other panels up.
  if (top_panel_closed) {
    Panel* new_top_panel = top_panel();
    if (new_top_panel) {
      gfx::Rect bounds = new_top_panel->GetBounds();
      bounds.set_y(top_y);
      new_top_panel->SetPanelBounds(bounds);
    }
  }

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
  bool expanded = panel->expansion_state() == Panel::EXPANDED;
  if (modifier == panel::APPLY_TO_ALL) {
    if (expanded)
      MinimizeAll();
    else
      RestoreAll(panel);
  } else {
    if (expanded)
      MinimizePanel(panel);
    else
      RestorePanel(panel);
  }
}

void StackedPanelCollection::ResizePanelWindow(
    Panel* panel,
    const gfx::Size& preferred_window_size) {
}

void StackedPanelCollection::ActivatePanel(Panel* panel) {
  // Nothing to do.
}

void StackedPanelCollection::MinimizePanel(Panel* panel) {
  panel->SetExpansionState(Panel::TITLE_ONLY);
}

void StackedPanelCollection::RestorePanel(Panel* panel) {
  // Ensure that the panel could fit within the work area after it is expanded.
  // First, try to collapse the unfocused panel in the least recent active
  // order in order to get enough space.
  int needed_space = panel->full_size().height() - panel->TitleOnlyHeight();
  int available_space = MinimizePanelsForSpace(needed_space);

  // If there is still not enough space, try to move up the stack.
  int space_beyond_available = needed_space - available_space;
  if (space_beyond_available > 0) {
    int top_available_space = GetCurrentAvailableTopSpace();
    int move_delta = (space_beyond_available > top_available_space) ?
        top_available_space : space_beyond_available;
    for (Panels::const_iterator iter = panels_.begin();
         iter != panels_.end(); iter++) {
      Panel* current_panel = *iter;
      gfx::Rect bounds = current_panel->GetBounds();
      bounds.set_y(bounds.y() - move_delta);
      current_panel->SetPanelBounds(bounds);
    }
    available_space += move_delta;
  }

  // If there is still not enough space, shrink the restored height to make it
  // fit at the last resort. Note that the restored height cannot be shrunk less
  // than the minimum panel height. If this is the case, we will just let it
  // expand beyond the screen boundary.
  space_beyond_available = needed_space - available_space;
  if (space_beyond_available > 0) {
    gfx::Size full_size = panel->full_size();
    int reduced_height = full_size.height() - space_beyond_available;
    if (reduced_height < panel::kPanelMinHeight)
      reduced_height = panel::kPanelMinHeight;
    full_size.set_height(reduced_height);
    panel->set_full_size(full_size);
  }

  panel->SetExpansionState(Panel::EXPANDED);
}

void StackedPanelCollection::MinimizeAll() {
  // Set minimizing_all_ to prevent deactivation of each panel when it
  // is minimized. See comments in OnPanelExpansionStateChanged.
  base::AutoReset<bool> pin(&minimizing_all_, true);
  Panel* minimized_active_panel = NULL;
  for (Panels::const_iterator iter = panels_.begin();
       iter != panels_.end(); ++iter) {
    if ((*iter)->IsActive())
      minimized_active_panel = *iter;
    MinimizePanel(*iter);
  }

  // When a single panel is minimized, it is deactivated to ensure that
  // a minimized panel does not have focus. However, when minimizing all,
  // the deactivation is only done once after all panels are minimized,
  // rather than per minimized panel, both for efficiency and to avoid
  // temporary activations of random not-yet-minimized panels.
  if (minimized_active_panel) {
    minimized_active_panel->Deactivate();
    // Layout will be refreshed in response to (de)activation notification.
  }
}

void StackedPanelCollection::RestoreAll(Panel* panel_clicked) {
  // Expand the panel being clicked first. This is to make sure at least one
  // panel that is clicked by the user will be expanded.
  RestorePanel(panel_clicked);

  // Try to expand all other panels starting from the most recently active
  // panel.
  for (Panels::const_iterator iter = most_recently_active_panels_.begin();
       iter != most_recently_active_panels_.end(); ++iter) {
    // If the stack already extends to both top and bottom of the work area,
    // stop now since we cannot fit any more expanded panels.
    if (GetCurrentAvailableTopSpace() == 0 &&
        GetCurrentAvailableBottomSpace() == 0) {
      break;
    }

    Panel* panel = *iter;
    if (panel != panel_clicked)
      RestorePanel(panel);
  }
}

void StackedPanelCollection::OnMinimizeButtonClicked(
    Panel* panel, panel::ClickModifier modifier) {
  // The minimize button is only present in the top panel.
  DCHECK_EQ(top_panel(), panel);

  native_stack_->Minimize();
}

void StackedPanelCollection::OnRestoreButtonClicked(
    Panel* panel, panel::ClickModifier modifier) {
  NOTREACHED();
}

bool StackedPanelCollection::CanShowMinimizeButton(const Panel* panel) const {
  // Only the top panel in the stack shows the minimize button.
  return panel == top_panel();
}

bool StackedPanelCollection::CanShowRestoreButton(const Panel* panel) const {
  return false;
}

bool StackedPanelCollection::IsPanelMinimized(const Panel* panel) const {
  return panel->expansion_state() != Panel::EXPANDED;
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
  // The panel in the stack can be resized by the following rules:
  // * If collapsed, it can only be resized by its left or right edge.
  // * Otherwise, it can be resized by its left or right edge plus:
  //   % top edge and corners, if it is at the top;
  //   % bottom edge, if it is not at the bottom.
  //   % bottom edge and corners, if it is at the bottom.
  panel::Resizability resizability = static_cast<panel::Resizability>(
      panel::RESIZABLE_LEFT | panel::RESIZABLE_RIGHT);
  if (panel->IsMinimized())
    return resizability;
  if (panel == top_panel()) {
    resizability = static_cast<panel::Resizability>(resizability |
        panel::RESIZABLE_TOP | panel::RESIZABLE_TOP_LEFT |
        panel::RESIZABLE_TOP_RIGHT);
  }
  if (panel == bottom_panel()) {
    resizability = static_cast<panel::Resizability>(resizability |
        panel::RESIZABLE_BOTTOM | panel::RESIZABLE_BOTTOM_LEFT |
        panel::RESIZABLE_BOTTOM_RIGHT);
  } else {
    resizability = static_cast<panel::Resizability>(resizability |
        panel::RESIZABLE_BOTTOM);
  }
  return resizability;
}

void StackedPanelCollection::OnPanelResizedByMouse(
    Panel* resized_panel, const gfx::Rect& new_bounds) {
  resized_panel->SetPanelBoundsInstantly(new_bounds);

  // The delta x and width can be computed from the difference between
  // the panel being resized and any other panel.
  Panel* other_panel = resized_panel == top_panel() ? bottom_panel()
                                                    : top_panel();
  gfx::Rect other_bounds = other_panel->GetBounds();
  int delta_x = new_bounds.x() - other_bounds.x();
  int delta_width = new_bounds.width() - other_bounds.width();

  gfx::Rect previous_bounds;
  bool resized_panel_found = false;
  bool panel_below_resized_panel_updated = false;
  for (Panels::const_iterator iter = panels_.begin();
       iter != panels_.end(); iter++) {
    Panel* panel = *iter;
    gfx::Rect bounds = panel->GetBounds();
    if (panel == resized_panel) {
      previous_bounds = bounds;
      resized_panel_found = true;
      continue;
    }

    bounds.set_x(bounds.x() + delta_x);
    bounds.set_width(bounds.width() + delta_width);

    // If the panel below the panel being resized is expanded, update its
    // height to offset the height change of the panel being resized.
    // For example, the stack has P1 and P2 (from top to bottom). P1's height
    // is 100 and P2's height is 120. If P1's bottom increases by 10, P2's
    // height needs to shrink by 10.
    if (resized_panel_found) {
      if (!panel_below_resized_panel_updated && !panel->IsMinimized()) {
        int old_bottom = bounds.bottom();
        bounds.set_y(previous_bounds.bottom());
        bounds.set_height(old_bottom - bounds.y());
      } else {
        bounds.set_y(previous_bounds.bottom());
      }
      panel_below_resized_panel_updated = true;
    }

    panel->SetPanelBoundsInstantly(bounds);
    previous_bounds = bounds;
  }

  UpdateNativeStackBounds();
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
  UpdatePanelCornerStyle(panel);
}

void StackedPanelCollection::OnPanelExpansionStateChanged(Panel* panel) {
  DCHECK_NE(Panel::MINIMIZED, panel->expansion_state());

  gfx::Rect bounds = panel->GetBounds();
  bounds.set_height(panel->expansion_state() == Panel::EXPANDED ?
      panel->full_size().height() : panel->TitleOnlyHeight());
  panel->SetPanelBounds(bounds);

  // Ensure minimized panel does not get the focus. If minimizing all,
  // the active panel will be deactivated once when all panels are minimized
  // rather than per minimized panel.
  if (panel->expansion_state() != Panel::EXPANDED && !minimizing_all_ &&
      panel->IsActive()) {
    panel->Deactivate();
  }

  RefreshLayout();
}

void StackedPanelCollection::OnPanelActiveStateChanged(Panel* panel) {
  if (!panel->IsActive())
    return;

  Panels::iterator iter = std::find(most_recently_active_panels_.begin(),
      most_recently_active_panels_.end(), panel);
  DCHECK(iter != most_recently_active_panels_.end());

  // If the panel is already in the front, nothing to do.
  if (iter == most_recently_active_panels_.begin())
    return;

  // Move the panel to the front.
  most_recently_active_panels_.erase(iter);
  most_recently_active_panels_.push_front(panel);
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

void StackedPanelCollection::UpdatePanelCornerStyle(Panel* panel) {
  panel::CornerStyle corner_style;
  bool at_top = panel == top_panel();
  bool at_bottom = panel == bottom_panel();
  if (at_top && at_bottom)
    corner_style = panel::ALL_ROUNDED;
  else if (at_top)
    corner_style = panel::TOP_ROUNDED;
  else if (at_bottom)
    corner_style = panel::BOTTOM_ROUNDED;
  else
    corner_style = panel::NOT_ROUNDED;
  panel->SetWindowCornerStyle(corner_style);
}

int StackedPanelCollection::GetCurrentAvailableTopSpace() const {
  if (panels_.empty())
    return panel_manager_->display_area().height();

  int available_space = top_panel()->GetBounds().y() -
      panel_manager_->display_area().y();
  if (available_space < 0)
    available_space = 0;
  return available_space;
}

int StackedPanelCollection::GetCurrentAvailableBottomSpace() const {
  if (panels_.empty())
    return panel_manager_->display_area().height();

  int available_space = panel_manager_->display_area().bottom() -
      bottom_panel()->GetBounds().bottom();
  if (available_space < 0)
    available_space = 0;
  return available_space;
}

int StackedPanelCollection::GetMaximiumAvailableBottomSpace() const {
  if (panels_.empty())
    return panel_manager_->display_area().height();

  int bottom = top_panel()->GetBounds().y();
  for (Panels::const_iterator iter = panels_.begin();
       iter != panels_.end(); iter++) {
    Panel* panel = *iter;
    if (panel->IsActive())
      bottom += panel->GetBounds().height();
    else
      bottom += panel::kTitlebarHeight;
  }
  int available_space = panel_manager_->display_area().bottom() - bottom;
  if (available_space < 0)
    available_space = 0;
  return available_space;
}
