// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/panel_overflow_strip.h"

#include "base/logging.h"
#include "chrome/browser/ui/panels/panel_manager.h"
#include "chrome/browser/ui/panels/panel_mouse_watcher.h"
#include "chrome/browser/ui/panels/panel_overflow_indicator.h"
#include "chrome/browser/ui/panels/panel_strip.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_service.h"
#include "ui/base/animation/slide_animation.h"

namespace {
// The width of the overflow area that is expanded to show more info, i.e.
// titles, when the mouse hovers over the area.
static const int kOverflowAreaHoverWidth = 200;

// Maximium number of visible overflow panels when the mouse does not hover over
// the overflow area.
const size_t kMaxVisibleOverflowPanels = 6;

// The ratio to determine the maximum number of visible overflow panels when the
// mouse hovers over the overflow area. These panels cannot occupy more than the
// fixed percentage of the display area height.
const double kHeightRatioForMaxVisibleOverflowPanelsOnHover = 0.67;

// This value is experimental and subjective.
const int kOverflowHoverAnimationMs = 180;
}

PanelOverflowStrip::PanelOverflowStrip(PanelManager* panel_manager)
    : panel_manager_(panel_manager),
      current_display_width_(0),
      max_visible_panels_(kMaxVisibleOverflowPanels),
      max_visible_panels_on_hover_(0),
      are_overflow_titles_shown_(false),
      overflow_hover_animator_start_width_(0),
      overflow_hover_animator_end_width_(0) {
}

PanelOverflowStrip::~PanelOverflowStrip() {
  DCHECK(panels_.empty());
  DCHECK(!overflow_indicator_.get());
}

void PanelOverflowStrip::SetDisplayArea(const gfx::Rect& display_area) {
  if (display_area_ == display_area)
    return;
  display_area_ = display_area;

  UpdateCurrentWidth();

  max_visible_panels_on_hover_ = 0;  // reset this value for recomputation.
  UpdateMaxVisiblePanelsOnHover();

  if (overflow_indicator_.get())
    UpdateOverflowIndicatorCount();

  Refresh();
}

void PanelOverflowStrip::UpdateMaxVisiblePanelsOnHover() {
  // No need to recompute this value.
  if (max_visible_panels_on_hover_ || panels_.empty())
    return;

  max_visible_panels_on_hover_ =
      kHeightRatioForMaxVisibleOverflowPanelsOnHover *
      display_area_.height() /
      panels_.front()->IconOnlySize().height();
}

void PanelOverflowStrip::UpdateCurrentWidth() {
  current_display_width_ = are_overflow_titles_shown_ ? kOverflowAreaHoverWidth
                                                      : display_area_.width();
}

void PanelOverflowStrip::AddPanel(Panel* panel) {
  // TODO(jianli): consider using other container to improve the perf for
  // inserting to the front. http://crbug.com/106222
  DCHECK_EQ(Panel::IN_OVERFLOW, panel->expansion_state());
  // Newly created panels that were temporarily in the panel strip
  // are added to the back of the overflow, whereas panels that are
  // bumped from the panel strip by other panels go to the front
  // of overflow.
  if (panel->has_temporary_layout()) {
    panel->set_has_temporary_layout(false);
    panels_.push_back(panel);
    DoRefresh(panels_.size() - 1, panels_.size() - 1);
  } else {
    panels_.insert(panels_.begin(), panel);
    Refresh();
  }

  if (num_panels() == 1) {
    panel_manager_->mouse_watcher()->AddObserver(this);
    UpdateMaxVisiblePanelsOnHover();
  }

  // Update the overflow indicator only when the number of overflow panels go
  // beyond the maximum visible limit.
  if (num_panels() > max_visible_panels_) {
    if (!overflow_indicator_.get()) {
      overflow_indicator_.reset(PanelOverflowIndicator::Create());
    }
    UpdateOverflowIndicatorCount();
  }
}

bool PanelOverflowStrip::Remove(Panel* panel) {
  size_t index = 0;
  Panels::iterator iter = panels_.begin();
  for (; iter != panels_.end(); ++iter, ++index)
    if (*iter == panel)
      break;
  if (iter == panels_.end())
    return false;

  panels_.erase(iter);
  DoRefresh(index, panels_.size() - 1);
  panel_manager_->OnPanelRemoved(panel);

  if (panels_.empty())
    panel_manager_->mouse_watcher()->RemoveObserver(this);

  // Update the overflow indicator. If the number of overflow panels fall below
  // the maximum visible limit, we do not need the overflow indicator any more.
  if (num_panels() < max_visible_panels_)
    overflow_indicator_.reset();
  else
    UpdateOverflowIndicatorCount();

  return true;
}

void PanelOverflowStrip::RemoveAll() {
  // Make a copy of the iterator as closing panels can modify the vector.
  Panels panels_copy = panels_;

  // Start from the bottom to avoid reshuffling.
  for (Panels::reverse_iterator iter = panels_copy.rbegin();
       iter != panels_copy.rend(); ++iter)
    (*iter)->Close();
}

void PanelOverflowStrip::OnPanelExpansionStateChanged(Panel* panel) {
  // Only care about new state being overflow.
  if (panel->expansion_state() != Panel::IN_OVERFLOW)
    return;

  panel_manager_->panel_strip()->Remove(panel);
  AddPanel(panel);
  panel->SetAppIconVisibility(false);
  panel->set_draggable(false);
}

void PanelOverflowStrip::OnPanelAttentionStateChanged(Panel* panel) {
  DCHECK(panel->expansion_state() == Panel::IN_OVERFLOW);
  UpdateOverflowIndicatorAttention();
}

void PanelOverflowStrip::Refresh() {
  if (panels_.empty())
    return;
  DoRefresh(0, panels_.size() - 1);
}

void PanelOverflowStrip::DoRefresh(size_t start_index, size_t end_index) {
  if (panels_.empty() || start_index == panels_.size())
    return;

  DCHECK(end_index < panels_.size());

  for (size_t index = start_index; index <= end_index; ++index) {
    Panel* panel = panels_[index];
    gfx::Rect new_bounds = ComputeLayout(index,
                                         panel->IconOnlySize());
    panel->SetPanelBounds(new_bounds);
  }
}

void PanelOverflowStrip::UpdateOverflowIndicatorCount() {
  if (!overflow_indicator_.get())
    return;

  int max_panels = max_visible_panels();

  // Setting the count to 0 will hide the indicator.
  if (num_panels() <= max_panels) {
    overflow_indicator_->SetCount(0);
    return;
  }

  // Update the bounds and count.
  int height = overflow_indicator_->GetHeight();
  overflow_indicator_->SetBounds(gfx::Rect(
      display_area_.x(),
      panels_[max_panels - 1]->GetBounds().y() - height,
      current_display_width_,
      height));
  overflow_indicator_->SetCount(num_panels() - max_panels);

  // The attention state might get changed when there is a change to count
  // value.
  UpdateOverflowIndicatorAttention();
}

void PanelOverflowStrip::UpdateOverflowIndicatorAttention() {
  if (!overflow_indicator_.get())
    return;

  int max_panels = max_visible_panels();

  // The overflow indicator is painted as drawing attention only when there is
  // at least one hidden panel that is drawing attention.
  bool is_drawing_attention = false;
  for (int index = max_panels; index < num_panels(); ++index) {
    if (panels_[index]->IsDrawingAttention()) {
      is_drawing_attention = true;
      break;
    }
  }
  if (is_drawing_attention)
    overflow_indicator_->DrawAttention();
  else
    overflow_indicator_->StopDrawingAttention();
}

gfx::Rect PanelOverflowStrip::ComputeLayout(
    size_t index, const gfx::Size& iconified_size) const {
  DCHECK(index != kInvalidPanelIndex);

  gfx::Rect bounds;
  int bottom = (index == 0) ? display_area_.bottom() :
      panels_[index - 1]->GetBounds().y();
  bounds.set_x(display_area_.x());
  bounds.set_y(bottom - iconified_size.height());

  if (static_cast<int>(index) < max_visible_panels()) {
    bounds.set_width(current_display_width_);
    bounds.set_height(iconified_size.height());
  } else {
    // Invisible for overflow-on-overflow.
    bounds.set_width(0);
    bounds.set_height(0);
  }

  return bounds;
}

void PanelOverflowStrip::OnMouseMove(const gfx::Point& mouse_position) {
  bool show_overflow_titles = ShouldShowOverflowTitles(mouse_position);
  ShowOverflowTitles(show_overflow_titles);
}

bool PanelOverflowStrip::ShouldShowOverflowTitles(
    const gfx::Point& mouse_position) const {
  if (panels_.empty())
    return false;

  Panel* top_visible_panel = num_panels() >= max_visible_panels() ?
        panels_[max_visible_panels() - 1] : panels_.back();
  return mouse_position.x() <= display_area_.x() + current_display_width_ &&
         top_visible_panel->GetBounds().y() <= mouse_position.y() &&
         mouse_position.y() <= display_area_.bottom();
}

void PanelOverflowStrip::ShowOverflowTitles(bool show_overflow_titles) {
  if (show_overflow_titles == are_overflow_titles_shown_)
    return;
  are_overflow_titles_shown_ = show_overflow_titles;

  int start_width = current_display_width_;
  UpdateCurrentWidth();

  if (panels_.empty())
    return;

  if (show_overflow_titles) {
    overflow_hover_animator_start_width_ = start_width;
    overflow_hover_animator_end_width_ = kOverflowAreaHoverWidth;

    // We need to bring all overflow panels to the top of z-order since the
    // active panel might obscure the expanded overflow panels.
    for (Panels::iterator iter = panels_.begin();
         iter != panels_.end(); ++iter) {
      (*iter)->EnsureFullyVisible();
    }
  } else {
    overflow_hover_animator_start_width_ = start_width;
    overflow_hover_animator_end_width_ = display_area_.width();
  }

  if (!overflow_hover_animator_.get())
    overflow_hover_animator_.reset(new ui::SlideAnimation(this));
  if (overflow_hover_animator_->IsShowing())
    overflow_hover_animator_->Reset();
  overflow_hover_animator_->SetSlideDuration(
    PanelManager::AdjustTimeInterval(kOverflowHoverAnimationMs));

  overflow_hover_animator_->Show();

  // The overflow indicator count needs to be updated when the overflow area
  // gets changed.
  UpdateOverflowIndicatorCount();
}

void PanelOverflowStrip::AnimationEnded(const ui::Animation* animation) {
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_PANEL_BOUNDS_ANIMATIONS_FINISHED,
      content::Source<PanelOverflowStrip>(this),
      content::NotificationService::NoDetails());
}

void PanelOverflowStrip::AnimationProgressed(const ui::Animation* animation) {
  int current_display_width = overflow_hover_animator_->CurrentValueBetween(
      overflow_hover_animator_start_width_, overflow_hover_animator_end_width_);
  bool end_of_shrinking = current_display_width == display_area_.width();
  int max_visible_panels = end_of_shrinking ?
      max_visible_panels_ : max_visible_panels_on_hover_;

  // Update each overflow panel.
  for (int i = 0; i < num_panels(); ++i) {
    Panel* overflow_panel = panels_[i];
    gfx::Rect bounds = overflow_panel->GetBounds();

    if (i >= max_visible_panels) {
      bounds.set_width(0);
      bounds.set_height(0);
    } else {
      bounds.set_width(current_display_width);
      bounds.set_height(overflow_panel->IconOnlySize().height());
    }

    overflow_panel->SetPanelBoundsInstantly(bounds);
  }

  // Update the indicator.
  if (overflow_indicator_.get()) {
    gfx::Rect bounds = overflow_indicator_->GetBounds();
    bounds.set_width(current_display_width);
    overflow_indicator_->SetBounds(bounds);
    overflow_indicator_->SetCount(num_panels() - max_visible_panels);
  }
}

void PanelOverflowStrip::OnFullScreenModeChanged(bool is_full_screen) {
  for (size_t i = 0; i < panels_.size(); ++i)
    panels_[i]->FullScreenModeChanged(is_full_screen);
}
