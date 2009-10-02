// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/find_bar_host.h"

#include "app/slide_animation.h"
#include "base/keyboard_codes.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/find_bar_controller.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/view_ids.h"
#include "chrome/browser/views/find_bar_view.h"
#include "chrome/browser/views/frame/browser_view.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_view.h"
#include "views/focus/external_focus_tracker.h"
#include "views/focus/view_storage.h"
#include "views/controls/scrollbar/native_scroll_bar.h"
#include "views/widget/root_view.h"

// static
bool FindBarHost::disable_animations_during_testing_ = false;

namespace browser {

// Declared in browser_dialogs.h so others don't have to depend on our header.
FindBar* CreateFindBar(BrowserView* browser_view) {
  return new FindBarHost(browser_view);
}

}  // namespace browser

////////////////////////////////////////////////////////////////////////////////
// FindBarHost, public:

FindBarHost::FindBarHost(BrowserView* browser_view)
    : browser_view_(browser_view),
      find_dialog_animation_offset_(0),
      esc_accel_target_registered_(false),
      find_bar_controller_(NULL) {
  view_ = new FindBarView(this);

  // Initialize the host.
  host_.reset(CreateHost());
  host_->Init(GetNativeView(browser_view), gfx::Rect());
  host_->SetContentsView(view_);

  // Start listening to focus changes, so we can register and unregister our
  // own handler for Escape.
  focus_manager_ =
      views::FocusManager::GetFocusManagerForNativeView(host_->GetNativeView());
  if (focus_manager_) {
    focus_manager_->AddFocusChangeListener(this);
  } else {
    // In some cases (see bug http://crbug.com/17056) it seems we may not have
    // a focus manager.  Please reopen the bug if you hit this.
    NOTREACHED();
  }

  // Start the process of animating the opening of the window.
  animation_.reset(new SlideAnimation(this));
}

FindBarHost::~FindBarHost() {
  focus_manager_->RemoveFocusChangeListener(this);
  focus_tracker_.reset(NULL);
}

void FindBarHost::Show() {
  // Stores the currently focused view, and tracks focus changes so that we can
  // restore focus when the find box is closed.
  focus_tracker_.reset(new views::ExternalFocusTracker(view_, focus_manager_));

  if (disable_animations_during_testing_) {
    animation_->Reset(1);
    MoveWindowIfNecessary(gfx::Rect(), true);
  } else {
    animation_->Reset();
    animation_->Show();
  }
}

void FindBarHost::SetFocusAndSelection() {
  view_->SetFocusAndSelection();
}

bool FindBarHost::IsAnimating() {
  return animation_->IsAnimating();
}

void FindBarHost::Hide(bool animate) {
  if (animate && !disable_animations_during_testing_) {
    animation_->Reset(1.0);
    animation_->Hide();
  } else {
    host_->Hide();
  }
}

void FindBarHost::ClearResults(const FindNotificationDetails& results) {
  view_->UpdateForResult(results, string16());
}

void FindBarHost::StopAnimation() {
  animation_->End();
}

void FindBarHost::SetFindText(const string16& find_text) {
  view_->SetFindText(find_text);
}

bool FindBarHost::IsFindBarVisible() {
  return host_->IsVisible();
}

void FindBarHost::MoveWindowIfNecessary(const gfx::Rect& selection_rect,
                                       bool no_redraw) {
  // We only move the window if one is active for the current TabContents. If we
  // don't check this, then SetDialogPosition below will end up making the Find
  // Bar visible.
  if (!find_bar_controller_->tab_contents() ||
      !find_bar_controller_->tab_contents()->find_ui_active()) {
    return;
  }

  gfx::Rect new_pos = GetDialogPosition(selection_rect);
  SetDialogPosition(new_pos, no_redraw);

  // May need to redraw our frame to accommodate bookmark bar styles.
  view_->SchedulePaint();
}

bool FindBarHost::IsVisible() {
  return host_->IsVisible();
}

////////////////////////////////////////////////////////////////////////////////
// FindBarHost, views::FocusChangeListener implementation:

void FindBarHost::FocusWillChange(views::View* focused_before,
                                 views::View* focused_now) {
  // First we need to determine if one or both of the views passed in are child
  // views of our view.
  bool our_view_before = focused_before && view_->IsParentOf(focused_before);
  bool our_view_now = focused_now && view_->IsParentOf(focused_now);

  // When both our_view_before and our_view_now are false, it means focus is
  // changing hands elsewhere in the application (and we shouldn't do anything).
  // Similarly, when both are true, focus is changing hands within the Find
  // window (and again, we should not do anything). We therefore only need to
  // look at when we gain initial focus and when we loose it.
  if (!our_view_before && our_view_now) {
    // We are gaining focus from outside the Find window so we must register
    // a handler for Escape.
    RegisterEscAccelerator();
  } else if (our_view_before && !our_view_now) {
    // We are losing focus to something outside our window so we restore the
    // original handler for Escape.
    UnregisterEscAccelerator();
  }
}

////////////////////////////////////////////////////////////////////////////////
// FindBarWin, views::AcceleratorTarget implementation:

bool FindBarHost::AcceleratorPressed(const views::Accelerator& accelerator) {
#if defined(OS_WIN)
  DCHECK(accelerator.GetKeyCode() == VK_ESCAPE);  // We only expect Escape key.
#endif
  // This will end the Find session and hide the window, causing it to loose
  // focus and in the process unregister us as the handler for the Escape
  // accelerator through the FocusWillChange event.
  find_bar_controller_->EndFindSession();

  return true;
}

////////////////////////////////////////////////////////////////////////////////
// FindBarHost, AnimationDelegate implementation:

void FindBarHost::AnimationProgressed(const Animation* animation) {
  // First, we calculate how many pixels to slide the window.
  gfx::Size pref_size = view_->GetPreferredSize();
  find_dialog_animation_offset_ =
      static_cast<int>((1.0 - animation_->GetCurrentValue()) *
      pref_size.height());

  // This call makes sure it appears in the right location, the size and shape
  // is correct and that it slides in the right direction.
  gfx::Rect find_dlg_rect = GetDialogPosition(gfx::Rect());
  SetDialogPosition(find_dlg_rect, false);

  // Let the view know if we are animating, and at which offset to draw the
  // edges.
  view_->animation_offset(find_dialog_animation_offset_);
  view_->SchedulePaint();
}

void FindBarHost::AnimationEnded(const Animation* animation) {
  // Place the find bar in its fully opened state.
  find_dialog_animation_offset_ = 0;

  if (!animation_->IsShowing()) {
    // Animation has finished closing.
    host_->Hide();
  } else {
    // Animation has finished opening.
  }
}

void FindBarHost::GetThemePosition(gfx::Rect* bounds) {
  *bounds = GetDialogPosition(gfx::Rect());
  gfx::Rect toolbar_bounds = browser_view_->GetToolbarBounds();
  gfx::Rect tab_strip_bounds = browser_view_->GetTabStripBounds();
  bounds->Offset(-toolbar_bounds.x(), -tab_strip_bounds.y());
}

////////////////////////////////////////////////////////////////////////////////
// FindBarTesting implementation:

bool FindBarHost::GetFindBarWindowInfo(gfx::Point* position,
                                      bool* fully_visible) {
  if (!find_bar_controller_ ||
#if defined(OS_WIN)
      !::IsWindow(host_->GetNativeView())) {
#else
      false) {
      // TODO(sky): figure out linux side.
#endif
    if (position)
      *position = gfx::Point();
    if (fully_visible)
      *fully_visible = false;
    return false;
  }

  gfx::Rect window_rect;
  host_->GetBounds(&window_rect, true);
  if (position)
    *position = window_rect.origin();
  if (fully_visible)
    *fully_visible = host_->IsVisible() && !IsAnimating();
  return true;
}

void FindBarHost::GetDialogBounds(gfx::Rect* bounds) {
  DCHECK(bounds);
  // The BrowserView does Layout for the components that we care about
  // positioning relative to, so we ask it to tell us where we should go.
  *bounds = browser_view_->GetFindBarBoundingBox();
}

gfx::Rect FindBarHost::GetDialogPosition(gfx::Rect avoid_overlapping_rect) {
  // Find the area we have to work with (after accounting for scrollbars, etc).
  gfx::Rect dialog_bounds;
  GetDialogBounds(&dialog_bounds);
  if (dialog_bounds.IsEmpty())
    return gfx::Rect();

  // Ask the view how large an area it needs to draw on.
  gfx::Size prefsize = view_->GetPreferredSize();

  // Place the view in the top right corner of the dialog boundaries (top left
  // for RTL languages).
  gfx::Rect view_location;
  int x = view_->UILayoutIsRightToLeft() ?
              dialog_bounds.x() : dialog_bounds.width() - prefsize.width();
  int y = dialog_bounds.y();
  view_location.SetRect(x, y, prefsize.width(), prefsize.height());

  // When we get Find results back, we specify a selection rect, which we
  // should strive to avoid overlapping. But first, we need to offset the
  // selection rect (if one was provided).
  if (!avoid_overlapping_rect.IsEmpty()) {
    // For comparison (with the Intersects function below) we need to account
    // for the fact that we draw the Find dialog relative to the window,
    // whereas the selection rect is relative to the page.
    GetDialogPositionNative(&avoid_overlapping_rect);
  }

  gfx::Rect new_pos = FindBarController::GetLocationForFindbarView(
      view_location, dialog_bounds, avoid_overlapping_rect);

  // While we are animating, the Find window will grow bottoms up so we need to
  // re-position the dialog so that it appears to grow out of the toolbar.
  if (find_dialog_animation_offset_ > 0)
    new_pos.Offset(0, std::min(0, -find_dialog_animation_offset_));

  return new_pos;
}

void FindBarHost::SetDialogPosition(const gfx::Rect& new_pos, bool no_redraw) {
  if (new_pos.IsEmpty())
    return;

  // Make sure the window edges are clipped to just the visible region. We need
  // to do this before changing position, so that when we animate the closure
  // of it it doesn't look like the window crumbles into the toolbar.
  UpdateWindowEdges(new_pos);

  SetDialogPositionNative(new_pos, no_redraw);
}

void FindBarHost::RestoreSavedFocus() {
  if (focus_tracker_.get() == NULL) {
    // TODO(brettw) Focus() should be on TabContentsView.
    find_bar_controller_->tab_contents()->Focus();
  } else {
    focus_tracker_->FocusLastFocusedExternalView();
  }
}

FindBarTesting* FindBarHost::GetFindBarTesting() {
  return this;
}

void FindBarHost::UpdateUIForFindResult(const FindNotificationDetails& result,
                                       const string16& find_text) {
  view_->UpdateForResult(result, find_text);

  // We now need to check if the window is obscuring the search results.
  if (!result.selection_rect().IsEmpty())
    MoveWindowIfNecessary(result.selection_rect(), false);

  // Once we find a match we no longer want to keep track of what had
  // focus. EndFindSession will then set the focus to the page content.
  if (result.number_of_matches() > 0)
    focus_tracker_.reset(NULL);
}

void FindBarHost::RegisterEscAccelerator() {
  DCHECK(!esc_accel_target_registered_);
  views::Accelerator escape(base::VKEY_ESCAPE, false, false, false);
  focus_manager_->RegisterAccelerator(escape, this);
  esc_accel_target_registered_ = true;
}

void FindBarHost::UnregisterEscAccelerator() {
  DCHECK(esc_accel_target_registered_);
  views::Accelerator escape(base::VKEY_ESCAPE, false, false, false);
  focus_manager_->UnregisterAccelerator(escape, this);
  esc_accel_target_registered_ = false;
}

bool FindBarHost::MaybeForwardKeystrokeToWebpage(
    const views::Textfield::Keystroke& key_stroke) {
  if (!ShouldForwardKeystrokeToWebpageNative(key_stroke)) {
    // Native implementation says not to forward these events.
    return false;
  }

  switch (key_stroke.GetKeyboardCode()) {
    case base::VKEY_DOWN:
    case base::VKEY_UP:
    case base::VKEY_PRIOR:
    case base::VKEY_NEXT:
      break;
    case base::VKEY_HOME:
    case base::VKEY_END:
      if (key_stroke.IsControlHeld())
        break;
    // Fall through.
    default:
      return false;
  }

  TabContents* contents = find_bar_controller_->tab_contents();
  if (!contents)
    return false;

  RenderViewHost* render_view_host = contents->render_view_host();

  // Make sure we don't have a text field element interfering with keyboard
  // input. Otherwise Up and Down arrow key strokes get eaten. "Nom Nom Nom".
  render_view_host->ClearFocusedNode();
  NativeWebKeyboardEvent event = GetKeyboardEvent(contents, key_stroke);
  render_view_host->ForwardKeyboardEvent(event);
  return true;
}
