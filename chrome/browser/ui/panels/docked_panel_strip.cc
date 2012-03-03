// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/docked_panel_strip.h"

#include <algorithm>
#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/panels/overflow_panel_strip.h"
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
      disable_layout_refresh_(false),
      delayed_titlebar_action_(NO_ACTION),
      titlebar_action_factory_(this) {
  dragging_panel_current_iterator_ = dragging_panel_original_iterator_ =
      panels_.end();
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

void DockedPanelStrip::AddPanel(Panel* panel) {
  DCHECK_EQ(this, panel->panel_strip());

  // Always update limits, even for exiting panels, in case the maximums changed
  // while panel was out of the strip.
  int max_panel_width = GetMaxPanelWidth();
  int max_panel_height = GetMaxPanelHeight();
  panel->SetSizeRange(gfx::Size(kPanelMinWidth, kPanelMinHeight),
                      gfx::Size(max_panel_width, max_panel_height));

  gfx::Size restored_size = panel->restored_size();
  int height = restored_size.height();
  int width = restored_size.width();

  if (panel->initialized()) {
    // Bump panels in the strip to make room for this panel.
    // Prevent layout refresh when panel is removed from this strip.
    disable_layout_refresh_ = true;
    PanelStrip* overflow_strip = panel_manager_->overflow_strip();
    int x;
    while ((x = GetRightMostAvailablePosition() - width) < display_area_.x()) {
      DCHECK(!panels_.empty());
      panels_.back()->MoveToStrip(overflow_strip);
    }
    disable_layout_refresh_ = false;

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
      IncrementMinimizedPanels();
    }

    int y =
        GetBottomPositionForExpansionState(expansion_state_to_restore) - height;
    panel->SetPanelBounds(gfx::Rect(x, y, width, height));

    // Update the minimized state to reflect current titlebar mode.
    // Do this AFTER setting panel bounds to avoid an extra bounds change.
    if (panel->expansion_state() != Panel::EXPANDED)
      panel->SetExpansionState(expansion_state_to_restore);

  } else {
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
  }

  // Set panel properties for this strip.
  panel->SetAppIconVisibility(true);

  if (panel->has_temporary_layout())
    panels_in_temporary_layout_.insert(panel);
  else
    panels_.push_back(panel);
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

bool DockedPanelStrip::RemovePanel(Panel* panel) {
  if (panel->has_temporary_layout()) {
    panels_in_temporary_layout_.erase(panel);
    return true;
  }

  Panels::iterator iter = find(panels_.begin(), panels_.end(), panel);
  if (iter == panels_.end())
    return false;

  if (panel->expansion_state() != Panel::EXPANDED)
    DecrementMinimizedPanels();

  // Removing an element from the list will invalidate the iterator that refers
  // to it. So we need to check if the dragging panel iterators are affected.
  bool update_original_dragging_iterator_after_erase = false;
  if (dragging_panel_original_iterator_ != panels_.end()) {
    if (dragging_panel_current_iterator_ == iter) {
      // If the dragging panel is being removed, set both dragging iterators to
      // the end of the list.
      dragging_panel_current_iterator_ = dragging_panel_original_iterator_ =
          panels_.end();
    } else if (dragging_panel_original_iterator_ == iter) {
      // If |dragging_panel_original_iterator_| refers to the element being
      // removed, set the flag so that the iterator can be updated to next
      // element.
      update_original_dragging_iterator_after_erase = true;
    }
  }

  iter = panels_.erase(iter);

  if (update_original_dragging_iterator_after_erase)
    dragging_panel_original_iterator_ = iter;

  if (!disable_layout_refresh_)
    RefreshLayout();

  return true;
}

bool DockedPanelStrip::IsPanelMinimized(Panel* panel) const {
  DCHECK_EQ(this, panel->panel_strip());
  return panel->expansion_state() != Panel::EXPANDED;
}

bool DockedPanelStrip::CanShowPanelAsActive(const Panel* panel) const {
  // Panels with temporary layout cannot be shown as active.
  return !panel->has_temporary_layout();
}

bool DockedPanelStrip::CanDragPanel(const Panel* panel) const {
  // Only the panels having temporary layout can't be dragged.
  return !panel->has_temporary_layout();
}

Panel* DockedPanelStrip::dragging_panel() const {
  return dragging_panel_current_iterator_ == panels_.end() ? NULL :
      *dragging_panel_current_iterator_;
}

void DockedPanelStrip::StartDraggingPanel(Panel* panel) {
  dragging_panel_current_iterator_ =
      find(panels_.begin(), panels_.end(), panel);
  DCHECK(dragging_panel_current_iterator_ != panels_.end());
  dragging_panel_original_iterator_ = dragging_panel_current_iterator_;
}

void DockedPanelStrip::DragPanel(Panel* panel, int delta_x, int delta_y) {
  if (!delta_x)
    return;

  // Moves this panel to the dragging position.
  gfx::Rect new_bounds(panel->GetBounds());
  new_bounds.set_x(new_bounds.x() + delta_x);
  panel->SetPanelBounds(new_bounds);

  // Checks and processes other affected panels.
  if (delta_x > 0)
    DragRight(panel);
  else
    DragLeft(panel);

  // Layout refresh will automatically recompute the bounds of all affected
  // panels due to their position changes.
  RefreshLayout();
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

void DockedPanelStrip::EndDraggingPanel(Panel* panel, bool cancelled) {
  if (cancelled) {
    if (dragging_panel_current_iterator_ != dragging_panel_original_iterator_) {
      // Find out if the dragging panel should be moved toward the end/beginning
      // of the list.
      bool move_towards_end_of_list = true;
      for (Panels::iterator iter = dragging_panel_original_iterator_;
           iter != panels_.end(); ++iter) {
        if (iter == dragging_panel_current_iterator_) {
          move_towards_end_of_list = false;
          break;
        }
      }

      // Move the dragging panel back to its original position by swapping it
      // with its adjacent element until it reach its original position.
      while (dragging_panel_current_iterator_ !=
             dragging_panel_original_iterator_) {
        Panels::iterator next_iter = dragging_panel_current_iterator_;
        if (move_towards_end_of_list)
          ++next_iter;
        else
          --next_iter;
        iter_swap(dragging_panel_current_iterator_, next_iter);
        dragging_panel_current_iterator_ = next_iter;
      }
    }
  }

  dragging_panel_current_iterator_ = dragging_panel_original_iterator_ =
      panels_.end();

  RefreshLayout();
}

void DockedPanelStrip::OnPanelExpansionStateChanged(Panel* panel) {
  gfx::Size size = panel->restored_size();
  Panel::ExpansionState expansion_state = panel->expansion_state();
  Panel::ExpansionState old_state = panel->old_expansion_state();
  switch (expansion_state) {
    case Panel::EXPANDED:
      if (old_state == Panel::TITLE_ONLY || old_state == Panel::MINIMIZED)
        DecrementMinimizedPanels();
      break;
    case Panel::TITLE_ONLY:
      size.set_height(panel->TitleOnlyHeight());
      if (old_state == Panel::EXPANDED)
        IncrementMinimizedPanels();
      break;
    case Panel::MINIMIZED:
      size.set_height(Panel::kMinimizedPanelHeight);
      if (old_state == Panel::EXPANDED)
        IncrementMinimizedPanels();
      break;
    default:
      NOTREACHED();
      break;
  }

  int bottom = GetBottomPositionForExpansionState(expansion_state);
  gfx::Rect bounds = panel->GetBounds();
  panel->SetPanelBounds(
      gfx::Rect(bounds.right() - size.width(),
                bottom - size.height(),
                size.width(),
                size.height()));
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

void DockedPanelStrip::IncrementMinimizedPanels() {
  minimized_panel_count_++;
  if (minimized_panel_count_ == 1)
    panel_manager_->mouse_watcher()->AddObserver(this);
  DCHECK_LE(minimized_panel_count_, num_panels());
}

void DockedPanelStrip::DecrementMinimizedPanels() {
  minimized_panel_count_--;
  DCHECK_GE(minimized_panel_count_, 0);
  if (minimized_panel_count_ == 0)
    panel_manager_->mouse_watcher()->RemoveObserver(this);
}

void DockedPanelStrip::ResizePanelWindow(
    Panel* panel,
    const gfx::Size& preferred_window_size) {
  // The panel width:
  // * cannot grow or shrink to go beyond [min_width, max_width]
  int new_width = preferred_window_size.width();
  if (new_width > panel->max_size().width())
    new_width = panel->max_size().width();
  if (new_width < panel->min_size().width())
    new_width = panel->min_size().width();

  // The panel height:
  // * cannot grow or shrink to go beyond [min_height, max_height]
  int new_height = preferred_window_size.height();
  if (new_height > panel->max_size().height())
    new_height = panel->max_size().height();
  if (new_height < panel->min_size().height())
    new_height = panel->min_size().height();

  // Update restored size.
  gfx::Size new_size(new_width, new_height);
  if (new_size != panel->restored_size())
    panel->set_restored_size(new_size);

  gfx::Rect bounds = panel->GetBounds();
  int delta_x = bounds.width() - new_width;

  // Only need to adjust current bounds if panel is in the dock.
  if (panel->panel_strip() == this) {
    // Only need to adjust bounds height when panel is expanded.
    Panel::ExpansionState expansion_state = panel->expansion_state();
    if (new_height != bounds.height() &&
        expansion_state == Panel::EXPANDED) {
      bounds.set_y(bounds.bottom() - new_height);
      bounds.set_height(new_height);
    }

    if (delta_x != 0) {
      bounds.set_width(new_width);
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
  AutoHidingDesktopBar* desktop_bar = panel_manager_->auto_hiding_desktop_bar();
  if (desktop_bar->IsEnabled(AutoHidingDesktopBar::ALIGN_BOTTOM) &&
      desktop_bar->GetVisibility(AutoHidingDesktopBar::ALIGN_BOTTOM) ==
          AutoHidingDesktopBar::VISIBLE &&
      mouse_y >= display_area_.bottom())
    return true;

  // Bring up titlebars if any panel needs the titlebar up.
  for (Panels::const_iterator iter = panels_.begin();
       iter != panels_.end(); ++iter) {
    Panel* panel = *iter;
    Panel::ExpansionState state = panel->expansion_state();
    // Skip the expanded panel.
    if (state == Panel::EXPANDED)
      continue;

    // If the panel is showing titlebar only, we want to keep it up when it is
    // being dragged.
    if (state == Panel::TITLE_ONLY && panel == dragging_panel())
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
  AutoHidingDesktopBar* desktop_bar = panel_manager_->auto_hiding_desktop_bar();
  if (desktop_bar->IsEnabled(AutoHidingDesktopBar::ALIGN_BOTTOM)) {
    AutoHidingDesktopBar::Visibility visibility =
        desktop_bar->GetVisibility(AutoHidingDesktopBar::ALIGN_BOTTOM);
    if (visibility != (bring_up ? AutoHidingDesktopBar::VISIBLE
                                : AutoHidingDesktopBar::HIDDEN)) {
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
  AutoHidingDesktopBar* desktop_bar = panel_manager_->auto_hiding_desktop_bar();
  if (expansion_state == Panel::TITLE_ONLY &&
      desktop_bar->IsEnabled(AutoHidingDesktopBar::ALIGN_BOTTOM)) {
    bottom -= desktop_bar->GetThickness(AutoHidingDesktopBar::ALIGN_BOTTOM);
  }

  return bottom;
}

void DockedPanelStrip::OnMouseMove(const gfx::Point& mouse_position) {
  bool bring_up_titlebars = ShouldBringUpTitlebars(mouse_position.x(),
                                                   mouse_position.y());
  BringUpOrDownTitlebars(bring_up_titlebars);
}

void DockedPanelStrip::OnAutoHidingDesktopBarVisibilityChanged(
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

void DockedPanelStrip::OnFullScreenModeChanged(bool is_full_screen) {
  for (Panels::const_iterator iter = panels_.begin();
       iter != panels_.end(); ++iter) {
    (*iter)->FullScreenModeChanged(is_full_screen);
  }
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

    // Don't update the docked panel that is currently dragged.
    if (panel != dragging_panel()) {
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
  OverflowPanelStrip* overflow_strip = panel_manager_->overflow_strip();
  if (panel_iter != panels_.end()) {
    // Prevent layout refresh when panel is removed from this strip.
    disable_layout_refresh_ = true;

    // Keep track of panels to move to overflow in a separate storage since
    // removing it from the list will invalidate the iterator.
    std::vector<Panel*> panels_to_move_to_overflow;
    for (; panel_iter != panels_.end(); ++panel_iter)
      panels_to_move_to_overflow.push_back(*panel_iter);

    // Move panels to overflow in reverse to maintain their order.
    for (std::vector<Panel*>::reverse_iterator iter =
             panels_to_move_to_overflow.rbegin();
         iter != panels_to_move_to_overflow.rend(); ++iter) {
      (*iter)->MoveToStrip(overflow_strip);
    }

    disable_layout_refresh_ = false;
  } else {
    // Attempt to add more panels from overflow to the strip.
    Panel* overflow_panel;
    while ((overflow_panel = overflow_strip->first_panel()) &&
           GetRightMostAvailablePosition() >=
               display_area_.x() + overflow_panel->restored_size().width()) {
      overflow_panel->MoveToStrip(this);
    }
  }
}

void DockedPanelStrip::DelayedMovePanelToOverflow(Panel* panel) {
  if (panels_in_temporary_layout_.erase(panel)) {
      DCHECK(panel->has_temporary_layout());
      panel->MoveToStrip(panel_manager_->overflow_strip());
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
