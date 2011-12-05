// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/panel_overflow_strip.h"

#include "base/logging.h"
#include "chrome/browser/ui/panels/panel_manager.h"
#include "chrome/browser/ui/panels/panel_mouse_watcher.h"
#include "chrome/browser/ui/panels/panel_strip.h"
#include "ui/base/animation/slide_animation.h"

namespace {
// The width of the overflow area that is expanded to show more info, i.e.
// titles, when the mouse hovers over the area.
const int kOverflowAreaHoverWidth = 200;

// Maximium number of overflow panels allowed to be shown.
const size_t kMaxVisibleOverflowPanelsAllowed = 6;

// This value is experimental and subjective.
const int kOverflowHoverAnimationMs = 180;
}

PanelOverflowStrip::PanelOverflowStrip(PanelManager* panel_manager)
    : panel_manager_(panel_manager),
      are_overflow_titles_shown_(false),
      overflow_hover_animator_start_width_(0),
      overflow_hover_animator_end_width_(0) {
}

PanelOverflowStrip::~PanelOverflowStrip() {
  DCHECK(panels_.empty());
}

void PanelOverflowStrip::SetDisplayArea(const gfx::Rect& display_area) {
  if (display_area_ == display_area)
    return;

  display_area_ = display_area;
  Refresh();
}

void PanelOverflowStrip::AddPanel(Panel* panel, bool is_new) {
  // TODO(jianli): consider using other container to improve the perf for
  // inserting to the front. http://crbug.com/106222
  if (is_new)
    panels_.push_back(panel);
  else
    panels_.insert(panels_.begin(), panel);

  if (panels_.size() == 1)
    panel_manager_->mouse_watcher()->AddObserver(this);

  panel->SetExpansionState(Panel::IN_OVERFLOW);

  if (is_new) {
    // When the overflow panel is added to the back, only need to refresh
    // itself.
    DoRefresh(panels_.size() - 1, panels_.size() - 1);
  } else {
    // When the overflow panel is added to the front, refresh all.
    Refresh();
  }
}

bool PanelOverflowStrip::Remove(Panel* panel) {
  size_t index = 0;
  for (Panels::iterator iter = panels_.begin(); iter != panels_.end();
       ++iter, ++index) {
    if (*iter == panel) {
      panels_.erase(iter);
      DoRefresh(index, panels_.size() - 1);
      panel_manager_->OnPanelRemoved(panel);
      if (panels_.empty())
        panel_manager_->mouse_watcher()->RemoveObserver(this);
      return true;
    }
  }
  return false;
}

void PanelOverflowStrip::RemoveAll() {
  // Make a copy of the iterator as closing panels can modify the vector.
  Panels panels_copy = panels_;

  // Start from the bottom to avoid reshuffling.
  for (Panels::reverse_iterator iter = panels_copy.rbegin();
       iter != panels_copy.rend(); ++iter)
    (*iter)->Close();
}

void PanelOverflowStrip::OnPanelExpansionStateChanged(
    Panel* panel, Panel::ExpansionState old_state) {
  DCHECK(panel->expansion_state() == Panel::IN_OVERFLOW);
}

void PanelOverflowStrip::Refresh() {
  if (panels_.empty())
    return;
  DoRefresh(0, panels_.size() - 1);
}

void PanelOverflowStrip::DoRefresh(size_t start_index, size_t end_index) {
  if (panels_.empty())
    return;

  DCHECK(start_index < panels_.size());
  DCHECK(end_index < panels_.size());

  for (size_t index = start_index; index <= end_index; ++index) {
    Panel* panel = panels_[index];
    gfx::Rect new_bounds = ComputeLayout(index,
                                         panel->IconOnlySize());
    DCHECK(!new_bounds.IsEmpty());
    panel->SetPanelBounds(new_bounds);
  }
}

gfx::Rect PanelOverflowStrip::ComputeLayout(
    size_t index, const gfx::Size& iconified_size) const {
  DCHECK(index != kInvalidPanelIndex);

  gfx::Rect bounds;
  int bottom = (index == 0) ? display_area_.bottom() :
      panels_[index - 1]->GetBounds().y();
  bounds.set_x(display_area_.x());
  bounds.set_y(bottom - iconified_size.height());

  if (are_overflow_titles_shown_) {
    // Both icon and title are visible when the mouse hovers over the overflow
    // area.
    bounds.set_width(kOverflowAreaHoverWidth);
    bounds.set_height(iconified_size.height());
  } else if (index < kMaxVisibleOverflowPanelsAllowed) {
    // Only the icon is visible.
    bounds.set_width(iconified_size.width());
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

  int width = are_overflow_titles_shown_ ? kOverflowAreaHoverWidth
                                         : display_area_.width();
  return mouse_position.x() <= display_area_.x() + width &&
         panels_.back()->GetBounds().y() <= mouse_position.y() &&
         mouse_position.y() <= display_area_.bottom();
}

void PanelOverflowStrip::ShowOverflowTitles(bool show_overflow_titles) {
  if (show_overflow_titles == are_overflow_titles_shown_)
    return;
  are_overflow_titles_shown_ = show_overflow_titles;

  if (show_overflow_titles) {
    overflow_hover_animator_start_width_ = display_area_.width();
    overflow_hover_animator_end_width_ = kOverflowAreaHoverWidth;

    // We need to bring all overflow panels to the top of z-order since the
    // active panel might obscure the expanded overflow panels.
    for (Panels::iterator iter = panels_.begin();
         iter != panels_.end(); ++iter) {
      (*iter)->EnsureFullyVisible();
    }
  } else {
    overflow_hover_animator_start_width_ = kOverflowAreaHoverWidth;
    overflow_hover_animator_end_width_ = display_area_.width();
  }

  if (!overflow_hover_animator_.get())
    overflow_hover_animator_.reset(new ui::SlideAnimation(this));
  if (overflow_hover_animator_->IsShowing())
    overflow_hover_animator_->Reset();
  overflow_hover_animator_->SetSlideDuration(kOverflowHoverAnimationMs);

  overflow_hover_animator_->Show();
}

void PanelOverflowStrip::AnimationProgressed(const ui::Animation* animation) {
  int current_width = overflow_hover_animator_->CurrentValueBetween(
      overflow_hover_animator_start_width_, overflow_hover_animator_end_width_);
  bool end_of_shrinking = current_width == display_area_.width();

  // Update each overflow panel.
  for (size_t i = 0; i < panels_.size(); ++i) {
    Panel* overflow_panel = panels_[i];
    gfx::Rect bounds = overflow_panel->GetBounds();

    if (i >= kMaxVisibleOverflowPanelsAllowed && end_of_shrinking) {
      bounds.set_width(0);
      bounds.set_height(0);
    } else {
      bounds.set_width(current_width);
      bounds.set_height(overflow_panel->IconOnlySize().height());
    }

    overflow_panel->SetPanelBoundsInstantly(bounds);
  }
}

void PanelOverflowStrip::OnFullScreenModeChanged(bool is_full_screen) {
  for (size_t i = 0; i < panels_.size(); ++i)
    panels_[i]->FullScreenModeChanged(is_full_screen);
}
