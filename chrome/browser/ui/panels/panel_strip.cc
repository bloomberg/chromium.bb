// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/panel_strip.h"

#include <algorithm>

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/panels/panel_manager.h"
#include "chrome/browser/ui/panels/panel_mouse_watcher.h"
#include "chrome/browser/ui/panels/panel_overflow_strip.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"

namespace {
// Invalid panel index.
const size_t kInvalidPanelIndex = static_cast<size_t>(-1);

// Width to height ratio is used to compute the default width or height
// when only one value is provided.
const double kPanelDefaultWidthToHeightRatio = 1.62;  // golden ratio

// Maxmium width of a panel is based on a factor of the entire panel strip.
const double kPanelMaxWidthFactor = 0.35;

// New panels that cannot fit in the panel strip are moved to overflow
// after a brief delay.
const int kMoveNewPanelToOverflowDelayMilliseconds = 1500;  // arbitrary

// Occasionally some system, like Windows, might not bring up or down the bottom
// bar when the mouse enters or leaves the bottom screen area. This is the
// maximum time we will wait for the bottom bar visibility change notification.
// After the time expires, we bring up/down the titlebars as planned.
const int kMaxMillisecondsWaitForBottomBarVisibilityChange = 1000;

// See usage below.
#if defined(TOOLKIT_GTK)
const int kMillisecondsBeforeCollapsingFromTitleOnlyState = 2000;
#else
const int kMillisecondsBeforeCollapsingFromTitleOnlyState = 0;
#endif
}  // namespace

// static
const int PanelStrip::kPanelMinWidth = 100;
const int PanelStrip::kPanelMinHeight = 20;

PanelStrip::PanelStrip(PanelManager* panel_manager)
    : panel_manager_(panel_manager),
      minimized_panel_count_(0),
      are_titlebars_up_(false),
      dragging_panel_index_(kInvalidPanelIndex),
      dragging_panel_original_x_(0),
      delayed_titlebar_action_(NO_ACTION),
      titlebar_action_factory_(this) {
}

PanelStrip::~PanelStrip() {
  DCHECK(panels_.empty());
  DCHECK(panels_pending_to_remove_.empty());
  DCHECK(panels_in_temporary_layout_.empty());
  DCHECK_EQ(0, minimized_panel_count_);
}

void PanelStrip::SetDisplayArea(const gfx::Rect& new_area) {
  if (display_area_ == new_area)
    return;

  gfx::Rect old_area = display_area_;
  display_area_ = new_area;

  if (panels_.empty())
    return;

  Rearrange();
}

void PanelStrip::AddPanel(Panel* panel) {
  DCHECK_NE(Panel::IN_OVERFLOW, panel->expansion_state());

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
    int x;
    while ((x = GetRightMostAvailablePosition() - width) < display_area_.x()) {
      DCHECK(!panels_.empty());
      panels_.back()->SetExpansionState(Panel::IN_OVERFLOW);
    }
    int y = display_area_.bottom() - height;
    panel->SetPanelBounds(gfx::Rect(x, y, width, height));
  } else {
    // Initialize the newly created panel. Does not bump any panels from strip.
    if (height == 0 && width == 0) {
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
    // TODO(jianli): remove the guard when overflow support is enabled on other
    // platforms. http://crbug.com/105073
#if defined(OS_WIN)
    if (x < display_area_.x()) {
      x = display_area_.x();
      panel->set_has_temporary_layout(true);
      panel->set_draggable(false);
      MessageLoop::current()->PostDelayedTask(
          FROM_HERE,
          base::Bind(&PanelStrip::DelayedMovePanelToOverflow,
                     base::Unretained(this),
                     panel),
                     PanelManager::AdjustTimeInterval(
                         kMoveNewPanelToOverflowDelayMilliseconds));
    }
#endif
    panel->Initialize(gfx::Rect(x, y, width, height));
  }

  if (panel->has_temporary_layout())
    panels_in_temporary_layout_.insert(panel);
  else
    panels_.push_back(panel);
}

int PanelStrip::GetMaxPanelWidth() const {
  return static_cast<int>(display_area_.width() * kPanelMaxWidthFactor);
}

int PanelStrip::GetMaxPanelHeight() const {
  return display_area_.height();
}

int PanelStrip::StartingRightPosition() const {
  return display_area_.right();
}

int PanelStrip::GetRightMostAvailablePosition() const {
  return panels_.empty() ? StartingRightPosition() :
      (panels_.back()->GetBounds().x() - kPanelsHorizontalSpacing);
}

bool PanelStrip::Remove(Panel* panel) {
  if (panel->has_temporary_layout()) {
    panels_in_temporary_layout_.erase(panel);
    return true;
  }

  if (find(panels_.begin(), panels_.end(), panel) == panels_.end())
    return false;

  // If we're in the process of dragging, delay the removal.
  if (dragging_panel_index_ != kInvalidPanelIndex) {
    panels_pending_to_remove_.push_back(panel);
    return true;
  }

  DoRemove(panel);

  // Don't rearrange the strip if a panel is being moved from the panel strip
  // to the overflow strip.
  if (panel->expansion_state() != Panel::IN_OVERFLOW)
    Rearrange();

  return true;
}

void PanelStrip::DelayedRemove() {
  for (size_t i = 0; i < panels_pending_to_remove_.size(); ++i)
    DoRemove(panels_pending_to_remove_[i]);
  panels_pending_to_remove_.clear();
  Rearrange();
}

bool PanelStrip::DoRemove(Panel* panel) {
  Panels::iterator iter = find(panels_.begin(), panels_.end(), panel);
  if (iter == panels_.end())
    return false;

  if (panel->expansion_state() == Panel::TITLE_ONLY ||
      panel->expansion_state() == Panel::MINIMIZED)
    DecrementMinimizedPanels();

  panels_.erase(iter);
  panel_manager_->OnPanelRemoved(panel);
  return true;
}

void PanelStrip::StartDragging(Panel* panel) {
  for (size_t i = 0; i < panels_.size(); ++i) {
    if (panels_[i] == panel) {
      dragging_panel_index_ = i;
      dragging_panel_bounds_ = panel->GetBounds();
      dragging_panel_original_x_ = dragging_panel_bounds_.x();
      break;
    }
  }
}

void PanelStrip::Drag(int delta_x) {
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

void PanelStrip::DragLeft() {
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

void PanelStrip::DragRight() {
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

void PanelStrip::EndDragging(bool cancelled) {
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

void PanelStrip::OnPanelExpansionStateChanged(Panel* panel) {
  gfx::Size size = panel->restored_size();
  Panel::ExpansionState expansion_state = panel->expansion_state();
  Panel::ExpansionState old_state = panel->old_expansion_state();
  if (old_state == Panel::IN_OVERFLOW) {
    panel_manager_->panel_overflow_strip()->Remove(panel);
    AddPanel(panel);
    panel->SetAppIconVisibility(true);
    panel->set_draggable(true);
  }
  switch (expansion_state) {
    case Panel::EXPANDED:
      if (old_state == Panel::TITLE_ONLY || old_state == Panel::MINIMIZED)
        DecrementMinimizedPanels();
      break;
    case Panel::TITLE_ONLY:
      size.set_height(panel->TitleOnlyHeight());
      if (old_state == Panel::EXPANDED || old_state == Panel::IN_OVERFLOW)
        IncrementMinimizedPanels();
      break;
    case Panel::MINIMIZED:
      size.set_height(Panel::kMinimizedPanelHeight);
      if (old_state == Panel::EXPANDED || old_state == Panel::IN_OVERFLOW)
        IncrementMinimizedPanels();
      break;
    case Panel::IN_OVERFLOW:
      if (old_state == Panel::TITLE_ONLY || old_state == Panel::MINIMIZED)
        DecrementMinimizedPanels();
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

void PanelStrip::IncrementMinimizedPanels() {
  minimized_panel_count_++;
  if (minimized_panel_count_ == 1)
    panel_manager_->mouse_watcher()->AddObserver(this);
  DCHECK_LE(minimized_panel_count_, num_panels());
}

void PanelStrip::DecrementMinimizedPanels() {
  minimized_panel_count_--;
  DCHECK_GE(minimized_panel_count_, 0);
  if (minimized_panel_count_ == 0)
    panel_manager_->mouse_watcher()->RemoveObserver(this);
}

void PanelStrip::OnWindowSizeChanged(
    Panel* panel, const gfx::Size& preferred_window_size) {
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

  // Only need to adjust bounds height when panel is expanded.
  gfx::Rect bounds = panel->GetBounds();
  Panel::ExpansionState expansion_state = panel->expansion_state();
  if (new_height != bounds.height() &&
      expansion_state == Panel::EXPANDED) {
    bounds.set_y(bounds.bottom() - new_height);
    bounds.set_height(new_height);
  }

  // Only need to adjust width if panel is in the panel strip.
  int delta_x = bounds.width() - new_width;
  if (delta_x != 0 && expansion_state != Panel::IN_OVERFLOW) {
    bounds.set_width(new_width);
    bounds.set_x(bounds.x() + delta_x);
  }

  if (bounds != panel->GetBounds())
    panel->SetPanelBounds(bounds);

  // Only need to rearrange if panel's width changed.
  if (delta_x != 0)
    Rearrange();
}

bool PanelStrip::ShouldBringUpTitlebars(int mouse_x, int mouse_y) const {
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
    if (state == Panel::TITLE_ONLY && is_dragging_panel())
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

void PanelStrip::BringUpOrDownTitlebars(bool bring_up) {
  if (are_titlebars_up_ == bring_up)
    return;
  are_titlebars_up_ = bring_up;

  int task_delay_milliseconds = 0;

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
      task_delay_milliseconds =
          kMaxMillisecondsWaitForBottomBarVisibilityChange;
    }
  }

  // On some OSes, the interaction with native Taskbars/Docks may be improved
  // if the panels do not go back to minimized state too fast. For example,
  // with a taskbar in auto-hide mode, the taskbar will cover the panel in
  // title-only mode which appears on hover. Leaving it up for a little longer
  // would allow the user to be able to click on it.
  //
  // Currently, no platforms use both delays.
  DCHECK(task_delay_milliseconds == 0 ||
         kMillisecondsBeforeCollapsingFromTitleOnlyState == 0);
  if (!bring_up && task_delay_milliseconds == 0)
    task_delay_milliseconds = kMillisecondsBeforeCollapsingFromTitleOnlyState;

  // OnAutoHidingDesktopBarVisibilityChanged will handle this.
  delayed_titlebar_action_ = bring_up ? BRING_UP : BRING_DOWN;

  // If user moves the mouse in and out of mouse tracking area, we might have
  // previously posted but not yet dispatched task in the queue. New action
  // should always 'reset' the delays so cancel any tasks that haven't run yet
  // and post a new one.
  titlebar_action_factory_.InvalidateWeakPtrs();
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&PanelStrip::DelayedBringUpOrDownTitlebarsCheck,
                 titlebar_action_factory_.GetWeakPtr()),
                 PanelManager::AdjustTimeInterval(task_delay_milliseconds));
}

void PanelStrip::DelayedBringUpOrDownTitlebarsCheck() {
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

void PanelStrip::DoBringUpOrDownTitlebars(bool bring_up) {
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

int PanelStrip::GetBottomPositionForExpansionState(
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

void PanelStrip::OnMouseMove(const gfx::Point& mouse_position) {
  bool bring_up_titlebars = ShouldBringUpTitlebars(mouse_position.x(),
                                                   mouse_position.y());
  BringUpOrDownTitlebars(bring_up_titlebars);
}

void PanelStrip::OnAutoHidingDesktopBarVisibilityChanged(
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

void PanelStrip::OnFullScreenModeChanged(bool is_full_screen) {
  for (size_t i = 0; i < panels_.size(); ++i)
    panels_[i]->FullScreenModeChanged(is_full_screen);
}

void PanelStrip::Rearrange() {
  int rightmost_position = StartingRightPosition();

  size_t panel_index = 0;
  for (; panel_index < panels_.size(); ++panel_index) {
    Panel* panel = panels_[panel_index];
    gfx::Rect new_bounds(panel->GetBounds());
    int x = rightmost_position - new_bounds.width();

  // TODO(jianli): remove the guard when overflow support is enabled on other
  // platforms. http://crbug.com/105073
#if defined(OS_WIN)
    if (x < display_area_.x())
      break;
#endif

    new_bounds.set_x(x);
    new_bounds.set_y(
        GetBottomPositionForExpansionState(panel->expansion_state()) -
            new_bounds.height());
    panel->SetPanelBounds(new_bounds);

    rightmost_position = new_bounds.x() - kPanelsHorizontalSpacing;
  }

  // TODO(jianli): remove the guard when overflow support is enabled on other
  // platforms. http://crbug.com/105073
#if defined(OS_WIN)
  // Add/remove panels from/to overflow. A change in work area or the
  // resize/removal of a panel may affect how many panels fit in the strip.
  if (panel_index < panels_.size()) {
    // Move panels to overflow in reverse to maintain their order.
    for (size_t overflow_index = panels_.size() - 1;
         overflow_index >= panel_index; --overflow_index)
      panels_[overflow_index]->SetExpansionState(Panel::IN_OVERFLOW);
  } else {
    // Attempt to add more panels from overflow to the strip.
    PanelOverflowStrip* overflow_strip = panel_manager_->panel_overflow_strip();
    Panel* overflow_panel;
    while ((overflow_panel = overflow_strip->first_panel()) &&
           GetRightMostAvailablePosition() >=
               display_area_.x() + overflow_panel->restored_size().width()) {
      // We need to get back to the previous expansion state.
      Panel::ExpansionState expansion_state_to_restore =
          overflow_panel->old_expansion_state();
      if (expansion_state_to_restore == Panel::MINIMIZED ||
          expansion_state_to_restore == Panel::TITLE_ONLY) {
        expansion_state_to_restore = are_titlebars_up_ ? Panel::TITLE_ONLY
                                                       : Panel::MINIMIZED;
      }
      overflow_panel->SetExpansionState(expansion_state_to_restore);
    }
  }
#endif
}

void PanelStrip::DelayedMovePanelToOverflow(Panel* panel) {
  if (panels_in_temporary_layout_.erase(panel)) {
      DCHECK(panel->has_temporary_layout());
      panel->SetExpansionState(Panel::IN_OVERFLOW);
  }
}

void PanelStrip::RemoveAll() {
  // This should only be called at the end of tests to clean up.
  DCHECK(dragging_panel_index_ == kInvalidPanelIndex);
  DCHECK(panels_in_temporary_layout_.empty());

  // Make a copy of the iterator as closing panels can modify the vector.
  Panels panels_copy = panels_;

  // Start from the bottom to avoid reshuffling.
  for (Panels::reverse_iterator iter = panels_copy.rbegin();
       iter != panels_copy.rend(); ++iter)
    (*iter)->Close();
}

bool PanelStrip::is_dragging_panel() const {
  return dragging_panel_index_ != kInvalidPanelIndex;
}
