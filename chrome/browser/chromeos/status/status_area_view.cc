// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/status/status_area_view.h"

#include <algorithm>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/message_loop.h"
#include "chrome/browser/chromeos/view_ids.h"
#include "ui/gfx/canvas.h"
#include "ui/views/border.h"

#if defined(USE_AURA)
#include "ui/views/widget/widget.h"
#endif

// Number of pixels to separate each icon.
#if defined(TOUCH_UI)
const int kSeparation = 25;
#else
const int kSeparation = 0;
#endif

StatusAreaView::StatusAreaView()
    : need_return_focus_(false) {
  set_id(VIEW_ID_STATUS_AREA);
}

StatusAreaView::~StatusAreaView() {
}

void StatusAreaView::AddButton(StatusAreaButton* button, bool bordered) {
  buttons_.push_back(button);
  if (bordered)
    button->set_border(views::Border::CreateEmptyBorder(0, 1, 0, 0));
  AddChildView(button);
  UpdateButtonVisibility();
}

void StatusAreaView::RemoveButton(StatusAreaButton* button) {
  std::list<StatusAreaButton*>::iterator iter =
      std::find(buttons_.begin(), buttons_.end(), button);
  if (iter != buttons_.end()) {
    RemoveChildView(*iter);
    buttons_.erase(iter);
  }
  UpdateButtonVisibility();
}

// views::View* overrides.

gfx::Size StatusAreaView::GetPreferredSize() {
  int result_w = 0;
  int result_h = 0;

  for (int i = 0; i < child_count(); i++) {
    views::View* cur = child_at(i);
    gfx::Size cur_size = cur->GetPreferredSize();
    if (cur->IsVisible() && !cur_size.IsEmpty()) {
      if (result_w == 0)
        result_w = kSeparation;

      // Add each width.
      result_w += cur_size.width() + kSeparation;
      // Use max height.
      result_h = std::max(result_h, cur_size.height());
    }
  }
  return gfx::Size(result_w, result_h);
}

void StatusAreaView::Layout() {
  int cur_x = kSeparation;
  for (int i = 0; i < child_count(); i++) {
    views::View* cur = child_at(i);
    gfx::Size cur_size = cur->GetPreferredSize();
    if (cur->IsVisible() && !cur_size.IsEmpty()) {
      int cur_y = (height() - cur_size.height()) / 2;

      // Handle odd number of pixels.
      cur_y += (height() - cur_size.height()) % 2;

      // Put next in row horizontally, and center vertically.
      cur->SetBounds(cur_x, cur_y, cur_size.width(), cur_size.height());
      cur_x += cur_size.width() + kSeparation;
    }
  }
}

void StatusAreaView::ChildPreferredSizeChanged(View* child) {
  // When something like the clock menu button's size changes, we need to
  // relayout. Also mark that this view's size has changed. This will let
  // BrowserView know to relayout, which will reset the bounds of this view.
  Layout();
  PreferredSizeChanged();
#if defined(USE_AURA)
  if (GetWidget())
    GetWidget()->SetSize(GetPreferredSize());
#endif
}

void StatusAreaView::MakeButtonsActive(bool active) {
  for (std::list<StatusAreaButton*>::iterator iter = buttons_.begin();
       iter != buttons_.end(); ++iter) {
    (*iter)->set_menu_active(active);
  }
}

void StatusAreaView::UpdateButtonVisibility() {
  Layout();
  PreferredSizeChanged();
}

void StatusAreaView::TakeFocus(
    bool reverse,
    const ReturnFocusCallback& return_focus_cb) {
  // Emulates focus receive by AccessiblePaneView::SetPaneFocus.
  if (!focus_manager_)
    focus_manager_ = GetFocusManager();
  focus_manager_->SetFocusedView(
      reverse ? GetLastFocusableChild() : GetFirstFocusableChild());
  pane_has_focus_ = true;
  need_return_focus_ = true;
  return_focus_cb_ = return_focus_cb;
  focus_manager_->AddFocusChangeListener(this);
}

void StatusAreaView::ReturnFocus(bool reverse) {
  // Emulates focus loss by AccessiblePaneView::RemovePaneFocus.
  if (!focus_manager_)
    focus_manager_ = GetFocusManager();
  focus_manager_->RemoveFocusChangeListener(this);
  pane_has_focus_ = false;
  need_return_focus_ = false;
  focus_manager_->ClearFocus();
  return_focus_cb_.Run(reverse);
}

void StatusAreaView::OnDidChangeFocus(views::View* focused_before,
                                      views::View* focused_now) {
  views::AccessiblePaneView::OnDidChangeFocus(focused_before, focused_now);
  if (need_return_focus_) {
    const views::View* first = GetFirstFocusableChild();
    const views::View* last = GetLastFocusableChild();
    const bool first_to_last = (focused_before == first && focused_now == last);
    const bool last_to_first = (focused_now == first && focused_before == last);

    if (first_to_last || last_to_first)
      ReturnFocus(first_to_last);
  }
}
