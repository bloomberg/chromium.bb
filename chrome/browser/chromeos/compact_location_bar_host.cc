// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/compact_location_bar_host.h"

#include <algorithm>

#include "app/l10n_util.h"
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
#include "chrome/browser/views/bookmark_bar_view.h"
#include "chrome/browser/views/find_bar_view.h"
#include "chrome/browser/views/frame/browser_view.h"
#include "chrome/browser/views/tabs/tab.h"
#include "chrome/browser/views/tabs/tab_strip.h"
#include "chrome/browser/views/toolbar_star_toggle.h"
#include "views/controls/scrollbar/native_scroll_bar.h"
#include "views/focus/external_focus_tracker.h"
#include "views/focus/view_storage.h"
#include "views/widget/root_view.h"
#include "views/widget/widget.h"

namespace chromeos {
const int kDefaultLocationBarWidth = 300;
const int kHideTimeoutInSeconds = 2;

// An mouse event observer to detect a mouse click on
// BrowserView's content area and hide the location bar.
class MouseObserver : public MessageLoopForUI::Observer {
 public:
  MouseObserver(CompactLocationBarHost* host, ::BrowserView* view)
      : host_(host),
        browser_view_(view) {
    top_level_window_ = browser_view_->GetWidget()->GetNativeView()->window;
  }

  // MessageLoopForUI::Observer overrides.
  virtual void WillProcessEvent(GdkEvent* event) {}
  virtual void DidProcessEvent(GdkEvent* event) {
    // Hide the location bar iff the mouse is pressed on the
    // BrowserView's content area.
    if (event->type == GDK_BUTTON_PRESS &&
        GDK_IS_WINDOW(event->any.window) &&
        top_level_window_ == gdk_window_get_toplevel(event->any.window) &&
        HitContentArea(event)) {
      host_->Hide(true);
    }
  }

 private:
  // Tests if the event occured on the content area, using
  // root window's coordinates.
  bool HitContentArea(GdkEvent* event) {
    gfx::Point p(event->button.x_root, event->button.y_root);
    // First, exclude the location bar as it's shown on top of
    // content area.
    if (HitOnScreen(host_->GetClbView(), p)) {
      return false;
    }
    // Treat the bookmark as a content area when it in detached mode.
    if (browser_view_->GetBookmarkBarView()->IsDetached() &&
        browser_view_->IsBookmarkBarVisible() &&
        HitOnScreen(browser_view_->GetBookmarkBarView(), p)) {
      return true;
    }
    if (HitOnScreen(browser_view_->GetContentsView(),
                    p)) {
      return true;
    }
    return false;
  }

  // Tests if |p| in the root window's coordinate is within the |view|'s bound.
  bool HitOnScreen(const views::View* view, const gfx::Point& p) {
    gfx::Point origin(0, 0);
    views::View::ConvertPointToScreen(view, &origin);
    gfx::Rect new_bounds(origin, view->size());
    return new_bounds.Contains(p);
  }

  CompactLocationBarHost* host_;
  ::BrowserView* browser_view_;
  GdkWindow* top_level_window_;

  DISALLOW_COPY_AND_ASSIGN(MouseObserver);
};

////////////////////////////////////////////////////////////////////////////////
// CompactLocationBarHost, public:

CompactLocationBarHost::CompactLocationBarHost(::BrowserView* browser_view)
    : DropdownBarHost(browser_view),
      current_tab_index_(-1) {
  auto_hide_timer_.reset(new base::OneShotTimer<CompactLocationBarHost>());
  mouse_observer_.reset(new MouseObserver(this, browser_view));
  Init(new CompactLocationBarView(this));
}

CompactLocationBarHost::~CompactLocationBarHost() {
  browser_view()->browser()->tabstrip_model()->RemoveObserver(this);
  MessageLoopForUI::current()->RemoveObserver(mouse_observer_.get());
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
  return new_pos;
}

void CompactLocationBarHost::SetDialogPosition(const gfx::Rect& new_pos,
                                               bool no_redraw) {
  if (new_pos.IsEmpty())
    return;

  // Make sure the window edges are clipped to just the visible region. We need
  // to do this before changing position, so that when we animate the closure
  // of it it doesn't look like the window crumbles into the toolbar.
  UpdateWindowEdges(new_pos);

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
  if (user_gesture) {
    // Show the compact location bar only when a user selected the tab.
    Update(index, false);
  } else {
    Hide(false);
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
  TabStrip* tabstrip = browser_view()->tabstrip()->AsTabStrip();
  gfx::Rect bounds = tabstrip->GetIdealBounds(index);
  gfx::Rect navbar_bounds(gfx::Point(bounds.x(), bounds.height()),
                          view()->GetPreferredSize());

  // For RTL case x() defines tab right corner.
  if (l10n_util::GetTextDirection() == l10n_util::RIGHT_TO_LEFT) {
    navbar_bounds.set_x(navbar_bounds.x() + bounds.width());
  }
  navbar_bounds.set_x(navbar_bounds.x() + tabstrip->x());
  navbar_bounds.set_y(navbar_bounds.y() + tabstrip->y());

  // The compact location bar must be smaller than browser_width.
  int width = std::min(browser_view()->width(),
                       view()->GetPreferredSize().width());

  // Try to center around the tab.
  navbar_bounds.set_x(browser_view()->MirroredXCoordinateInsideView(
      navbar_bounds.x()) - ((width - bounds.width()) / 2));

  if (browser_view()->IsBookmarkBarVisible() &&
      !browser_view()->GetBookmarkBarView()->IsDetached()) {
    // Adjust the location to create the illusion that the compact location bar
    // is a part of boolmark bar.
    // TODO(oshima): compact location bar does not have right background
    // image yet, so -2 is tentative. Fix this once UI is settled.
    navbar_bounds.set_y(
        browser_view()->GetBookmarkBarView()->bounds().bottom() - 2);
  }
  return navbar_bounds.AdjustToFit(browser_view()->bounds());
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

ToolbarStarToggle* CompactLocationBarHost::GetStarButton() {
  return GetClbView()->star_button();
}

void CompactLocationBarHost::Show(bool a) {
  MessageLoopForUI::current()->AddObserver(mouse_observer_.get());
  DropdownBarHost::Show(a);
}

void CompactLocationBarHost::Hide(bool a) {
  MessageLoopForUI::current()->RemoveObserver(mouse_observer_.get());
  DropdownBarHost::Hide(a);
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
