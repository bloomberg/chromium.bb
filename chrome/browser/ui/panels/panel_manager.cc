// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/panel_manager.h"

#include <algorithm>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/window_sizer.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"

namespace {
// Invalid panel index.
const size_t kInvalidPanelIndex = static_cast<size_t>(-1);

// Width of spacing between first panel and the right edge of the screen.
// Leaving a larger gap at the edge of the screen allows access to UI
// elements located on the bottom right of windows.
const int kRightScreenEdgeSpacingWidth = 24;

// Width to height ratio is used to compute the default width or height
// when only one value is provided.
const double kPanelDefaultWidthToHeightRatio = 1.62;  // golden ratio

// Maxmium width and height of a panel based on the factor of the working
// area.
const double kPanelMaxWidthFactor = 0.35;
const double kPanelMaxHeightFactor = 0.5;

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
PanelManager* PanelManager::GetInstance() {
  static base::LazyInstance<PanelManager> instance = LAZY_INSTANCE_INITIALIZER;
  return instance.Pointer();
}

PanelManager::PanelManager()
    : minimized_panel_count_(0),
      are_titlebars_up_(false),
      dragging_panel_index_(kInvalidPanelIndex),
      dragging_panel_original_x_(0),
      delayed_titlebar_action_(NO_ACTION),
      remove_delays_for_testing_(false),
      titlebar_action_factory_(this),
      auto_sizing_enabled_(true),
      mouse_watching_disabled_(false) {
  panel_mouse_watcher_.reset(PanelMouseWatcher::Create(this));
  auto_hiding_desktop_bar_ = AutoHidingDesktopBar::Create(this);
  OnDisplayChanged();
}

PanelManager::~PanelManager() {
  DCHECK(panels_.empty());
  DCHECK(panels_pending_to_remove_.empty());
  DCHECK_EQ(0, minimized_panel_count_);
}

void PanelManager::OnDisplayChanged() {
  scoped_ptr<WindowSizer::MonitorInfoProvider> info_provider(
      WindowSizer::CreateDefaultMonitorInfoProvider());
#if defined(OS_MACOSX)
  // On OSX, panels should be dropped all the way to the bottom edge of the
  // screen (and overlap Dock).
  gfx::Rect work_area = info_provider->GetPrimaryMonitorBounds();
#else
  gfx::Rect work_area = info_provider->GetPrimaryMonitorWorkArea();
#endif
  SetWorkArea(work_area);
}

void PanelManager::SetWorkArea(const gfx::Rect& work_area) {
  if (work_area == work_area_)
    return;
  work_area_ = work_area;

  auto_hiding_desktop_bar_->UpdateWorkArea(work_area_);
  AdjustWorkAreaForAutoHidingDesktopBars();

  Rearrange(panels_.begin(), StartingRightPosition());
}

Panel* PanelManager::CreatePanel(Browser* browser) {
  // Adjust the width and height to fit into our constraint.
  int width = browser->override_bounds().width();
  int height = browser->override_bounds().height();

  // Auto resizable is enabled only if no initial size is provided.
  bool auto_resize = (width == 0 && height == 0);

  if (!auto_resize) {
    if (height == 0)
      height = width / kPanelDefaultWidthToHeightRatio;
    if (width == 0)
      width = height * kPanelDefaultWidthToHeightRatio;
  }

  int max_panel_width = GetMaxPanelWidth();
  int max_panel_height = GetMaxPanelHeight();

  if (width < kPanelMinWidth)
    width = kPanelMinWidth;
  else if (width > max_panel_width)
    width = max_panel_width;

  if (height < kPanelMinHeight)
    height = kPanelMinHeight;
  else if (height > max_panel_height)
    height = max_panel_height;

  int y = adjusted_work_area_.bottom() - height;
  int x = GetRightMostAvailablePosition() - width;

  // Now create the panel with the computed bounds.
  Panel* panel = new Panel(browser,
                           gfx::Rect(x, y, width, height),
                           gfx::Size(kPanelMinWidth, kPanelMinHeight),
                           gfx::Size(max_panel_width, max_panel_height),
                           auto_resize);
  panels_.push_back(panel);

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_PANEL_ADDED,
      content::Source<Panel>(panel),
      content::NotificationService::NoDetails());

  return panel;
}

int PanelManager::GetMaxPanelWidth() const {
  return static_cast<int>(adjusted_work_area_.width() * kPanelMaxWidthFactor);
}

int PanelManager::GetMaxPanelHeight() const {
  return static_cast<int>(adjusted_work_area_.height() * kPanelMaxHeightFactor);
}

int PanelManager::StartingRightPosition() const {
  return adjusted_work_area_.right() - kRightScreenEdgeSpacingWidth;
}

int PanelManager::GetRightMostAvailablePosition() const {
  return panels_.empty() ? StartingRightPosition() :
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

  if (panel->expansion_state() != Panel::EXPANDED)
    DecrementMinimizedPanels();

  gfx::Rect bounds = (*iter)->GetBounds();
  Rearrange(panels_.erase(iter), bounds.right());

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_PANEL_REMOVED,
      content::Source<Panel>(panel),
      content::NotificationService::NoDetails());
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

void PanelManager::OnPanelExpansionStateChanged(
    Panel::ExpansionState old_state, Panel::ExpansionState new_state) {
  DCHECK_NE(new_state, old_state);
  switch (new_state) {
    case Panel::EXPANDED:
      DecrementMinimizedPanels();
      break;
    case Panel::MINIMIZED:
    case Panel::TITLE_ONLY:
      if (old_state == Panel::EXPANDED)
        IncrementMinimizedPanels();
      break;
    default:
      break;
  }
}

void PanelManager::IncrementMinimizedPanels() {
  if (!mouse_watching_disabled_ && !minimized_panel_count_)
    panel_mouse_watcher_->Start();
  minimized_panel_count_++;
  DCHECK_LE(minimized_panel_count_, num_panels());
}

void PanelManager::DecrementMinimizedPanels() {
  minimized_panel_count_--;
  DCHECK_GE(minimized_panel_count_, 0);
  if (!mouse_watching_disabled_ && !minimized_panel_count_)
    panel_mouse_watcher_->Stop();
}

void PanelManager::OnPreferredWindowSizeChanged(
    Panel* panel, const gfx::Size& preferred_window_size) {
  if (!auto_sizing_enabled_)
    return;

  gfx::Rect bounds = panel->GetBounds();
  int restored_height = panel->GetRestoredHeight();

  // The panel width:
  // * cannot grow or shrink to go beyond [min_width, max_width]
  int new_width = preferred_window_size.width();
  if (new_width > panel->max_size().width())
    new_width = panel->max_size().width();
  if (new_width < panel->min_size().width())
    new_width = panel->min_size().width();

  if (new_width != bounds.width()) {
    int delta = bounds.width() - new_width;
    bounds.set_x(bounds.x() + delta);
    bounds.set_width(new_width);

    // Reposition all the panels on the left.
    int panel_index = -1;
    for (int i = 0; i < static_cast<int>(panels_.size()); ++i) {
      if (panels_[i] == panel) {
        panel_index = i;
        break;
      }
    }
    DCHECK(panel_index >= 0);
    for (int i = static_cast<int>(panels_.size()) -1; i > panel_index;
         --i) {
      gfx::Rect this_bounds = panels_[i]->GetBounds();
      this_bounds.set_x(this_bounds.x() + delta);
      panels_[i]->SetPanelBounds(this_bounds);
    }
  }

  // The panel height:
  // * cannot grow or shrink to go beyond [min_height, max_height]
  int new_height = preferred_window_size.height();
  if (new_height > panel->max_size().height())
    new_height = panel->max_size().height();
  if (new_height < panel->min_size().height())
    new_height = panel->min_size().height();

  if (new_height != restored_height) {
    panel->SetRestoredHeight(new_height);
    // Only need to adjust bounds height when panel is expanded.
    if (panel->expansion_state() == Panel::EXPANDED) {
      bounds.set_y(bounds.y() - new_height + bounds.height());
      bounds.set_height(new_height);
    }
  }

  panel->SetPanelBounds(bounds);
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
  if (are_titlebars_up_ == bring_up)
    return;
  are_titlebars_up_ = bring_up;

  int task_delay_milliseconds = 0;

  // If the auto-hiding bottom bar exists, delay the action until the bottom
  // bar is fully visible or hidden. We do not want both bottom bar and panel
  // titlebar to move at the same time but with different speeds.
  if (auto_hiding_desktop_bar_->IsEnabled(AutoHidingDesktopBar::ALIGN_BOTTOM)) {
    AutoHidingDesktopBar::Visibility visibility = auto_hiding_desktop_bar_->
        GetVisibility(AutoHidingDesktopBar::ALIGN_BOTTOM);
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
  // it makes it possible to hit the titlebar on OSX if Dock has Magnifying
  // enabled - the panels stay up for a while after Dock magnification effect
  // stops covering the panels.
  //
  // Another example would be taskbar in auto-hide mode on Linux. In this mode
  // taskbar will cover the panel in title hover mode, leaving it up for a few
  // seconds would allow the user to be able to click on it.
  //
  // Currently, no platforms use both delays.
  DCHECK(task_delay_milliseconds == 0);
  if (!bring_up)
    task_delay_milliseconds = kMillisecondsBeforeCollapsingFromTitleOnlyState;

  // OnAutoHidingDesktopBarVisibilityChanged will handle this.
  delayed_titlebar_action_ = bring_up ? BRING_UP : BRING_DOWN;
  if (remove_delays_for_testing_)
    task_delay_milliseconds = 0;

  // If user moves the mouse in and out of mouse tracking area, we might have
  // previously posted but not yet dispatched task in the queue. New action
  // should always 'reset' the delays so cancel any tasks that haven't run yet
  // and post a new one.
  titlebar_action_factory_.InvalidateWeakPtrs();
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&PanelManager::DelayedBringUpOrDownTitlebarsCheck,
                 titlebar_action_factory_.GetWeakPtr()),
      task_delay_milliseconds);
}

void PanelManager::DelayedBringUpOrDownTitlebarsCheck() {
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
  int bottom = adjusted_work_area_.bottom();
  // If there is an auto-hiding desktop bar aligned to the bottom edge, we need
  // to move the title-only panel above the auto-hiding desktop bar.
  if (expansion_state == Panel::TITLE_ONLY &&
      auto_hiding_desktop_bar_->IsEnabled(AutoHidingDesktopBar::ALIGN_BOTTOM)) {
    bottom -= auto_hiding_desktop_bar_->GetThickness(
        AutoHidingDesktopBar::ALIGN_BOTTOM);
  }

  return bottom;
}

BrowserWindow* PanelManager::GetNextBrowserWindowToActivate(
    Panel* panel) const {
  // Find the last active browser window that is not minimized.
  BrowserList::const_reverse_iterator iter = BrowserList::begin_last_active();
  BrowserList::const_reverse_iterator end = BrowserList::end_last_active();
  for (; (iter != end); ++iter) {
    Browser* browser = *iter;
    if (panel->browser() != browser && !browser->window()->IsMinimized())
      return browser->window();
  }

  return NULL;
}

void PanelManager::OnMouseMove(const gfx::Point& mouse_position) {
  bool bring_up_titlebars = ShouldBringUpTitlebars(mouse_position.x(),
                                                   mouse_position.y());
  BringUpOrDownTitlebars(bring_up_titlebars);
}

void PanelManager::OnAutoHidingDesktopBarThicknessChanged() {
  AdjustWorkAreaForAutoHidingDesktopBars();
  Rearrange(panels_.begin(), StartingRightPosition());
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
  for (Panels::iterator iter = iter_to_start; iter != panels_.end(); ++iter) {
    Panel* panel = *iter;
    gfx::Rect new_bounds(panel->GetBounds());
    new_bounds.set_x(rightmost_position - new_bounds.width());
    new_bounds.set_y(
        GetBottomPositionForExpansionState(panel->expansion_state()) -
            new_bounds.height());
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
