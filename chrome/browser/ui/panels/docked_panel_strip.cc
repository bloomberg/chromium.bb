// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/docked_panel_strip.h"

#include <algorithm>
#include <vector>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/panels/overflow_panel_strip.h"
#include "chrome/browser/ui/panels/panel_drag_controller.h"
#include "chrome/browser/ui/panels/panel_manager.h"
#include "chrome/browser/ui/panels/panel_mouse_watcher.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"

namespace {
// Width to height ratio is used to compute the default width or height
// when only one value is provided.
const double kPanelDefaultWidthToHeightRatio = 1.62;  // golden ratio

// Maxmium width of a panel is based on a factor of the entire panel strip.
#if defined(OS_CHROMEOS)
// ChromeOS device screens are relatively small and limiting the width
// interferes with some apps (e.g. http://crbug.com/111121).
const double kPanelMaxWidthFactor = 0.80;
#else
const double kPanelMaxWidthFactor = 0.35;
#endif

// New panels that cannot fit in the panel strip are moved to overflow
// after a brief delay.
const int kMoveNewPanelToOverflowDelayMs = 1500;  // arbitrary

// Occasionally some system, like Windows, might not bring up or down the bottom
// bar when the mouse enters or leaves the bottom screen area. This is the
// maximum time we will wait for the bottom bar visibility change notification.
// After the time expires, we bring up/down the titlebars as planned.
const int kMaxDelayWaitForBottomBarVisibilityChangeMs = 1000;

// See usage below.
#if defined(TOOLKIT_GTK)
const int kDelayBeforeCollapsingFromTitleOnlyStateMs = 2000;
#else
const int kDelayBeforeCollapsingFromTitleOnlyStateMs = 0;
#endif
}  // namespace

// static
const int DockedPanelStrip::kPanelMinWidth = 100;
const int DockedPanelStrip::kPanelMinHeight = 20;

DockedPanelStrip::DockedPanelStrip(PanelManager* panel_manager)
    : PanelStrip(PanelStrip::DOCKED),
      panel_manager_(panel_manager),
      minimized_panel_count_(0),
      are_titlebars_up_(false),
      minimizing_all_(false),
      delayed_titlebar_action_(NO_ACTION),
      titlebar_action_factory_(this) {
  dragging_panel_current_iterator_ = panels_.end();
  panel_manager_->display_settings_provider()->set_desktop_bar_observer(this);
}

DockedPanelStrip::~DockedPanelStrip() {
  DCHECK(panels_.empty());
  DCHECK(panels_in_temporary_layout_.empty());
  DCHECK_EQ(0, minimized_panel_count_);
}

void DockedPanelStrip::SetDisplayArea(const gfx::Rect& display_area) {
  if (display_area_ == display_area)
    return;

  gfx::Rect old_area = display_area_;
  display_area_ = display_area;

  if (panels_.empty())
    return;

  RefreshLayout();
}

void DockedPanelStrip::AddPanel(Panel* panel,
                                PositioningMask positioning_mask) {
  DCHECK_NE(this, panel->panel_strip());
  panel->SetPanelStrip(this);

  bool known_position = (positioning_mask & KNOWN_POSITION) != 0;
  bool update_bounds = (positioning_mask & DO_NOT_UPDATE_BOUNDS) == 0;

  if (!panel->initialized()) {
    DCHECK(!known_position && update_bounds);
    InsertNewlyCreatedPanel(panel);
  } else if (known_position) {
    DCHECK(update_bounds);
    InsertExistingPanelAtKnownPosition(panel);
  } else {
    InsertExistingPanelAtDefaultPosition(panel, update_bounds);
  }
}

void DockedPanelStrip::InsertNewlyCreatedPanel(Panel* panel) {
  DCHECK(!panel->initialized());

  int max_panel_width = GetMaxPanelWidth();
  int max_panel_height = GetMaxPanelHeight();
  gfx::Size restored_size = panel->restored_size();
  int height = restored_size.height();
  int width = restored_size.width();

  // Initialize the newly created panel. Does not bump any panels from strip.
  if (height == 0 && width == 0 && panel_manager_->auto_sizing_enabled()) {
    // Auto resizable is enabled only if no initial size is provided.
    panel->SetAutoResizable(true);
  } else {
    if (height == 0)
      height = width / kPanelDefaultWidthToHeightRatio;
    if (width == 0)
      width = height * kPanelDefaultWidthToHeightRatio;
  }

  // Constrain sizes to limits.
  if (width < kPanelMinWidth)
    width = kPanelMinWidth;
  else if (width > max_panel_width)
    width = max_panel_width;

  if (height < kPanelMinHeight)
    height = kPanelMinHeight;
  else if (height > max_panel_height)
    height = max_panel_height;

  panel->set_restored_size(gfx::Size(width, height));
  int x = GetRightMostAvailablePosition() - width;
  int y = display_area_.bottom() - height;

  // Keep panel visible in the strip even if overlap would occur.
  // Panel is moved to overflow from the strip after a delay.
  if (x < display_area_.x()) {
    x = display_area_.x();
    panel->set_has_temporary_layout(true);
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&DockedPanelStrip::DelayedMovePanelToOverflow,
                   base::Unretained(this),
                   panel),
        base::TimeDelta::FromMilliseconds(PanelManager::AdjustTimeInterval(
            kMoveNewPanelToOverflowDelayMs)));
  }
  panel->Initialize(gfx::Rect(x, y, width, height));

  if (panel->has_temporary_layout())
    panels_in_temporary_layout_.insert(panel);
  else
    panels_.push_back(panel);
}

void DockedPanelStrip::InsertExistingPanelAtKnownPosition(Panel* panel) {
  DCHECK(panel->initialized());

  FitPanelWithWidth(panel->GetBounds().width());

  int x = panel->GetBounds().x();
  Panels::iterator iter = panels_.begin();
  for (; iter != panels_.end(); ++iter)
    if (x > (*iter)->GetBounds().x())
      break;
  panels_.insert(iter, panel);
  UpdateMinimizedPanelCount();

  // This will automatically update all affected panels due to the insertion.
  if (iter != panels_.end())
    RefreshLayout();
}

void DockedPanelStrip::InsertExistingPanelAtDefaultPosition(
    Panel* panel, bool update_bounds) {
  DCHECK(panel->initialized());

  gfx::Size restored_size = panel->restored_size();
  int height = restored_size.height();
  int width = restored_size.width();

  int x = FitPanelWithWidth(width);
  if (update_bounds) {
    Panel::ExpansionState expansion_state_to_restore;
    if (panel->expansion_state() == Panel::EXPANDED) {
      expansion_state_to_restore = Panel::EXPANDED;
    } else {
      if (are_titlebars_up_ || panel->IsDrawingAttention()) {
        expansion_state_to_restore = Panel::TITLE_ONLY;
        height = panel->TitleOnlyHeight();
      } else {
        expansion_state_to_restore = Panel::MINIMIZED;
        height = Panel::kMinimizedPanelHeight;
      }
    }
    int y =
        GetBottomPositionForExpansionState(expansion_state_to_restore) - height;
    panel->SetPanelBounds(gfx::Rect(x, y, width, height));

    // Update the minimized state to reflect current titlebar mode.
    // Do this AFTER setting panel bounds to avoid an extra bounds change.
    if (panel->expansion_state() != Panel::EXPANDED)
      panel->SetExpansionState(expansion_state_to_restore);
  }

  panels_.push_back(panel);
  UpdateMinimizedPanelCount();
}

int DockedPanelStrip::GetMaxPanelWidth() const {
  return static_cast<int>(display_area_.width() * kPanelMaxWidthFactor);
}

int DockedPanelStrip::GetMaxPanelHeight() const {
  return display_area_.height();
}

int DockedPanelStrip::StartingRightPosition() const {
  return display_area_.right();
}

int DockedPanelStrip::GetRightMostAvailablePosition() const {
  return panels_.empty() ? StartingRightPosition() :
      (panels_.back()->GetBounds().x() - kPanelsHorizontalSpacing);
}

void DockedPanelStrip::RemovePanel(Panel* panel) {
  DCHECK_EQ(this, panel->panel_strip());
  panel->SetPanelStrip(NULL);

  if (panel->has_temporary_layout()) {
    panels_in_temporary_layout_.erase(panel);
    return;
  }

  // Removing an element from the list will invalidate the iterator that refers
  // to it. We need to update the iterator in that case.
  DCHECK(dragging_panel_current_iterator_ == panels_.end() ||
         *dragging_panel_current_iterator_ != panel);

  // Optimize for the common case of removing the last panel.
  DCHECK(!panels_.empty());
  if (panels_.back() == panel) {
    panels_.pop_back();

  // Update the saved panel placement if needed. This is because we might remove
  // |saved_panel_placement_.left_panel|.
  // Note: if |left_panel| moves to overflow and then comes back, it is OK to
  // restore the panel to the end of list since we do not want to deal with
  // this case specially.
  if (saved_panel_placement_.panel &&
      saved_panel_placement_.left_panel == panel)
    saved_panel_placement_.left_panel = NULL;

    // No need to refresh layout as the remaining panels are unaffected.
    // Just check if other panels can now fit in the freed up space.
    panel_manager_->MovePanelsOutOfOverflowIfCanFit();
  } else {
    Panels::iterator iter = find(panels_.begin(), panels_.end(), panel);
    DCHECK(iter != panels_.end());
    iter = panels_.erase(iter);

  // Update the saved panel placement if needed. This is because we might remove
  // |saved_panel_placement_.left_panel|.
  if (saved_panel_placement_.panel &&
      saved_panel_placement_.left_panel == panel)
    saved_panel_placement_.left_panel = *iter;

    RefreshLayout();
  }

  if (panel->expansion_state() != Panel::EXPANDED)
    UpdateMinimizedPanelCount();
}

bool DockedPanelStrip::CanShowPanelAsActive(const Panel* panel) const {
  // Panels with temporary layout cannot be shown as active.
  return !panel->has_temporary_layout();
}

void DockedPanelStrip::SavePanelPlacement(Panel* panel) {
  DCHECK(!saved_panel_placement_.panel);

  saved_panel_placement_.panel = panel;

  // To recover panel to its original placement, we only need to track the panel
  // that is placed after it.
  Panels::iterator iter = find(panels_.begin(), panels_.end(), panel);
  DCHECK(iter != panels_.end());
  ++iter;
  saved_panel_placement_.left_panel = (iter == panels_.end()) ? NULL : *iter;
}

void DockedPanelStrip::RestorePanelToSavedPlacement() {
  DCHECK(saved_panel_placement_.panel);

  Panel* panel = saved_panel_placement_.panel;

  // Find next panel after this panel.
  Panels::iterator iter = std::find(panels_.begin(), panels_.end(), panel);
  DCHECK(iter != panels_.end());
  Panels::iterator next_iter = iter;
  next_iter++;
  Panel* next_panel = (next_iter == panels_.end()) ? NULL : *iter;

  // Restoring is only needed when this panel is not in the right position.
  if (next_panel != saved_panel_placement_.left_panel) {
    // Remove this panel from its current position.
    panels_.erase(iter);

    // Insert this panel into its previous position.
    if (saved_panel_placement_.left_panel) {
      Panels::iterator iter_to_insert_before = std::find(panels_.begin(),
          panels_.end(), saved_panel_placement_.left_panel);
      DCHECK(iter_to_insert_before != panels_.end());
      panels_.insert(iter_to_insert_before, panel);
    } else {
      panels_.push_back(panel);
    }
  }

  RefreshLayout();

  DiscardSavedPanelPlacement();
}

void DockedPanelStrip::DiscardSavedPanelPlacement() {
  DCHECK(saved_panel_placement_.panel);
  saved_panel_placement_.panel = NULL;
  saved_panel_placement_.left_panel = NULL;
}

bool DockedPanelStrip::CanDragPanel(const Panel* panel) const {
  // Only the panels having temporary layout can't be dragged.
  return !panel->has_temporary_layout();
}

void DockedPanelStrip::StartDraggingPanelWithinStrip(Panel* panel) {
  dragging_panel_current_iterator_ =
      find(panels_.begin(), panels_.end(), panel);
  DCHECK(dragging_panel_current_iterator_ != panels_.end());
}

void DockedPanelStrip::DragPanelWithinStrip(Panel* panel,
                                            int delta_x,
                                            int delta_y) {
  // Moves this panel to the dragging position.
  // Note that we still allow the panel to be moved vertically until it gets
  // aligned to the bottom area.
  gfx::Rect new_bounds(panel->GetBounds());
  new_bounds.set_x(new_bounds.x() + delta_x);
  int bottom = GetBottomPositionForExpansionState(panel->expansion_state());
  if (new_bounds.bottom() != bottom) {
    new_bounds.set_y(new_bounds.y() + delta_y);
    if (new_bounds.bottom() > bottom)
      new_bounds.set_y(bottom - new_bounds.height());
  }
  panel->SetPanelBoundsInstantly(new_bounds);

  if (delta_x) {
    // Checks and processes other affected panels.
    if (delta_x > 0)
      DragRight(panel);
    else
      DragLeft(panel);

    // Layout refresh will automatically recompute the bounds of all affected
    // panels due to their position changes.
    RefreshLayout();
  }
}

void DockedPanelStrip::DragLeft(Panel* dragging_panel) {
  // This is the left corner of the dragging panel. We use it to check against
  // all the panels on its left.
  int dragging_panel_left_boundary = dragging_panel->GetBounds().x();

  // Checks the panels to the left of the dragging panel.
  Panels::iterator current_panel_iterator = dragging_panel_current_iterator_;
  ++current_panel_iterator;
  for (; current_panel_iterator != panels_.end(); ++current_panel_iterator) {
    Panel* current_panel = *current_panel_iterator;

    // Can we swap dragging panel with its left panel? The criterion is that
    // the left corner of dragging panel should pass the middle position of
    // its left panel.
    if (dragging_panel_left_boundary > current_panel->GetBounds().x() +
            current_panel->GetBounds().width() / 2)
      break;

    // Swaps the contents and makes |dragging_panel_current_iterator_| refers
    // to the new position.
    *dragging_panel_current_iterator_ = current_panel;
    *current_panel_iterator = dragging_panel;
    dragging_panel_current_iterator_ = current_panel_iterator;
  }
}

void DockedPanelStrip::DragRight(Panel* dragging_panel) {
  // This is the right corner of the dragging panel. We use it to check against
  // all the panels on its right.
  int dragging_panel_right_boundary = dragging_panel->GetBounds().x() +
      dragging_panel->GetBounds().width() - 1;

  // Checks the panels to the right of the dragging panel.
  Panels::iterator current_panel_iterator = dragging_panel_current_iterator_;
  while (current_panel_iterator != panels_.begin()) {
    current_panel_iterator--;
    Panel* current_panel = *current_panel_iterator;

    // Can we swap dragging panel with its right panel? The criterion is that
    // the left corner of dragging panel should pass the middle position of
    // its right panel.
    if (dragging_panel_right_boundary < current_panel->GetBounds().x() +
            current_panel->GetBounds().width() / 2)
      break;

    // Swaps the contents and makes |dragging_panel_current_iterator_| refers
    // to the new position.
    *dragging_panel_current_iterator_ = current_panel;
    *current_panel_iterator = dragging_panel;
    dragging_panel_current_iterator_ = current_panel_iterator;
  }
}

void DockedPanelStrip::EndDraggingPanelWithinStrip(Panel* panel, bool aborted) {
  dragging_panel_current_iterator_ = panels_.end();

  // Calls RefreshLayout to update the dragging panel to its final position
  // when the drag ends normally. Otherwise, the drag within this strip is
  // aborted because either the drag enters other strip or the drag is
  // cancelled. Either way, we don't need to do anything here and let the drag
  // controller handle the inter-strip transition or the drag cancellation.
  if (!aborted)
    RefreshLayout();
}

bool DockedPanelStrip::CanResizePanel(const Panel* panel) const {
  return false;
}

void DockedPanelStrip::SetPanelBounds(Panel* panel,
                                      const gfx::Rect& new_bounds) {
  DCHECK_EQ(this, panel->panel_strip());
  NOTREACHED();
}


void DockedPanelStrip::OnPanelExpansionStateChanged(Panel* panel) {
  gfx::Size size = panel->restored_size();
  Panel::ExpansionState expansion_state = panel->expansion_state();
  switch (expansion_state) {
    case Panel::EXPANDED:

      break;
    case Panel::TITLE_ONLY:
      size.set_height(panel->TitleOnlyHeight());

      break;
    case Panel::MINIMIZED:
      size.set_height(Panel::kMinimizedPanelHeight);

      break;
    default:
      NOTREACHED();
      break;
  }
  UpdateMinimizedPanelCount();

  int bottom = GetBottomPositionForExpansionState(expansion_state);
  gfx::Rect bounds = panel->GetBounds();
  panel->SetPanelBounds(
      gfx::Rect(bounds.right() - size.width(),
                bottom - size.height(),
                size.width(),
                size.height()));

  // Ensure minimized panel does not get the focus. If minimizing all,
  // the active panel will be deactivated once when all panels are minimized
  // rather than per minimized panel.
  if (expansion_state != Panel::EXPANDED && !minimizing_all_ &&
      panel->IsActive())
    panel->Deactivate();
}

void DockedPanelStrip::OnPanelAttentionStateChanged(Panel* panel) {
  DCHECK_EQ(this, panel->panel_strip());
  if (panel->IsDrawingAttention()) {
    // Bring up the titlebar to get user's attention.
    if (panel->expansion_state() == Panel::MINIMIZED)
      panel->SetExpansionState(Panel::TITLE_ONLY);
  } else {
    // Maybe bring down the titlebar now that panel is not drawing attention.
    if (panel->expansion_state() == Panel::TITLE_ONLY && !are_titlebars_up_)
      panel->SetExpansionState(Panel::MINIMIZED);
  }
}

void DockedPanelStrip::OnPanelTitlebarClicked(Panel* panel,
                                              panel::ClickModifier modifier) {
  DCHECK_EQ(this, panel->panel_strip());
  if (modifier == panel::APPLY_TO_ALL)
    ToggleMinimizeAll(panel);

  // TODO(jennb): Move all other titlebar click handling here.
  // (http://crbug.com/118431)
}

void DockedPanelStrip::ActivatePanel(Panel* panel) {
  DCHECK_EQ(this, panel->panel_strip());

  // Make sure the panel is expanded when activated so the user input
  // does not go into a collapsed window.
  panel->SetExpansionState(Panel::EXPANDED);
}

void DockedPanelStrip::MinimizePanel(Panel* panel) {
  DCHECK_EQ(this, panel->panel_strip());

  if (panel->expansion_state() != Panel::EXPANDED)
    return;

  panel->SetExpansionState(panel->IsDrawingAttention() ?
      Panel::TITLE_ONLY : Panel::MINIMIZED);
}

void DockedPanelStrip::RestorePanel(Panel* panel) {
  DCHECK_EQ(this, panel->panel_strip());
  panel->SetExpansionState(Panel::EXPANDED);
}

bool DockedPanelStrip::IsPanelMinimized(const Panel* panel) const {
  return panel->expansion_state() != Panel::EXPANDED;
}

void DockedPanelStrip::UpdateMinimizedPanelCount() {
  int prev_minimized_panel_count = minimized_panel_count_;
  minimized_panel_count_ = 0;
  for (Panels::const_iterator panel_iter = panels_.begin();
        panel_iter != panels_.end(); ++panel_iter) {
    if ((*panel_iter)->expansion_state() != Panel::EXPANDED)
      minimized_panel_count_++;
  }

  if (prev_minimized_panel_count == 0 && minimized_panel_count_ > 0)
    panel_manager_->mouse_watcher()->AddObserver(this);
  else if (prev_minimized_panel_count > 0 &&  minimized_panel_count_ == 0)
    panel_manager_->mouse_watcher()->RemoveObserver(this);

  DCHECK_LE(minimized_panel_count_, num_panels());
}

void DockedPanelStrip::ToggleMinimizeAll(Panel* panel) {
  DCHECK_EQ(this, panel->panel_strip());
  AutoReset<bool> pin(&minimizing_all_, IsPanelMinimized(panel) ? false : true);
  Panel* minimized_active_panel = NULL;
  for (Panels::const_iterator iter = panels_.begin();
       iter != panels_.end(); ++iter) {
    if (minimizing_all_) {
      if ((*iter)->IsActive())
        minimized_active_panel = *iter;
      MinimizePanel(*iter);
    } else {
      RestorePanel(*iter);
    }
  }

  // When a single panel is minimized, it is deactivated to ensure that
  // a minimized panel does not have focus. However, when minimizing all,
  // the deactivation is only done once after all panels are minimized,
  // rather than per minimized panel, both for efficiency and to avoid
  // temporary activations of random not-yet-minimized panels.
  if (minimized_active_panel)
    minimized_active_panel->Deactivate();
}

void DockedPanelStrip::ResizePanelWindow(
    Panel* panel,
    const gfx::Size& preferred_window_size) {
  // Make sure the new size does not violate panel's size restrictions.
  gfx::Size new_size(preferred_window_size.width(),
                     preferred_window_size.height());
  panel->ClampSize(&new_size);

  if (new_size != panel->restored_size())
    panel->set_restored_size(new_size);

  gfx::Rect bounds = panel->GetBounds();
  int delta_x = bounds.width() - new_size.width();

  // Only need to adjust current bounds if panel is in the dock.
  if (panel->panel_strip() == this) {
    // Only need to adjust bounds height when panel is expanded.
    Panel::ExpansionState expansion_state = panel->expansion_state();
    if (new_size.height() != bounds.height() &&
        expansion_state == Panel::EXPANDED) {
      bounds.set_y(bounds.bottom() - new_size.height());
      bounds.set_height(new_size.height());
    }

    if (delta_x != 0) {
      bounds.set_width(new_size.width());
      bounds.set_x(bounds.x() + delta_x);
    }

    if (bounds != panel->GetBounds())
      panel->SetPanelBounds(bounds);
  }

  // Only need to rearrange if panel's width changed. Rearrange even if panel
  // is in overflow because there may now be room to fit that panel.
  if (delta_x != 0)
    RefreshLayout();
}

bool DockedPanelStrip::ShouldBringUpTitlebars(int mouse_x, int mouse_y) const {
  // We should always bring up the titlebar if the mouse is over the
  // visible auto-hiding bottom bar.
  DisplaySettingsProvider* provider =
      panel_manager_->display_settings_provider();
  if (provider->IsAutoHidingDesktopBarEnabled(
          DisplaySettingsProvider::DESKTOP_BAR_ALIGNED_BOTTOM) &&
      provider->GetDesktopBarVisibility(
          DisplaySettingsProvider::DESKTOP_BAR_ALIGNED_BOTTOM) ==
              DisplaySettingsProvider::DESKTOP_BAR_VISIBLE &&
      mouse_y >= display_area_.bottom())
    return true;

  // Bring up titlebars if any panel needs the titlebar up.
  Panel* dragging_panel = dragging_panel_current_iterator_ == panels_.end() ?
      NULL : *dragging_panel_current_iterator_;
  for (Panels::const_iterator iter = panels_.begin();
       iter != panels_.end(); ++iter) {
    Panel* panel = *iter;
    Panel::ExpansionState state = panel->expansion_state();
    // Skip the expanded panel.
    if (state == Panel::EXPANDED)
      continue;

    // If the panel is showing titlebar only, we want to keep it up when it is
    // being dragged.
    if (state == Panel::TITLE_ONLY && panel == dragging_panel)
      return true;

    // We do not want to bring up other minimized panels if the mouse is over
    // the panel that pops up the titlebar to attract attention.
    if (panel->IsDrawingAttention())
      continue;

    gfx::Rect bounds = panel->GetBounds();
    if (bounds.x() <= mouse_x && mouse_x <= bounds.right() &&
        mouse_y >= bounds.y())
      return true;
  }
  return false;
}

void DockedPanelStrip::BringUpOrDownTitlebars(bool bring_up) {
  if (are_titlebars_up_ == bring_up)
    return;
  are_titlebars_up_ = bring_up;

  int task_delay_ms = 0;

  // If the auto-hiding bottom bar exists, delay the action until the bottom
  // bar is fully visible or hidden. We do not want both bottom bar and panel
  // titlebar to move at the same time but with different speeds.
  DisplaySettingsProvider* provider =
      panel_manager_->display_settings_provider();
  if (provider->IsAutoHidingDesktopBarEnabled(
          DisplaySettingsProvider::DESKTOP_BAR_ALIGNED_BOTTOM)) {
    DisplaySettingsProvider::DesktopBarVisibility visibility =
        provider->GetDesktopBarVisibility(
            DisplaySettingsProvider::DESKTOP_BAR_ALIGNED_BOTTOM);
    if (visibility !=
        (bring_up ? DisplaySettingsProvider::DESKTOP_BAR_VISIBLE
                  : DisplaySettingsProvider::DESKTOP_BAR_HIDDEN)) {
      // Occasionally some system, like Windows, might not bring up or down the
      // bottom bar when the mouse enters or leaves the bottom screen area.
      // Thus, we schedule a delayed task to do the work if we do not receive
      // the bottom bar visibility change notification within a certain period
      // of time.
      task_delay_ms = kMaxDelayWaitForBottomBarVisibilityChangeMs;
    }
  }

  // On some OSes, the interaction with native Taskbars/Docks may be improved
  // if the panels do not go back to minimized state too fast. For example,
  // with a taskbar in auto-hide mode, the taskbar will cover the panel in
  // title-only mode which appears on hover. Leaving it up for a little longer
  // would allow the user to be able to click on it.
  //
  // Currently, no platforms use both delays.
  DCHECK(task_delay_ms == 0 ||
         kDelayBeforeCollapsingFromTitleOnlyStateMs == 0);
  if (!bring_up && task_delay_ms == 0) {
    task_delay_ms = kDelayBeforeCollapsingFromTitleOnlyStateMs;
  }

  // OnAutoHidingDesktopBarVisibilityChanged will handle this.
  delayed_titlebar_action_ = bring_up ? BRING_UP : BRING_DOWN;

  // If user moves the mouse in and out of mouse tracking area, we might have
  // previously posted but not yet dispatched task in the queue. New action
  // should always 'reset' the delays so cancel any tasks that haven't run yet
  // and post a new one.
  titlebar_action_factory_.InvalidateWeakPtrs();
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&DockedPanelStrip::DelayedBringUpOrDownTitlebarsCheck,
                 titlebar_action_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(
          PanelManager::AdjustTimeInterval(task_delay_ms)));
}

void DockedPanelStrip::DelayedBringUpOrDownTitlebarsCheck() {
  // Task was already processed or cancelled - bail out.
  if (delayed_titlebar_action_ == NO_ACTION)
    return;

  bool need_to_bring_up_titlebars = (delayed_titlebar_action_ == BRING_UP);

  delayed_titlebar_action_ = NO_ACTION;

  // Check if the action is still needed based on the latest mouse position. The
  // user could move the mouse into the tracking area and then quickly move it
  // out of the area. In case of this, cancel the action.
  if (are_titlebars_up_ != need_to_bring_up_titlebars)
    return;

  DoBringUpOrDownTitlebars(need_to_bring_up_titlebars);
}

void DockedPanelStrip::DoBringUpOrDownTitlebars(bool bring_up) {
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

int DockedPanelStrip::GetBottomPositionForExpansionState(
    Panel::ExpansionState expansion_state) const {
  int bottom = display_area_.bottom();
  // If there is an auto-hiding desktop bar aligned to the bottom edge, we need
  // to move the title-only panel above the auto-hiding desktop bar.
  DisplaySettingsProvider* provider =
      panel_manager_->display_settings_provider();
  if (expansion_state == Panel::TITLE_ONLY &&
      provider->IsAutoHidingDesktopBarEnabled(
          DisplaySettingsProvider::DESKTOP_BAR_ALIGNED_BOTTOM)) {
    bottom -= provider->GetDesktopBarThickness(
        DisplaySettingsProvider::DESKTOP_BAR_ALIGNED_BOTTOM);
  }

  return bottom;
}

void DockedPanelStrip::OnMouseMove(const gfx::Point& mouse_position) {
  bool bring_up_titlebars = ShouldBringUpTitlebars(mouse_position.x(),
                                                   mouse_position.y());
  BringUpOrDownTitlebars(bring_up_titlebars);
}

void DockedPanelStrip::OnAutoHidingDesktopBarVisibilityChanged(
    DisplaySettingsProvider::DesktopBarAlignment alignment,
    DisplaySettingsProvider::DesktopBarVisibility visibility) {
  if (delayed_titlebar_action_ == NO_ACTION)
    return;

  DisplaySettingsProvider::DesktopBarVisibility expected_visibility =
      delayed_titlebar_action_ == BRING_UP
          ? DisplaySettingsProvider::DESKTOP_BAR_VISIBLE
          : DisplaySettingsProvider::DESKTOP_BAR_HIDDEN;
  if (visibility != expected_visibility)
    return;

  DoBringUpOrDownTitlebars(delayed_titlebar_action_ == BRING_UP);
  delayed_titlebar_action_ = NO_ACTION;
}

void DockedPanelStrip::OnFullScreenModeChanged(bool is_full_screen) {
  for (Panels::const_iterator iter = panels_.begin();
       iter != panels_.end(); ++iter) {
    (*iter)->FullScreenModeChanged(is_full_screen);
  }
}

bool DockedPanelStrip::CanFitPanel(const Panel* panel) const {
  int width = panel->GetRestoredBounds().width();
  return GetRightMostAvailablePosition() - width >= display_area_.x();
}

int DockedPanelStrip::FitPanelWithWidth(int width) {
  int x = GetRightMostAvailablePosition() - width;
  if (x < display_area_.x()) {
    // Insufficient space for the requested width. Bump panels to overflow.
    Panel* last_panel_to_send_to_overflow;
    for (Panels::reverse_iterator iter = panels_.rbegin();
         iter != panels_.rend(); ++iter) {
      last_panel_to_send_to_overflow = *iter;
      x = last_panel_to_send_to_overflow->GetRestoredBounds().right() - width;
      if (x >= display_area_.x()) {
        panel_manager_->MovePanelsToOverflow(last_panel_to_send_to_overflow);
        break;
      }
    }
  }
  return x;
}

void DockedPanelStrip::RefreshLayout() {
  int rightmost_position = StartingRightPosition();

  Panels::const_iterator panel_iter = panels_.begin();
  for (; panel_iter != panels_.end(); ++panel_iter) {
    Panel* panel = *panel_iter;
    gfx::Rect new_bounds(panel->GetBounds());
    int x = rightmost_position - new_bounds.width();

    if (x < display_area_.x())
      break;

    // Don't update the docked panel that is in preview mode.
    if (!panel->in_preview_mode()) {
      new_bounds.set_x(x);
      new_bounds.set_y(
          GetBottomPositionForExpansionState(panel->expansion_state()) -
              new_bounds.height());
      panel->SetPanelBounds(new_bounds);
    }

    rightmost_position = x - kPanelsHorizontalSpacing;
  }

  // Add/remove panels from/to overflow. A change in work area or the
  // resize/removal of a panel may affect how many panels fit in the strip.
  // TODO(jianli): Need to handle the case that panel in preview mode could be
  // moved to overflow. (http://crbug.com/117574)
  if (panel_iter != panels_.end())
    panel_manager_->MovePanelsToOverflow(*panel_iter);
  else
    panel_manager_->MovePanelsOutOfOverflowIfCanFit();
}

void DockedPanelStrip::DelayedMovePanelToOverflow(Panel* panel) {
  if (panels_in_temporary_layout_.erase(panel)) {
      DCHECK(panel->has_temporary_layout());
      panel_manager_->MovePanelToStrip(panel,
                                       PanelStrip::IN_OVERFLOW,
                                       PanelStrip::DEFAULT_POSITION);
  }
}

void DockedPanelStrip::CloseAll() {
  // This should only be called at the end of tests to clean up.
  DCHECK(panels_in_temporary_layout_.empty());

  // Make a copy of the iterator as closing panels can modify the vector.
  Panels panels_copy = panels_;

  // Start from the bottom to avoid reshuffling.
  for (Panels::reverse_iterator iter = panels_copy.rbegin();
       iter != panels_copy.rend(); ++iter)
    (*iter)->Close();
}

void DockedPanelStrip::UpdatePanelOnStripChange(Panel* panel) {
  // Always update limits, even on existing panels, in case the limits changed
  // while panel was out of the strip.
  int max_panel_width = GetMaxPanelWidth();
  int max_panel_height = GetMaxPanelHeight();
  panel->SetSizeRange(gfx::Size(kPanelMinWidth, kPanelMinHeight),
                      gfx::Size(max_panel_width, max_panel_height));

  panel->set_attention_mode(Panel::USE_PANEL_ATTENTION);
  panel->SetAppIconVisibility(true);
  panel->SetAlwaysOnTop(true);
  panel->EnableResizeByMouse(false);
}
