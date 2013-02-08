// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/stacked_tab_strip_layout.h"

#include <stdio.h>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"

StackedTabStripLayout::StackedTabStripLayout(const gfx::Size& size,
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
      mini_tab_to_non_mini_tab_(0),
      active_index_(-1),
      first_tab_x_(0) {
}

StackedTabStripLayout::~StackedTabStripLayout() {
}

void StackedTabStripLayout::SetXAndMiniCount(int x, int mini_tab_count) {
  first_tab_x_ = x;
  x_ = x;
  mini_tab_count_ = mini_tab_count;
  mini_tab_to_non_mini_tab_ = 0;
  if (!requires_stacking() || tab_count() == mini_tab_count) {
    ResetToIdealState();
    return;
  }
  if (mini_tab_count > 0) {
    mini_tab_to_non_mini_tab_ = x - ideal_x(mini_tab_count - 1);
    first_tab_x_ = ideal_x(0);
  }
  SetIdealBoundsAt(active_index(), ConstrainActiveX(ideal_x(active_index())));
  LayoutByTabOffsetAfter(active_index());
  LayoutByTabOffsetBefore(active_index());
}

void StackedTabStripLayout::SetWidth(int width) {
  if (width_ == width)
    return;

  width_ = width;
  if (!requires_stacking()) {
    ResetToIdealState();
    return;
  }
  SetActiveBoundsAndLayoutFromActiveTab();
}

void StackedTabStripLayout::SetActiveIndex(int index) {
  int old = active_index();
  active_index_ = index;
  if (old == active_index() || !requires_stacking())
    return;
  SetIdealBoundsAt(active_index(), ConstrainActiveX(ideal_x(active_index())));
  LayoutByTabOffsetBefore(active_index());
  LayoutByTabOffsetAfter(active_index());
  AdjustStackedTabs();
}

void StackedTabStripLayout::DragActiveTab(int delta) {
  if (delta == 0 || !requires_stacking())
    return;
  int initial_x = ideal_x(active_index());
  // If we're at a particular edge and start dragging, expose all the tabs after
  // the tab (or before when dragging to the left).
  if (delta > 0 && initial_x == GetMinX(active_index())) {
    LayoutByTabOffsetAfter(active_index());
    AdjustStackedTabs();
  } else if (delta < 0 && initial_x == GetMaxX(active_index())) {
    LayoutByTabOffsetBefore(active_index());
    ResetToIdealState();
  }
  int x = delta > 0 ?
      std::min(initial_x + delta, GetMaxDragX(active_index())) :
      std::max(initial_x + delta, GetMinDragX(active_index()));
  if (x != initial_x) {
    SetIdealBoundsAt(active_index(), x);
    if (delta > 0) {
      PushTabsAfter(active_index(), (x - initial_x));
      LayoutForDragBefore(active_index());
    } else {
      PushTabsBefore(active_index(), initial_x - x);
      LayoutForDragAfter(active_index());
    }
    delta -= (x - initial_x);
  }
  if (delta > 0)
    ExpandTabsBefore(active_index(), delta);
  else if (delta < 0)
    ExpandTabsAfter(active_index(), -delta);
  AdjustStackedTabs();
}

void StackedTabStripLayout::SizeToFit() {
  if (!tab_count())
    return;

  if (!requires_stacking()) {
    ResetToIdealState();
    return;
  }

  if (ideal_x(0) != first_tab_x_) {
    // Tabs have been dragged to the right. Pull in the tabs from left to right
    // to fill in space.
    int delta = ideal_x(0) - first_tab_x_;
    int i = 0;
    for (; i < mini_tab_count_; ++i) {
      gfx::Rect mini_bounds(view_model_->ideal_bounds(i));
      mini_bounds.set_x(ideal_x(i) - delta);
      view_model_->set_ideal_bounds(i, mini_bounds);
    }
    for (; delta > 0 && i < tab_count() - 1; ++i) {
      const int exposed = tab_offset() - (ideal_x(i + 1) - ideal_x(i));
      SetIdealBoundsAt(i, ideal_x(i) - delta);
      delta -= exposed;
    }
    AdjustStackedTabs();
    return;
  }

  const int max_x = width_ - size_.width();
  if (ideal_x(tab_count() - 1) == max_x)
    return;

  // Tabs have been dragged to the left. Pull in tabs from right to left to fill
  // in space.
  SetIdealBoundsAt(tab_count() - 1, max_x);
  for (int i = tab_count() - 2; i > mini_tab_count_ &&
           ideal_x(i + 1) - ideal_x(i) > tab_offset(); --i) {
    SetIdealBoundsAt(i, ideal_x(i + 1) - tab_offset());
  }
  AdjustStackedTabs();
}

void StackedTabStripLayout::AddTab(int index, int add_types, int start_x) {
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

void StackedTabStripLayout::RemoveTab(int index, int start_x, int old_x) {
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

void StackedTabStripLayout::MoveTab(int from,
                                    int to,
                                    int new_active_index,
                                    int start_x,
                                    int mini_tab_count) {
  x_ = start_x;
  mini_tab_count_ = mini_tab_count;
  active_index_ = new_active_index;
  if (!requires_stacking() || tab_count() == mini_tab_count_) {
    ResetToIdealState();
  } else {
    SetIdealBoundsAt(active_index(),
                     ConstrainActiveX(ideal_x(active_index())));
    LayoutByTabOffsetAfter(active_index());
    LayoutByTabOffsetBefore(active_index());
    AdjustStackedTabs();
  }
  mini_tab_to_non_mini_tab_ = mini_tab_count > 0 ?
      start_x - ideal_x(mini_tab_count - 1) : 0;
  first_tab_x_ = mini_tab_count > 0 ? ideal_x(0) : start_x;
}

bool StackedTabStripLayout::IsStacked(int index) const {
  if (index == active_index() || tab_count() == mini_tab_count_ ||
      index < mini_tab_count_)
    return false;
  if (index > active_index())
    return ideal_x(index) != ideal_x(index - 1) + tab_offset();
  return ideal_x(index + 1) != ideal_x(index) + tab_offset();
}

void StackedTabStripLayout::SetActiveTabLocation(int x) {
  if (!requires_stacking())
    return;

  const int index = active_index();
  if (index <= mini_tab_count_)
    return;

  x = std::min(GetMaxX(index), std::max(x, GetMinX(index)));
  if (x == ideal_x(index))
    return;

  SetIdealBoundsAt(index, x);
  LayoutByTabOffsetBefore(index);
  LayoutByTabOffsetAfter(index);
}

#if !defined(NDEBUG)
std::string StackedTabStripLayout::BoundsString() const {
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

void StackedTabStripLayout::Reset(int x,
                                  int width,
                                  int mini_tab_count,
                                  int active_index) {
  x_ = x;
  width_ = width;
  mini_tab_count_ = mini_tab_count;
  mini_tab_to_non_mini_tab_ = mini_tab_count > 0 ?
      x - ideal_x(mini_tab_count - 1) : 0;
  first_tab_x_ = mini_tab_count > 0 ? ideal_x(0) : x;
  active_index_ = active_index;
  ResetToIdealState();
}

void StackedTabStripLayout::ResetToIdealState() {
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

void StackedTabStripLayout::MakeVisible(int index) {
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

int StackedTabStripLayout::ConstrainActiveX(int x) const {
  return std::min(GetMaxX(active_index()),
                  std::max(GetMinX(active_index()), x));
}

void StackedTabStripLayout::SetActiveBoundsAndLayoutFromActiveTab() {
  int x = ConstrainActiveX(ideal_x(active_index()));
  SetIdealBoundsAt(active_index(), x);
  LayoutUsingCurrentBefore(active_index());
  LayoutUsingCurrentAfter(active_index());
  AdjustStackedTabs();
}

void StackedTabStripLayout::LayoutByTabOffsetAfter(int index) {
  for (int i = index + 1; i < tab_count(); ++i) {
    int max_x = width_ - size_.width() -
        stacked_padding_for_count(tab_count() - i - 1);
    int x = std::min(max_x,
                     view_model_->ideal_bounds(i - 1).x() + tab_offset());
    SetIdealBoundsAt(i, x);
  }
}

void StackedTabStripLayout::LayoutByTabOffsetBefore(int index) {
  for (int i = index - 1; i >= mini_tab_count_; --i) {
    int min_x = x_ + stacked_padding_for_count(i - mini_tab_count_);
    int x = std::max(min_x, ideal_x(i + 1) - (tab_offset()));
    SetIdealBoundsAt(i, x);
  }
}

void StackedTabStripLayout::LayoutUsingCurrentAfter(int index) {
  for (int i = index + 1; i < tab_count(); ++i) {
    int min_x = width_ - width_for_count(tab_count() - i);
    int x = std::max(min_x,
                     std::min(ideal_x(i), ideal_x(i - 1) + tab_offset()));
    x = std::min(GetMaxX(i), x);
    SetIdealBoundsAt(i, x);
  }
}

void StackedTabStripLayout::LayoutUsingCurrentBefore(int index) {
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

void StackedTabStripLayout::PushTabsAfter(int index, int delta) {
  for (int i = index + 1; i < tab_count(); ++i)
    SetIdealBoundsAt(i, std::min(ideal_x(i) + delta, GetMaxDragX(i)));
}

void StackedTabStripLayout::PushTabsBefore(int index, int delta) {
  for (int i = index - 1; i > mini_tab_count_; --i)
    SetIdealBoundsAt(i, std::max(ideal_x(i) - delta, GetMinDragX(i)));
}

void StackedTabStripLayout::LayoutForDragAfter(int index) {
  for (int i = index + 1; i < tab_count(); ++i) {
    const int min_x = ideal_x(i - 1) + stacked_padding_;
    const int max_x = ideal_x(i - 1) + tab_offset();
    SetIdealBoundsAt(
        i, std::max(min_x, std::min(ideal_x(i), max_x)));
  }
}

void StackedTabStripLayout::LayoutForDragBefore(int index) {
  for (int i = index - 1; i >= mini_tab_count_; --i) {
    const int max_x = ideal_x(i + 1) - stacked_padding_;
    const int min_x = ideal_x(i + 1) - tab_offset();
    SetIdealBoundsAt(
        i, std::max(min_x, std::min(ideal_x(i), max_x)));
  }

  if (mini_tab_count_ == 0)
    return;

  // Pull in the mini-tabs.
  const int delta = (mini_tab_count_ > 1) ? ideal_x(1) - ideal_x(0) : 0;
  for (int i = mini_tab_count_ - 1; i >= 0; --i) {
    gfx::Rect mini_bounds(view_model_->ideal_bounds(i));
    if (i == mini_tab_count_ - 1)
      mini_bounds.set_x(ideal_x(i + 1) - mini_tab_to_non_mini_tab_);
    else
      mini_bounds.set_x(ideal_x(i + 1) - delta);
    view_model_->set_ideal_bounds(i, mini_bounds);
  }
}

void StackedTabStripLayout::ExpandTabsBefore(int index, int delta) {
  for (int i = index - 1; i >= mini_tab_count_ && delta > 0; --i) {
    const int max_x = ideal_x(active_index()) -
        stacked_padding_for_count(active_index() - i);
    int to_resize = std::min(delta, max_x - ideal_x(i));

    if (to_resize <= 0)
      continue;
    SetIdealBoundsAt(i, ideal_x(i) + to_resize);
    delta -= to_resize;
    LayoutForDragBefore(i);
  }
}

void StackedTabStripLayout::ExpandTabsAfter(int index, int delta) {
  if (index == tab_count() - 1)
    return;  // Nothing to expand.

  for (int i = index + 1; i < tab_count() && delta > 0; ++i) {
    const int min_compressed =
        ideal_x(active_index()) + stacked_padding_for_count(i - active_index());
    const int to_resize = std::min(ideal_x(i) - min_compressed, delta);
    if (to_resize <= 0)
      continue;
    SetIdealBoundsAt(i, ideal_x(i) - to_resize);
    delta -= to_resize;
    LayoutForDragAfter(i);
  }
}

void StackedTabStripLayout::AdjustStackedTabs() {
  if (!requires_stacking() || tab_count() <= mini_tab_count_ + 1)
    return;

  AdjustLeadingStackedTabs();
  AdjustTrailingStackedTabs();
}

void StackedTabStripLayout::AdjustLeadingStackedTabs() {
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

void StackedTabStripLayout::AdjustTrailingStackedTabs() {
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

void StackedTabStripLayout::SetIdealBoundsAt(int index, int x) {
  view_model_->set_ideal_bounds(index, gfx::Rect(gfx::Point(x, 0), size_));
}

int StackedTabStripLayout::GetMinX(int index) const {
  int leading_count = index - mini_tab_count_;
  int trailing_count = tab_count() - index;
  return std::max(x_ + stacked_padding_for_count(leading_count),
                  width_ - width_for_count(trailing_count));
}

int StackedTabStripLayout::GetMaxX(int index) const {
  int leading_count = index - mini_tab_count_;
  int trailing_count = tab_count() - index - 1;
  int trailing_offset = stacked_padding_for_count(trailing_count);
  int leading_size = width_for_count(leading_count) + x_;
  if (leading_count > 0)
    leading_size += padding_;
  return std::min(width_ - trailing_offset - size_.width(), leading_size);
}

int StackedTabStripLayout::GetMinDragX(int index) const {
  return x_ + stacked_padding_for_count(index - mini_tab_count_);
}

int StackedTabStripLayout::GetMaxDragX(int index) const {
  const int trailing_offset =
      stacked_padding_for_count(tab_count() - index - 1);
  return width_ - trailing_offset - size_.width();
}

int StackedTabStripLayout::GetMinXCompressed(int index) const {
  DCHECK_GT(index, active_index());
  return std::max(
      width_ - width_for_count(tab_count() - index),
      ideal_x(active_index()) +
          stacked_padding_for_count(index - active_index()));
}
