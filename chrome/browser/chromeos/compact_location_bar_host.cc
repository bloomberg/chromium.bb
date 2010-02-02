// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/compact_location_bar_host.h"

#include "app/slide_animation.h"
#include "base/keyboard_codes.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/compact_location_bar_view.h"
#include "chrome/browser/find_bar_controller.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_view.h"
#include "chrome/browser/view_ids.h"
#include "chrome/browser/views/find_bar_view.h"
#include "chrome/browser/views/frame/browser_view.h"
#include "chrome/browser/views/tabs/tab.h"
#include "chrome/browser/views/tabs/tab_strip.h"
#include "views/controls/scrollbar/native_scroll_bar.h"
#include "views/focus/external_focus_tracker.h"
#include "views/focus/view_storage.h"
#include "views/widget/root_view.h"
#include "views/widget/widget.h"

namespace chromeos {

const int kDefaultLocationBarWidth = 300;
const int kHideTimeoutInSeconds = 2;

////////////////////////////////////////////////////////////////////////////////
// CompactLocationBarHost, public:

CompactLocationBarHost::CompactLocationBarHost(BrowserView* browser_view)
    : DropdownBarHost(browser_view),
      current_tab_index_(-1) {
  auto_hide_timer_.reset(new base::OneShotTimer<CompactLocationBarHost>());
  Init(new CompactLocationBarView(this));
}

CompactLocationBarHost::~CompactLocationBarHost() {
  browser_view()->browser()->tabstrip_model()->RemoveObserver(this);
}

void CompactLocationBarHost::StartAutoHideTimer() {
  if (!host()->IsVisible())
    return;
  if (auto_hide_timer_->IsRunning()) {
    // Restart the timer.
    auto_hide_timer_->Reset();
  } else {
    auto_hide_timer_->Start(base::TimeDelta::FromSeconds(kHideTimeoutInSeconds),
                            this, &CompactLocationBarHost::HideCallback);
  }
}

////////////////////////////////////////////////////////////////////////////////
// CompactLocationBarHost, views::AcceleratorTarget implementation:

bool CompactLocationBarHost::AcceleratorPressed(
    const views::Accelerator& accelerator) {
  Hide(true);
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// CompactLocationBarHost, views::DropdownBarHost implementation:

gfx::Rect CompactLocationBarHost::GetDialogPosition(
    gfx::Rect avoid_overlapping_rect) {
  DCHECK_GE(current_tab_index_, 0);
  gfx::Rect new_pos = GetBoundsUnderTab(current_tab_index_);

  if (animation_offset() > 0)
    new_pos.Offset(0, std::min(0, -animation_offset()));

  // TODO(oshima): Animate the window clipping like find-bar.
  return new_pos;
}

void CompactLocationBarHost::SetDialogPosition(const gfx::Rect& new_pos,
                                               bool no_redraw) {
  if (new_pos.IsEmpty())
    return;

  // TODO(oshima): Animate the window clipping like find-bar.
  SetWidgetPositionNative(new_pos, no_redraw);
}

////////////////////////////////////////////////////////////////////////////////
// CompactLocationBarHost, views::TabStripModelObserver implementation:

void CompactLocationBarHost::TabInsertedAt(TabContents* contents,
                                           int index,
                                           bool foreground) {
  Hide(false);
  // TODO(oshima): Consult UX team if we should show the location bar.
}

void CompactLocationBarHost::TabClosingAt(TabContents* contents, int index) {
  if (IsCurrentTabIndex(index)) {
    Hide(false);
  } else {
    // TODO(oshima): We need to relocate the compact navigation bar here,
    // but the tabstrip does not have the ideal location yet
    // because the tabs are animating at this time. Need to investigate
    // the best way to handle this case.
  }
}

void CompactLocationBarHost::TabSelectedAt(TabContents* old_contents,
                                           TabContents* new_contents,
                                           int index,
                                           bool user_gesture) {
  Hide(false);
  if (user_gesture) {
    // Show the compact location bar only when a user selected the tab.
    Update(index, false);
  }
}

void CompactLocationBarHost::TabMoved(TabContents* contents,
                                      int from_index,
                                      int to_index) {
  Update(to_index, false);
}

void CompactLocationBarHost::TabChangedAt(TabContents* contents, int index,
                                          TabChangeType change_type) {
  if (IsCurrentTabIndex(index) && IsVisible()) {
    GetClbView()->Update(contents);
  }
}

////////////////////////////////////////////////////////////////////////////////
// CompactLocationBarHost public:

gfx::Rect CompactLocationBarHost::GetBoundsUnderTab(int index) const {
  // Get the position of the left-bottom corner of the tab on the
  // widget.  The widget of the tab is same as the widget of the
  // BrowserView which is the parent of the host.
  TabStrip* tabstrip = browser_view()->tabstrip();
  gfx::Rect bounds = tabstrip->GetIdealBounds(index);
  gfx::Point tab_left_bottom(bounds.x(), bounds.height());
  views::View::ConvertPointToWidget(tabstrip, &tab_left_bottom);

  // The compact location bar must be smaller than browser_width.
  int width = std::min(browser_view()->width(), kDefaultLocationBarWidth);

  // Try to center around the tab, or align to the left of the window.
  // TODO(oshima): handle RTL
  int x = std::max(tab_left_bottom.x() - ((width - bounds.width()) / 2), 0);
  return gfx::Rect(x, tab_left_bottom.y(), width, 28);
}

void CompactLocationBarHost::Update(int index, bool animate_x) {
  DCHECK_GE(index, 0);
  if (IsCurrentTabIndex(index) && IsVisible()) {
    return;
  }
  current_tab_index_ = index;
  // Don't aminate if the bar is already shown.
  bool animate = !animation()->IsShowing();
  Hide(false);
  GetClbView()->Update(browser_view()->browser()->GetSelectedTabContents());
  GetClbView()->SetFocusAndSelection();
  Show(animate && animate_x);
}

void CompactLocationBarHost::CancelAutoHideTimer() {
  auto_hide_timer_->Stop();
}

void CompactLocationBarHost::SetEnabled(bool enabled) {
  if (enabled) {
    browser_view()->browser()->tabstrip_model()->AddObserver(this);
  } else {
    browser_view()->browser()->tabstrip_model()->RemoveObserver(this);
  }
}

////////////////////////////////////////////////////////////////////////////////
// CompactLocationBarHost private:

CompactLocationBarView* CompactLocationBarHost::GetClbView() {
  return static_cast<CompactLocationBarView*>(view());
}

bool CompactLocationBarHost::IsCurrentTabIndex(int index) {
  return current_tab_index_ == index;
}

}  // namespace chromeos
