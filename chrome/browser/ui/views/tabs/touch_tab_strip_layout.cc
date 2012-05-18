// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/touch_tab_strip_layout.h"

#include <stdio.h>

#include "base/logging.h"
#include "base/string_number_conversions.h"

TouchTabStripLayout::TouchTabStripLayout(const gfx::Size& size,
                                         int padding,
                                         int stacked_padding,
                                         int max_stacked_count,
                                         views::ViewModel* view_model)
    : size_(size),
      padding_(padding),
      stacked_padding_(stacked_padding),
      max_stacked_count_(max_stacked_count),
      view_model_(view_model),
      x_(0),
      width_(0),
      mini_tab_count_(0),
      active_index_(-1) {
}

TouchTabStripLayout::~TouchTabStripLayout() {
}

void TouchTabStripLayout::SetXAndMiniCount(int x, int mini_tab_count) {
  x_ = x;
  mini_tab_count_ = mini_tab_count;
  if (!requires_stacking() || tab_count() == mini_tab_count) {
    ResetToIdealState();
    return;
  }
  SetIdealBoundsAt(active_index(), ConstrainActiveX(ideal_x(active_index())));
  LayoutByTabOffsetAfter(active_index());
  LayoutByTabOffsetBefore(active_index());
}

void TouchTabStripLayout::SetWidth(int width) {
  if (width_ == width)
    return;

  width_ = width;
  if (!requires_stacking()) {
    ResetToIdealState();
    return;
  }
  SetActiveBoundsAndLayoutFromActiveTab();
}

void TouchTabStripLayout::SetActiveIndex(int index) {
  int old = active_index();
  active_index_ = index;
  if (old == active_index() || !requires_stacking())
    return;
  SetIdealBoundsAt(active_index(), ConstrainActiveX(ideal_x(active_index())));
  LayoutByTabOffsetBefore(active_index());
  LayoutByTabOffsetAfter(active_index());
  AdjustStackedTabs();
}

void TouchTabStripLayout::DragActiveTab(int delta) {
  if (delta == 0 || !requires_stacking())
    return;
  int initial_x = ideal_x(active_index());
  // If we're at a particular edge and start dragging, reset to ideal state.
  if ((delta > 0 && initial_x == GetMinX(active_index())) ||
      (delta < 0 && initial_x == GetMaxX(active_index()))) {
    ResetToIdealState();
  }
  int x = delta > 0 ?
      std::min(initial_x + delta, GetMaxX(active_index())) :
      std::max(initial_x + delta, GetMinX(active_index()));
  if (x != initial_x) {
    SetIdealBoundsAt(active_index(), x);
    LayoutByTabOffsetAfter(active_index());
    LayoutByTabOffsetBefore(active_index());
    delta -= (x - initial_x);
  }
  if (delta > 0)
    ExpandTabsBefore(active_index(), delta);
  else if (delta < 0)
    ExpandTabsAfter(active_index(), -delta);
  AdjustStackedTabs();
}

void TouchTabStripLayout::AddTab(int index,
                                 int add_types,
                                 int start_x) {
  if (add_types & kAddTypeActive)
    active_index_ = index;
  else if (active_index_ >= index)
    active_index_++;
  if (add_types & kAddTypeMini)
    mini_tab_count_++;
  x_ = start_x;
  if (!requires_stacking() || normal_tab_count() <= 1) {
    ResetToIdealState();
    return;
  }
  int active_x = (index + 1 == tab_count()) ?
      width_ - size_.width() : ideal_x(index + 1);
  SetIdealBoundsAt(active_index(), ConstrainActiveX(active_x));
  LayoutByTabOffsetAfter(active_index());
  LayoutByTabOffsetBefore(active_index());
  AdjustStackedTabs();

  if ((add_types & kAddTypeActive) == 0)
    MakeVisible(index);
}

void TouchTabStripLayout::RemoveTab(int index, int start_x, int old_x) {
  if (index == active_index_)
    active_index_ = std::min(active_index_, tab_count() - 1);
  else if (index < active_index_)
    active_index_--;
  bool removed_mini_tab = index < mini_tab_count_;
  if (removed_mini_tab) {
    mini_tab_count_--;
    DCHECK_GE(mini_tab_count_, 0);
  }
  int delta = start_x - x_;
  x_ = start_x;
  if (!requires_stacking()) {
    ResetToIdealState();
    return;
  }
  if (removed_mini_tab) {
    for (int i = mini_tab_count_; i < tab_count(); ++i)
      SetIdealBoundsAt(i, ideal_x(i) + delta);
  }
  SetActiveBoundsAndLayoutFromActiveTab();
  AdjustStackedTabs();
}

void TouchTabStripLayout::MoveTab(int from,
                                  int to,
                                  int new_active_index,
                                  int start_x,
                                  int mini_tab_count) {
  x_ = start_x;
  mini_tab_count_ = mini_tab_count;
  active_index_ = new_active_index;
  if (!requires_stacking() || tab_count() == mini_tab_count_) {
    ResetToIdealState();
    return;
  }
  SetIdealBoundsAt(active_index(), ConstrainActiveX(ideal_x(active_index())));
  LayoutByTabOffsetAfter(active_index());
  LayoutByTabOffsetBefore(active_index());
  AdjustStackedTabs();
}

bool TouchTabStripLayout::IsStacked(int index) const {
  if (index == active_index() || tab_count() == mini_tab_count_)
    return false;
  if (index > active_index())
    return ideal_x(index) != ideal_x(index - 1) + tab_offset();
  return ideal_x(index + 1) != ideal_x(index) + tab_offset();
}

#if !defined(NDEBUG)
std::string TouchTabStripLayout::BoundsString() const {
  std::string result;
  for (int i = 0; i < view_model_->view_size(); ++i) {
    if (!result.empty())
      result += " ";
    if (i == active_index())
      result += "[";
    result += base::IntToString(view_model_->ideal_bounds(i).x());
    if (i == active_index())
      result += "]";
  }
  return result;
}
#endif

void TouchTabStripLayout::Reset(int x,
                                int width,
                                int mini_tab_count,
                                int active_index) {
  x_ = x;
  width_ = width;
  mini_tab_count_ = mini_tab_count;
  active_index_ = active_index;
  ResetToIdealState();
}

void TouchTabStripLayout::ResetToIdealState() {
  if (tab_count() == mini_tab_count_)
    return;

  if (!requires_stacking()) {
    SetIdealBoundsAt(mini_tab_count_, x_);
    LayoutByTabOffsetAfter(mini_tab_count_);
    return;
  }

  if (normal_tab_count() == 1) {
    // TODO: might want to shrink the tab here.
    SetIdealBoundsAt(mini_tab_count_, 0);
    return;
  }

  int available_width = width_ - x_;
  int leading_count = active_index() - mini_tab_count_;
  int trailing_count = tab_count() - active_index();
  if (width_for_count(leading_count + 1) + max_stacked_width() <
      available_width) {
    SetIdealBoundsAt(mini_tab_count_, x_);
    LayoutByTabOffsetAfter(mini_tab_count_);
  } else if (width_for_count(trailing_count) + max_stacked_width() <
             available_width) {
    SetIdealBoundsAt(tab_count() - 1, width_ - size_.width());
    LayoutByTabOffsetBefore(tab_count() - 1);
  } else {
    int index = active_index();
    do {
      int stacked_padding = stacked_padding_for_count(index - mini_tab_count_);
      SetIdealBoundsAt(index, x_ + stacked_padding);
      LayoutByTabOffsetAfter(index);
      LayoutByTabOffsetBefore(index);
      index--;
    } while (index >= mini_tab_count_ && ideal_x(mini_tab_count_) != x_ &&
             ideal_x(tab_count() - 1) != width_ - size_.width());
  }
  AdjustStackedTabs();
}

void TouchTabStripLayout::MakeVisible(int index) {
  // Currently no need to support tabs openning before |index| visible.
  if (index <= active_index() || !requires_stacking() || !IsStacked(index))
    return;

  int ideal_delta = width_for_count(index - active_index()) + padding_;
  if (ideal_x(index) - ideal_x(active_index()) == ideal_delta)
    return;

  // First push active index as far to the left as it'll go.
  int active_x = std::max(GetMinX(active_index()),
                          std::min(ideal_x(index) - ideal_delta,
                                   ideal_x(active_index())));
  SetIdealBoundsAt(active_index(), active_x);
  LayoutUsingCurrentBefore(active_index());
  LayoutUsingCurrentAfter(active_index());
  AdjustStackedTabs();
  if (ideal_x(index) - ideal_x(active_index()) == ideal_delta)
    return;

  // If we get here active_index() is left aligned. Push |index| as far to
  // the right as possible.
  int x = std::min(GetMaxX(index), active_x + ideal_delta);
  SetIdealBoundsAt(index, x);
  LayoutByTabOffsetAfter(index);
  for (int next_x = x, i = index - 1; i > active_index(); --i) {
    next_x = std::max(GetMinXCompressed(i), next_x - tab_offset());
    SetIdealBoundsAt(i, next_x);
  }
  LayoutUsingCurrentAfter(active_index());
  AdjustStackedTabs();
}

int TouchTabStripLayout::ConstrainActiveX(int x) const {
  return std::min(GetMaxX(active_index()),
                  std::max(GetMinX(active_index()), x));
}

void TouchTabStripLayout::SetActiveBoundsAndLayoutFromActiveTab() {
  int x = ConstrainActiveX(ideal_x(active_index()));
  SetIdealBoundsAt(active_index(), x);
  LayoutUsingCurrentBefore(active_index());
  LayoutUsingCurrentAfter(active_index());
  AdjustStackedTabs();
}

void TouchTabStripLayout::LayoutByTabOffsetAfter(int index) {
  for (int i = index + 1; i < tab_count(); ++i) {
    int max_x = width_ - size_.width() -
        stacked_padding_for_count(tab_count() - i - 1);
    int x = std::min(max_x,
                     view_model_->ideal_bounds(i - 1).x() + tab_offset());
    SetIdealBoundsAt(i, x);
  }
}

void TouchTabStripLayout::LayoutByTabOffsetBefore(int index) {
  for (int i = index - 1; i >= mini_tab_count_; --i) {
    int min_x = x_ + stacked_padding_for_count(i - mini_tab_count_);
    int x = std::max(min_x, ideal_x(i + 1) - (tab_offset()));
    SetIdealBoundsAt(i, x);
  }
}

void TouchTabStripLayout::LayoutUsingCurrentAfter(int index) {
  for (int i = index + 1; i < tab_count(); ++i) {
    int min_x = width_ - width_for_count(tab_count() - i);
    int x = std::max(min_x,
                     std::min(ideal_x(i), ideal_x(i - 1) + tab_offset()));
    x = std::min(GetMaxX(i), x);
    SetIdealBoundsAt(i, x);
  }
}

void TouchTabStripLayout::LayoutUsingCurrentBefore(int index) {
  for (int i = index - 1; i >= mini_tab_count_; --i) {
    int max_x = x_ + width_for_count(i - mini_tab_count_);
    if (i > mini_tab_count_)
      max_x += padding_;
    max_x = std::min(max_x, ideal_x(i + 1) - stacked_padding_);
    SetIdealBoundsAt(
        i, std::min(max_x,
                    std::max(ideal_x(i), ideal_x(i + 1) - tab_offset())));
  }
}

void TouchTabStripLayout::ExpandTabsBefore(int index, int delta) {
  if (index == mini_tab_count_ + 1)
    return;  // Nothing to expand.

  for (int i = index - 1; i > mini_tab_count_ && delta > 0; --i) {
    int to_resize = std::min(delta, GetMaxXCompressed(i) - ideal_x(i));
    if (to_resize <= 0)
      continue;
    SetIdealBoundsAt(i, ideal_x(i) + to_resize);
    delta -= to_resize;
    LayoutByTabOffsetBefore(i);
  }
}

void TouchTabStripLayout::ExpandTabsAfter(int index, int delta) {
  if (index == tab_count() - 1)
    return;  // Nothing to expand.

  for (int i = index + 1; i < tab_count() - 1 && delta > 0; ++i) {
    int to_resize = std::min(ideal_x(i) - GetMinXCompressed(i), delta);
    if (to_resize <= 0)
      continue;
    SetIdealBoundsAt(i, ideal_x(i) - to_resize);
    delta -= to_resize;
    LayoutByTabOffsetAfter(i);
  }
}

void TouchTabStripLayout::AdjustStackedTabs() {
  if (!requires_stacking() || tab_count() <= mini_tab_count_ + 1)
    return;

  AdjustLeadingStackedTabs();
  AdjustTrailingStackedTabs();
}

void TouchTabStripLayout::AdjustLeadingStackedTabs() {
  int index = mini_tab_count_ + 1;
  while (index < active_index() &&
         ideal_x(index) - ideal_x(index - 1) <= stacked_padding_ &&
         ideal_x(index) <= x_ + max_stacked_width()) {
    index++;
  }
  if (ideal_x(index) - ideal_x(index - 1) <= stacked_padding_ &&
      ideal_x(index) <= x_ + max_stacked_width()) {
    index++;
  }
  if (index <= mini_tab_count_ + max_stacked_count_ - 1)
    return;
  int max_stacked = index;
  int x = x_;
  index = mini_tab_count_;
  for (; index < max_stacked - max_stacked_count_ - 1; ++index)
    SetIdealBoundsAt(index, x);
  for (; index < max_stacked; ++index, x += stacked_padding_)
    SetIdealBoundsAt(index, x);
}

void TouchTabStripLayout::AdjustTrailingStackedTabs() {
  int index = tab_count() - 1;
  int max_stacked_x = width_ - size_.width() - max_stacked_width();
  while (index > active_index() &&
         ideal_x(index) - ideal_x(index - 1) <= stacked_padding_ &&
         ideal_x(index - 1) >= max_stacked_x) {
    index--;
  }
  if (index > active_index() &&
      ideal_x(index) - ideal_x(index - 1) <= stacked_padding_ &&
      ideal_x(index - 1) >= max_stacked_x) {
    index--;
  }
  if (index >= tab_count() - max_stacked_count_)
    return;
  int first_stacked = index;
  int x = width_ - size_.width() -
      std::min(tab_count() - first_stacked, max_stacked_count_) *
      stacked_padding_;
  for (; index < first_stacked + max_stacked_count_;
       ++index, x += stacked_padding_) {
    SetIdealBoundsAt(index, x);
  }
  for (; index < tab_count(); ++index)
    SetIdealBoundsAt(index, x);
}

void TouchTabStripLayout::SetIdealBoundsAt(int index, int x) {
  view_model_->set_ideal_bounds(index, gfx::Rect(gfx::Point(x, 0), size_));
}

int TouchTabStripLayout::GetMinX(int index) const {
  int leading_count = index - mini_tab_count_;
  int trailing_count = tab_count() - index;
  return std::max(x_ + stacked_padding_for_count(leading_count),
                  width_ - width_for_count(trailing_count));
}

int TouchTabStripLayout::GetMaxX(int index) const {
  int leading_count = index - mini_tab_count_;
  int trailing_count = tab_count() - index - 1;
  int trailing_offset = stacked_padding_for_count(trailing_count);
  int leading_size = width_for_count(leading_count) + x_;
  if (leading_count > 0)
    leading_size += padding_;
  return std::min(width_ - trailing_offset - size_.width(), leading_size);
}

int TouchTabStripLayout::GetMaxXCompressed(int index) const {
  DCHECK_LT(index, active_index());
  DCHECK_GT(index, mini_tab_count_);
  int trailing = active_index() - index;
  return std::min(
      x_ + width_for_count(index - mini_tab_count_) + padding_,
      ideal_x(active_index()) - stacked_padding_for_count(trailing));
}

int TouchTabStripLayout::GetMinXCompressed(int index) const {
  DCHECK_GT(index, active_index());
  return std::max(
      width_ - width_for_count(tab_count() - index),
      ideal_x(active_index()) +
          stacked_padding_for_count(index - active_index()));

}
