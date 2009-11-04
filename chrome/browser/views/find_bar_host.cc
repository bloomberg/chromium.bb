// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/find_bar_host.h"

#include "app/gfx/path.h"
#include "app/slide_animation.h"
#include "base/keyboard_codes.h"
#include "base/scoped_handle.h"
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

using gfx::Path;

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


animation_->SetSlideDuration(5000);
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

void FindBarHost::UpdateWindowEdges(const gfx::Rect& new_pos) {
  // |w| is used to make it easier to create the part of the polygon that curves
  // the right side of the Find window. It essentially keeps track of the
  // x-pixel position of the right-most background image inside the view.
  // TODO(finnur): Let the view tell us how to draw the curves or convert
  // this to a CustomFrameWindow.
  int w = new_pos.width() - 6;  // -6 positions us at the left edge of the
                                // rightmost background image of the view.

  // This polygon array represents the outline of the background image for the
  // dialog. Basically, it encompasses only the visible pixels of the
  // concatenated find_dlg_LMR_bg images (where LMR = [left | middle | right]).
  static const Path::Point polygon[] = {
      {0, 0}, {0, 1}, {2, 3}, {2, 29}, {4, 31},
        {4, 32}, {w+0, 32},
      {w+0, 31}, {w+1, 31}, {w+3, 29}, {w+3, 3}, {w+6, 0}
  };

  // Find the largest x and y value in the polygon.
  int max_x = 0, max_y = 0;
  for (size_t i = 0; i < arraysize(polygon); i++) {
    max_x = std::max(max_x, static_cast<int>(polygon[i].x));
    max_y = std::max(max_y, static_cast<int>(polygon[i].y));
  }

  // We then create the polygon and use SetWindowRgn to force the window to draw
  // only within that area. This region may get reduced in size below.
  Path path(polygon, arraysize(polygon));
  ScopedRegion region(path.CreateNativeRegion());

  // Are we animating?
  if (find_dialog_animation_offset_ > 0) {
    // The animation happens in two steps: First, we clip the window and then in
    // GetDialogPosition we offset the window position so that it still looks
    // attached to the toolbar as it grows. We clip the window by creating a
    // rectangle region (that gradually increases as the animation progresses)
    // and find the intersection between the two regions using CombineRgn.

    // |y| shrinks as the animation progresses from the height of the view down
    // to 0 (and reverses when closing).
    int y = find_dialog_animation_offset_;
    // |y| shrinking means the animation (visible) region gets larger. In other
    // words: the rectangle grows upward (when the dialog is opening).
    Path animation_path;
    SkRect animation_rect = { SkIntToScalar(0), SkIntToScalar(y),
                              SkIntToScalar(max_x), SkIntToScalar(max_y) };
    animation_path.addRect(animation_rect);
    ScopedRegion animation_region(animation_path.CreateNativeRegion());
    region.Set(Path::IntersectRegions(animation_region.Get(), region.Get()));

    // Next, we need to increase the region a little bit to account for the
    // curved edges that the view will draw to make it look like grows out of
    // the toolbar.
    Path::Point left_curve[] = {
      {0, y+0}, {0, y+1}, {2, y+3}, {2, y+0}, {0, y+0}
    };
    Path::Point right_curve[] = {
      {w+3, y+3}, {w+6, y+0}, {w+3, y+0}, {w+3, y+3}
    };

    // Combine the region for the curve on the left with our main region.
    Path left_path(left_curve, arraysize(left_curve));
    ScopedRegion r(left_path.CreateNativeRegion());
    region.Set(Path::CombineRegions(r.Get(), region.Get()));

    // Combine the region for the curve on the right with our main region.
    Path right_path(right_curve, arraysize(right_curve));
    region.Set(Path::CombineRegions(r.Get(), region.Get()));
  }

  // Now see if we need to truncate the region because parts of it obscures
  // the main window border.
  gfx::Rect dialog_bounds;
  GetDialogBounds(&dialog_bounds);

  // Calculate how much our current position overlaps our boundaries. If we
  // overlap, it means we have too little space to draw the whole dialog and
  // we allow overwriting the scrollbar before we start truncating our dialog.
  //
  // TODO(brettw) this constant is evil. This is the amount of room we've added
  // to the window size, when we set the region, it can change the size.
  static const int kAddedWidth = 7;
  int difference = (new_pos.right() - kAddedWidth) -
                   dialog_bounds.width() -
                   views::NativeScrollBar::GetVerticalScrollBarWidth() +
                   1;
  if (difference > 0) {
    Path::Point exclude[4];
    exclude[0].x = max_x - difference;  // Top left corner.
    exclude[0].y = 0;

    exclude[1].x = max_x;               // Top right corner.
    exclude[1].y = 0;

    exclude[2].x = max_x;               // Bottom right corner.
    exclude[2].y = max_y;

    exclude[3].x = max_x - difference;  // Bottom left corner.
    exclude[3].y = max_y;

    // Subtract this region from the original region.
    gfx::Path exclude_path(exclude, arraysize(exclude));
    ScopedRegion exclude_region(exclude_path.CreateNativeRegion());
    region.Set(Path::SubtractRegion(region.Get(), exclude_region.Get()));
  }

  // Window takes ownership of the region.
  host_->SetShape(region.release());
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
