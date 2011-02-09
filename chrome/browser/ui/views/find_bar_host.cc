// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/find_bar_host.h"

#include <algorithm>

#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_view.h"
#include "chrome/browser/ui/find_bar/find_bar_controller.h"
#include "chrome/browser/ui/find_bar/find_manager.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/find_bar_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "views/focus/external_focus_tracker.h"
#include "views/focus/view_storage.h"
#include "views/widget/root_view.h"
#include "views/widget/widget.h"

namespace browser {

// Declared in browser_dialogs.h so others don't have to depend on our header.
FindBar* CreateFindBar(BrowserView* browser_view) {
  return new FindBarHost(browser_view);
}

}  // namespace browser

////////////////////////////////////////////////////////////////////////////////
// FindBarHost, public:

FindBarHost::FindBarHost(BrowserView* browser_view)
    : DropdownBarHost(browser_view),
      find_bar_controller_(NULL) {
  Init(new FindBarView(this));
}

FindBarHost::~FindBarHost() {
}

bool FindBarHost::MaybeForwardKeyEventToWebpage(
    const views::KeyEvent& key_event) {
  if (!ShouldForwardKeyEventToWebpageNative(key_event)) {
    // Native implementation says not to forward these events.
    return false;
  }

  switch (key_event.GetKeyCode()) {
    case ui::VKEY_DOWN:
    case ui::VKEY_UP:
    case ui::VKEY_PRIOR:
    case ui::VKEY_NEXT:
      break;
    case ui::VKEY_HOME:
    case ui::VKEY_END:
      if (key_event.IsControlDown())
        break;
    // Fall through.
    default:
      return false;
  }

  TabContentsWrapper* contents = find_bar_controller_->tab_contents();
  if (!contents)
    return false;

  RenderViewHost* render_view_host = contents->render_view_host();

  // Make sure we don't have a text field element interfering with keyboard
  // input. Otherwise Up and Down arrow key strokes get eaten. "Nom Nom Nom".
  render_view_host->ClearFocusedNode();
  NativeWebKeyboardEvent event = GetKeyboardEvent(contents->tab_contents(),
                                                  key_event);
  render_view_host->ForwardKeyboardEvent(event);
  return true;
}

FindBarController* FindBarHost::GetFindBarController() const {
  return find_bar_controller_;
}

void FindBarHost::SetFindBarController(FindBarController* find_bar_controller) {
  find_bar_controller_ = find_bar_controller;
}

void FindBarHost::Show(bool animate) {
  DropdownBarHost::Show(animate);
}

void FindBarHost::Hide(bool animate) {
  DropdownBarHost::Hide(animate);
}

void FindBarHost::SetFocusAndSelection() {
  DropdownBarHost::SetFocusAndSelection();
}

void FindBarHost::ClearResults(const FindNotificationDetails& results) {
  find_bar_view()->UpdateForResult(results, string16());
}

void FindBarHost::StopAnimation() {
  DropdownBarHost::StopAnimation();
}

void FindBarHost::MoveWindowIfNecessary(const gfx::Rect& selection_rect,
                                        bool no_redraw) {
  // We only move the window if one is active for the current TabContents. If we
  // don't check this, then SetWidgetPosition below will end up making the Find
  // Bar visible.
  if (!find_bar_controller_->tab_contents() ||
      !find_bar_controller_->
          tab_contents()->GetFindManager()->find_ui_active()) {
    return;
  }

  gfx::Rect new_pos = GetDialogPosition(selection_rect);
  SetDialogPosition(new_pos, no_redraw);

  // May need to redraw our frame to accommodate bookmark bar styles.
  view()->SchedulePaint();
}

void FindBarHost::SetFindText(const string16& find_text) {
  find_bar_view()->SetFindText(find_text);
}

void FindBarHost::UpdateUIForFindResult(const FindNotificationDetails& result,
                                        const string16& find_text) {
  // Make sure match count is clear. It may get set again in UpdateForResult
  // if enough data is available.
  find_bar_view()->ClearMatchCount();

  if (!find_text.empty())
    find_bar_view()->UpdateForResult(result, find_text);

  // We now need to check if the window is obscuring the search results.
  if (!result.selection_rect().IsEmpty())
    MoveWindowIfNecessary(result.selection_rect(), false);

  // Once we find a match we no longer want to keep track of what had
  // focus. EndFindSession will then set the focus to the page content.
  if (result.number_of_matches() > 0)
    ResetFocusTracker();
}

bool FindBarHost::IsFindBarVisible() {
  return DropdownBarHost::IsVisible();
}

void FindBarHost::RestoreSavedFocus() {
  if (focus_tracker() == NULL) {
    // TODO(brettw) Focus() should be on TabContentsView.
    find_bar_controller_->tab_contents()->tab_contents()->Focus();
  } else {
    focus_tracker()->FocusLastFocusedExternalView();
  }
}

FindBarTesting* FindBarHost::GetFindBarTesting() {
  return this;
}

////////////////////////////////////////////////////////////////////////////////
// FindBarWin, views::AcceleratorTarget implementation:

bool FindBarHost::AcceleratorPressed(const views::Accelerator& accelerator) {
  ui::KeyboardCode key = accelerator.GetKeyCode();
  if (key == ui::VKEY_RETURN && accelerator.IsCtrlDown()) {
    // Ctrl+Enter closes the Find session and navigates any link that is active.
    find_bar_controller_->EndFindSession(FindBarController::kActivateSelection);
  } else if (key == ui::VKEY_ESCAPE) {
    // This will end the Find session and hide the window, causing it to loose
    // focus and in the process unregister us as the handler for the Escape
    // accelerator through the FocusWillChange event.
    find_bar_controller_->EndFindSession(FindBarController::kKeepSelection);
  } else {
    NOTREACHED() << "Unknown accelerator";
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
// FindBarTesting implementation:

bool FindBarHost::GetFindBarWindowInfo(gfx::Point* position,
                                      bool* fully_visible) {
  if (!find_bar_controller_ ||
#if defined(OS_WIN)
      !::IsWindow(host()->GetNativeView())) {
#else
      false) {
      // TODO(sky): figure out linux side.
      // This is tricky due to asynchronous nature of x11.
      // See bug http://crbug.com/28629.
#endif
    if (position)
      *position = gfx::Point();
    if (fully_visible)
      *fully_visible = false;
    return false;
  }

  gfx::Rect window_rect;
  host()->GetBounds(&window_rect, true);
  if (position)
    *position = window_rect.origin();
  if (fully_visible)
    *fully_visible = IsVisible() && !IsAnimating();
  return true;
}

string16 FindBarHost::GetFindText() {
  return find_bar_view()->GetFindText();
}

string16 FindBarHost::GetFindSelectedText() {
  return find_bar_view()->GetFindSelectedText();
}

string16 FindBarHost::GetMatchCountText() {
  return find_bar_view()->GetMatchCountText();
}

////////////////////////////////////////////////////////////////////////////////
// Overridden from DropdownBarHost:

gfx::Rect FindBarHost::GetDialogPosition(gfx::Rect avoid_overlapping_rect) {
  // Find the area we have to work with (after accounting for scrollbars, etc).
  gfx::Rect widget_bounds;
  GetWidgetBounds(&widget_bounds);
  if (widget_bounds.IsEmpty())
    return gfx::Rect();

  // Ask the view how large an area it needs to draw on.
  gfx::Size prefsize = view()->GetPreferredSize();

  // Place the view in the top right corner of the widget boundaries (top left
  // for RTL languages).
  gfx::Rect view_location;
  int x = widget_bounds.x();
  if (!base::i18n::IsRTL())
    x += widget_bounds.width() - prefsize.width();
  int y = widget_bounds.y();
  view_location.SetRect(x, y, prefsize.width(), prefsize.height());

  // When we get Find results back, we specify a selection rect, which we
  // should strive to avoid overlapping. But first, we need to offset the
  // selection rect (if one was provided).
  if (!avoid_overlapping_rect.IsEmpty()) {
    // For comparison (with the Intersects function below) we need to account
    // for the fact that we draw the Find widget relative to the Chrome frame,
    // whereas the selection rect is relative to the page.
    GetWidgetPositionNative(&avoid_overlapping_rect);
  }

  gfx::Rect new_pos = FindBarController::GetLocationForFindbarView(
      view_location, widget_bounds, avoid_overlapping_rect);

  // While we are animating, the Find window will grow bottoms up so we need to
  // re-position the widget so that it appears to grow out of the toolbar.
  if (animation_offset() > 0)
    new_pos.Offset(0, std::min(0, -animation_offset()));

  return new_pos;
}

void FindBarHost::SetDialogPosition(const gfx::Rect& new_pos, bool no_redraw) {
  if (new_pos.IsEmpty())
    return;

  // Make sure the window edges are clipped to just the visible region. We need
  // to do this before changing position, so that when we animate the closure
  // of it it doesn't look like the window crumbles into the toolbar.
  UpdateWindowEdges(new_pos);

  SetWidgetPositionNative(new_pos, no_redraw);
}

void FindBarHost::GetWidgetBounds(gfx::Rect* bounds) {
  DCHECK(bounds);
  // The BrowserView does Layout for the components that we care about
  // positioning relative to, so we ask it to tell us where we should go.
  *bounds = browser_view()->GetFindBarBoundingBox();
}

void FindBarHost::RegisterAccelerators() {
  DropdownBarHost::RegisterAccelerators();

  // Register for Ctrl+Return.
  views::Accelerator escape(ui::VKEY_RETURN, false, true, false);
  focus_manager()->RegisterAccelerator(escape, this);
}

void FindBarHost::UnregisterAccelerators() {
  // Unregister Ctrl+Return.
  views::Accelerator escape(ui::VKEY_RETURN, false, true, false);
  focus_manager()->UnregisterAccelerator(escape, this);

  DropdownBarHost::UnregisterAccelerators();
}

////////////////////////////////////////////////////////////////////////////////
// private:

FindBarView* FindBarHost::find_bar_view() {
  return static_cast<FindBarView*>(view());
}
