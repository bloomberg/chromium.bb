// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/compact_nav/compact_location_bar_view_host.h"

#if defined(TOOLKIT_USES_GTK)
#include <gtk/gtk.h>
#endif

#include <algorithm>

#include "base/i18n/rtl.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view.h"
#include "chrome/browser/ui/views/compact_nav/compact_location_bar_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/base_tab_strip.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/tab_contents/tab_contents_view.h"
#include "content/common/notification_source.h"
#include "ui/base/animation/slide_animation.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/gfx/rect.h"
#include "views/controls/scrollbar/native_scroll_bar.h"
#include "views/events/event.h"
#include "views/focus/external_focus_tracker.h"
#include "views/focus/view_storage.h"
#include "views/widget/root_view.h"
#include "views/widget/widget.h"

namespace {

const int kHideTimeoutInSeconds = 2;
// TODO(stevet): Share this with CompactLocationBarView. This is the actual
// height, without the overlap added.
const int kCompactNavbarSpacerHeight = 4;
const int kBookmarkBarLocationBarOverlap = 2;
const int kSpacerLocationbarOverlap = 1;

}  // namespace

// An mouse event observer to detect a mouse click on
// BrowserView's content area and hide the location bar.
class MouseObserver : public MessageLoopForUI::Observer {
 public:
  MouseObserver(CompactLocationBarViewHost* host, BrowserView* view);
  ~MouseObserver();

  // MessageLoopForUI::Observer overrides.
#if defined(OS_WIN)
  virtual void WillProcessMessage(const MSG& native_event) OVERRIDE;
  virtual void DidProcessMessage(const MSG& native_event) OVERRIDE;
#elif defined(OS_LINUX)
  virtual void WillProcessEvent(GdkEvent* native_event) OVERRIDE;
  virtual void DidProcessEvent(GdkEvent* native_event) OVERRIDE;
#endif

  void Observe(MessageLoopForUI* loop);
  void StopObserving(MessageLoopForUI* loop);

 private:
  // TODO(mad): would be nice to have this on the views::Event class.
  bool IsMouseEvent(const views::NativeEvent& native_event);

  bool IsSameTopLevelWindow(views::NativeEvent native_event);

  // Tests if the event occurred on the content area, using
  // root window's coordinates.
  bool HitContentArea(int x, int y);

  // Tests if |p| in the root window's coordinate is within the |view|'s bound.
  bool HitOnScreen(const views::View* view, const gfx::Point& p);

  CompactLocationBarViewHost* host_;
  BrowserView* browser_view_;
  gfx::NativeView top_level_window_;
  bool observing_;

  DISALLOW_COPY_AND_ASSIGN(MouseObserver);
};

MouseObserver::MouseObserver(CompactLocationBarViewHost* host,
                             BrowserView* view)
    : host_(host),
      browser_view_(view),
      observing_(false) {
  top_level_window_ = browser_view_->GetWidget()->GetNativeView();
}

MouseObserver::~MouseObserver() {
  StopObserving(MessageLoopForUI::current());
}

#if defined(OS_WIN)
void MouseObserver::WillProcessMessage(const MSG& native_event) {}
void MouseObserver::DidProcessMessage(const MSG& native_event) {
#elif defined(OS_LINUX)
void MouseObserver::WillProcessEvent(GdkEvent* native_event) {}
void MouseObserver::DidProcessEvent(GdkEvent* native_event) {
#endif
  // Hide the location bar iff the mouse is pressed on the
  // BrowserView's content area.
  if (!IsMouseEvent(native_event))
    return;
  views::MouseEvent event(native_event);
  if (event.type() == ui::ET_MOUSE_PRESSED &&
      IsSameTopLevelWindow(native_event) &&
      HitContentArea(event.x(), event.y())) {
    host_->Hide(true);
  }
}

void MouseObserver::Observe(MessageLoopForUI* loop) {
  if (!observing_) {
    loop->AddObserver(this);
    observing_ = true;
  }
}

void MouseObserver::StopObserving(MessageLoopForUI* loop) {
  if (observing_) {
    loop->RemoveObserver(this);
    observing_ = false;
  }
}

bool MouseObserver::IsMouseEvent(const views::NativeEvent& native_event) {
#if defined(OS_WIN)
  return views::IsClientMouseEvent(native_event) ||
         views::IsNonClientMouseEvent(native_event);
#elif defined(OS_LINUX)
  return native_event->type == GDK_MOTION_NOTIFY ||
         native_event->type == GDK_BUTTON_PRESS ||
         native_event->type == GDK_2BUTTON_PRESS ||
         native_event->type == GDK_3BUTTON_PRESS ||
         native_event->type == GDK_BUTTON_RELEASE;
#endif
}

// TODO(mad): Would be nice to have a NativeEvent -> NativeWindow mapping.
// Then, with a GetTopLevel receiving a NativeWindow, we could do this in a
// platform independent way.
bool MouseObserver::IsSameTopLevelWindow(views::NativeEvent native_event) {
#if defined(OS_WIN)
  return platform_util::GetTopLevel(native_event.hwnd) == top_level_window_;
#elif defined(OS_LINUX)
  return gdk_window_get_toplevel(
      reinterpret_cast<GdkEventAny*>(native_event)->window) ==
      top_level_window_->window;
#endif
}

bool MouseObserver::HitContentArea(int x, int y) {
  gfx::Point p(x, y);
  // First, exclude the location bar as it's shown on top of
  // content area.
  if (HitOnScreen(host_->view(), p)) {
    return false;
  }
  // Treat the bookmark as a content area when it in detached mode.
  if (browser_view_->GetBookmarkBarView()->IsDetached() &&
      browser_view_->IsBookmarkBarVisible() &&
      HitOnScreen(browser_view_->GetBookmarkBarView(), p)) {
    return true;
  }
  if (HitOnScreen(browser_view_->GetContentsView(), p)) {
    return true;
  }
  return false;
}

bool MouseObserver::HitOnScreen(const views::View* view, const gfx::Point& p) {
  gfx::Point origin(0, 0);
  views::View::ConvertPointToScreen(view, &origin);
  gfx::Rect new_bounds(origin, view->size());
  return new_bounds.Contains(p);
}

////////////////////////////////////////////////////////////////////////////////
// CompactLocationBarViewHost, public:

CompactLocationBarViewHost::CompactLocationBarViewHost(
    BrowserView* browser_view) : DropdownBarHost(browser_view),
                                 current_tab_model_index_(-1),
                                 is_observing_(false) {
  auto_hide_timer_.reset(new base::OneShotTimer<CompactLocationBarViewHost>());
  mouse_observer_.reset(new MouseObserver(this, browser_view));
  CompactLocationBarView* clbv = new CompactLocationBarView(this);
  Init(clbv, clbv);
}

CompactLocationBarViewHost::~CompactLocationBarViewHost() {
  // This may happen if we are destroyed during cleanup.
  if (browser_view() && browser_view()->browser() &&
      browser_view()->browser()->tabstrip_model()) {
    browser_view()->browser()->tabstrip_model()->RemoveObserver(this);
  }
}

CompactLocationBarView* CompactLocationBarViewHost::
    GetCompactLocationBarView() {
  return static_cast<CompactLocationBarView*>(view());
}

void CompactLocationBarViewHost::MoveWindowIfNecessary(
  const gfx::Rect& selection_rect,
  bool no_redraw) {
    // We only move the window if one is currently shown. If we don't check
    // this, then SetWidgetPosition below will end up making the Location Bar
    // visible.
    if (!IsVisible())
      return;

    gfx::Rect new_pos = GetDialogPosition(selection_rect);
    SetDialogPosition(new_pos, no_redraw);

    // May need to redraw our frame to accommodate bookmark bar styles.
    view()->SchedulePaint();
}

////////////////////////////////////////////////////////////////////////////////
// LocationBarView::Delegate implementation:
TabContentsWrapper* CompactLocationBarViewHost::GetTabContentsWrapper() const {
  return browser_view()->browser()->GetSelectedTabContentsWrapper();
}

InstantController* CompactLocationBarViewHost::GetInstant() {
  // TODO(stevet): Re-enable instant for compact nav.
  // return browser_view()->browser()->instant();
  return NULL;
}

void CompactLocationBarViewHost::OnInputInProgress(bool in_progress) {
}

////////////////////////////////////////////////////////////////////////////////
// CompactLocationBarViewHost, views::AcceleratorTarget implementation:

bool CompactLocationBarViewHost::AcceleratorPressed(
    const views::Accelerator& accelerator) {
  if (HasFocus()) {
    DCHECK(view() != NULL);
    views::FocusManager* focus_manager = view()->GetFocusManager();
    if (focus_manager)
      focus_manager->ClearFocus();
  }
  Hide(true);
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// CompactLocationBarViewHost, views::DropdownBarHost implementation:

gfx::Rect CompactLocationBarViewHost::GetDialogPosition(
    gfx::Rect avoid_overlapping_rect) {
  if (!browser_view() || !browser_view()->browser() ||
      !browser_view()->browser()->tabstrip_model()) {
    return gfx::Rect();
  }
  DCHECK_GE(current_tab_model_index_, 0);
  if (!browser_view()->browser()->tabstrip_model()->ContainsIndex(
      current_tab_model_index_)) {
    return gfx::Rect();
  }

  gfx::Rect new_pos = GetBoundsUnderTab(current_tab_model_index_);

  if (animation_offset() > 0)
    new_pos.Offset(0, std::min(0, -animation_offset()));
  return new_pos;
}

void CompactLocationBarViewHost::SetDialogPosition(const gfx::Rect& new_pos,
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
// CompactLocationBarViewHost, views::TabStripModelObserver implementation:

void CompactLocationBarViewHost::TabClosingAt(TabStripModel* tab_strip_model,
                                              TabContentsWrapper* contents,
                                              int index) {
  // TODO(stevet): We need to relocate the compact navigation bar if the
  // removed tab is not the one we are currently under but the tabstrip does
  // not have the ideal location yet because the tabs are animating at this
  // time. Need to investigate the best way to handle this case.
  Hide(false);
}

void CompactLocationBarViewHost::TabSelectedAt(TabContentsWrapper* old_contents,
                                               TabContentsWrapper* new_contents,
                                               int index,
                                               bool user_gesture) {
  current_tab_model_index_ = index;
  if (new_contents && new_contents->tab_contents()->is_loading()) {
    Show(false);
  } else {
    Hide(false);
  }
}

void CompactLocationBarViewHost::TabMoved(TabContentsWrapper* contents,
                                          int from_index,
                                          int to_index) {
  if (from_index == current_tab_model_index_) {
    UpdateOnTabChange(to_index, false);
    StartAutoHideTimer();
  }
}

void CompactLocationBarViewHost::TabChangedAt(TabContentsWrapper* contents,
                                              int index,
                                              TabChangeType change_type) {
  if (IsCurrentTabIndex(index) && change_type ==
      TabStripModelObserver::LOADING_ONLY) {
    bool was_not_visible = !IsVisible();
    TabContents* tab_contents = contents->tab_contents();
    Update(tab_contents, false);
    if (was_not_visible) {
      if (tab_contents->is_loading()) {
        // Register to NavigationController LOAD_STOP so that we can autohide
        // when loading is done.
        if (!registrar_.IsRegistered(this, NotificationType::LOAD_STOP,
            Source<NavigationController>(&tab_contents->controller()))) {
          registrar_.Add(this, NotificationType::LOAD_STOP,
              Source<NavigationController>(&tab_contents->controller()));
        }
      } else {
        StartAutoHideTimer();
      }
    }
  }
}

void CompactLocationBarViewHost::ActiveTabClicked(int index) {
  UpdateOnTabChange(index, true);
}

////////////////////////////////////////////////////////////////////////////////
// CompactLocationBarViewHost, NotificationObserver implementation:

void CompactLocationBarViewHost::Observe(NotificationType type,
                                         const NotificationSource& source,
                                         const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::LOAD_STOP: {
      StartAutoHideTimer();
      // This is one shot deal...
      registrar_.Remove(this, NotificationType::LOAD_STOP, source);
      break;
    }
    default:
      NOTREACHED();
  }
}

////////////////////////////////////////////////////////////////////////////////
// CompactLocationBarViewHost public:

gfx::Rect CompactLocationBarViewHost::GetBoundsUnderTab(int model_index) const {
  DCHECK(!browser_view()->UseVerticalTabs());

  // Get the position of the left-bottom corner of the tab on the
  // widget.  The widget of the tab is same as the widget of the
  // BrowserView which is the parent of the host.
  BaseTabStrip* tabstrip =
      static_cast<BaseTabStrip*>(browser_view()->tabstrip());
  gfx::Rect tab_bounds =
      tabstrip->ideal_bounds(tabstrip->ModelIndexToTabIndex(model_index));
  gfx::Rect navbar_bounds(gfx::Point(tab_bounds.x(), tab_bounds.height()),
                          view()->GetPreferredSize());

  // Convert our point to be relative to the widget, since the native code that
  // draws the dropdown is not in the BrowserView coordinate system.
  gfx::Point origin = navbar_bounds.origin();
  views::View::ConvertPointToWidget(browser_view(), &origin);
  navbar_bounds.set_origin(origin);

  // For RTL case x() defines tab right corner.
  if (base::i18n::IsRTL())
    navbar_bounds.Offset(tab_bounds.width(), 0);
  navbar_bounds.Offset(tabstrip->x(), tabstrip->y());

  // The compact location bar must be smaller than browser_width.
  int width = std::min(browser_view()->width(),
                       view()->GetPreferredSize().width());

  // Try to center around the tab.
  navbar_bounds.set_x(browser_view()->GetMirroredXInView(
      navbar_bounds.x()) - ((width - tab_bounds.width()) / 2));

  // Adjust the location to create the illusion that the compact location bar
  // is a part of the spacer or bookmark bar, depending on which is showing.
  if (browser_view()->IsBookmarkBarVisible() &&
      !browser_view()->GetBookmarkBarView()->IsDetached()) {
    // TODO(stevet): Compact location bar does not have right background image
    // yet, so kBookmarkBarLocationBarOverlap is tentative. Fix this once UI is
    // settled. This may be entirely replaced by a popup CLB, anyway.
    navbar_bounds.Offset(0,
        browser_view()->GetBookmarkBarView()->bounds().height() +
        kCompactNavbarSpacerHeight - kBookmarkBarLocationBarOverlap);
  } else {
    // TODO(stevet): kSpacerLocationbarOverlap is tentative as well, as above.
    navbar_bounds.Offset(0, kCompactNavbarSpacerHeight -
                         kSpacerLocationbarOverlap);
  }

  // TODO(stevet): Adjust to the right if there is an info bar visible.
  return navbar_bounds.AdjustToFit(browser_view()->bounds());
}

void CompactLocationBarViewHost::Update(TabContents* contents, bool animate) {
  // Don't animate if the bar is already shown.
  bool showing_in_same_tab = animation()->IsShowing() && IsCurrentTab(contents);
  current_tab_model_index_ = browser_view()->browser()->active_index();
  Hide(false);
  GetCompactLocationBarView()->Update(contents);
  Show(animate && !showing_in_same_tab);
  // If the tab is loading, we must wait for the notification that it is done.
  if (contents && !contents->is_loading()) {
    // This will be a NOOP if we have focus.
    // We never want to stay up, unless we have focus.
    StartAutoHideTimer();
  }
}

void CompactLocationBarViewHost::UpdateOnTabChange(int model_index,
                                                   bool animate) {
  DCHECK_GE(model_index, 0);
  if (IsCurrentTabIndex(model_index) && animation()->IsShowing()) {
    return;
  }
  current_tab_model_index_ = model_index;
  Update(browser_view()->browser()->tabstrip_model()->
         GetTabContentsAt(model_index)->tab_contents(), animate);
}

void CompactLocationBarViewHost::StartAutoHideTimer() {
  if (!IsVisible() || HasFocus())
    return;

  if (auto_hide_timer_->IsRunning()) {
    // Restart the timer.
    auto_hide_timer_->Reset();
  } else {
    auto_hide_timer_->Start(base::TimeDelta::FromSeconds(kHideTimeoutInSeconds),
                            this, &CompactLocationBarViewHost::HideCallback);
  }
}

void CompactLocationBarViewHost::CancelAutoHideTimer() {
  auto_hide_timer_->Stop();
}

void CompactLocationBarViewHost::SetEnabled(bool enabled) {
  if (enabled && !is_observing_) {
    browser_view()->browser()->tabstrip_model()->AddObserver(this);
    is_observing_ = true;
  } else {
    browser_view()->browser()->tabstrip_model()->RemoveObserver(this);
    is_observing_ = false;
  }
}

void CompactLocationBarViewHost::Show(bool animate) {
  CancelAutoHideTimer();
  mouse_observer_->Observe(MessageLoopForUI::current());
  DropdownBarHost::Show(animate);
  host()->Show();
}

void CompactLocationBarViewHost::Hide(bool animate) {
  CancelAutoHideTimer();
  mouse_observer_->StopObserving(MessageLoopForUI::current());
  host()->Hide();
  DropdownBarHost::Hide(animate);
}

////////////////////////////////////////////////////////////////////////////////
// CompactLocationBarViewHost private:
bool CompactLocationBarViewHost::HasFocus() {
  DCHECK(view() != NULL);
  views::FocusManager* focus_manager = view()->GetFocusManager();
  return focus_manager && view()->Contains(focus_manager->GetFocusedView());
}

void CompactLocationBarViewHost::HideCallback() {
  if (IsVisible() && !HasFocus())
    Hide(true);
}

bool CompactLocationBarViewHost::IsCurrentTabIndex(int index) {
  return current_tab_model_index_ == index;
}

bool CompactLocationBarViewHost::IsCurrentTab(TabContents* contents) {
  TabStripModel* tab_strip_model = browser_view()->browser()->tabstrip_model();
  return tab_strip_model->ContainsIndex(current_tab_model_index_) &&
      tab_strip_model->GetTabContentsAt(current_tab_model_index_)->
          tab_contents() == contents;
}
