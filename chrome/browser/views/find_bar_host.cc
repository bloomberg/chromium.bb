// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/find_bar_host.h"

#include "app/gfx/path.h"
#include "app/gfx/scrollbar_size.h"
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
#include "views/widget/root_view.h"
#include "views/widget/widget.h"

#if defined(OS_LINUX)
#include "app/scoped_handle_gtk.h"
#endif

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
    : DropdownBarHost(browser_view),
      find_bar_controller_(NULL) {
  Init(new FindBarView(this));
}

FindBarHost::~FindBarHost() {
}

void FindBarHost::Show() {
  DropdownBarHost::Show(true);
}

void FindBarHost::SetFocusAndSelection() {
  DropdownBarHost::SetFocusAndSelection();
}

void FindBarHost::Hide(bool animate) {
  DropdownBarHost::Hide(animate);
}

void FindBarHost::ClearResults(const FindNotificationDetails& results) {
  find_bar_view()->UpdateForResult(results, string16());
}

void FindBarHost::StopAnimation() {
  DropdownBarHost::StopAnimation();
}

void FindBarHost::SetFindText(const string16& find_text) {
  find_bar_view()->SetFindText(find_text);
}

bool FindBarHost::IsFindBarVisible() {
  return DropdownBarHost::IsVisible();
}

void FindBarHost::MoveWindowIfNecessary(const gfx::Rect& selection_rect,
                                        bool no_redraw) {
  // We only move the window if one is active for the current TabContents. If we
  // don't check this, then SetWidgetPosition below will end up making the Find
  // Bar visible.
  if (!find_bar_controller_->tab_contents() ||
      !find_bar_controller_->tab_contents()->find_ui_active()) {
    return;
  }

  gfx::Rect new_pos = GetDialogPosition(selection_rect);
  SetDialogPosition(new_pos, no_redraw);

  // May need to redraw our frame to accommodate bookmark bar styles.
  view()->SchedulePaint();
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
    *fully_visible = host()->IsVisible() && !IsAnimating();
  return true;
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
  // window. Basically, it encompasses only the visible pixels of the
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
  if (animation_offset() > 0) {
    // The animation happens in two steps: First, we clip the window and then in
    // GetWidgetPosition we offset the window position so that it still looks
    // attached to the toolbar as it grows. We clip the window by creating a
    // rectangle region (that gradually increases as the animation progresses)
    // and find the intersection between the two regions using CombineRgn.

    // |y| shrinks as the animation progresses from the height of the view down
    // to 0 (and reverses when closing).
    int y = animation_offset();
    // |y| shrinking means the animation (visible) region gets larger. In other
    // words: the rectangle grows upward (when the widget is opening).
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
  gfx::Rect widget_bounds;
  GetWidgetBounds(&widget_bounds);

  // Calculate how much our current position overlaps our boundaries. If we
  // overlap, it means we have too little space to draw the whole widget and
  // we allow overwriting the scrollbar before we start truncating our widget.
  //
  // TODO(brettw) this constant is evil. This is the amount of room we've added
  // to the window size, when we set the region, it can change the size.
  static const int kAddedWidth = 7;
  int difference = new_pos.right() - kAddedWidth - widget_bounds.width() -
      gfx::scrollbar_size() + 1;
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
  host()->SetShape(region.release());
}

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
  int x = view()->UILayoutIsRightToLeft() ?
              widget_bounds.x() : widget_bounds.width() - prefsize.width();
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

void FindBarHost::RestoreSavedFocus() {
  if (focus_tracker() == NULL) {
    // TODO(brettw) Focus() should be on TabContentsView.
    find_bar_controller_->tab_contents()->Focus();
  } else {
    focus_tracker()->FocusLastFocusedExternalView();
  }
}

FindBarTesting* FindBarHost::GetFindBarTesting() {
  return this;
}

void FindBarHost::UpdateUIForFindResult(const FindNotificationDetails& result,
                                       const string16& find_text) {
  find_bar_view()->UpdateForResult(result, find_text);

  // We now need to check if the window is obscuring the search results.
  if (!result.selection_rect().IsEmpty())
    MoveWindowIfNecessary(result.selection_rect(), false);

  // Once we find a match we no longer want to keep track of what had
  // focus. EndFindSession will then set the focus to the page content.
  if (result.number_of_matches() > 0)
    ResetFocusTracker();
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

FindBarView* FindBarHost::find_bar_view() {
  return static_cast<FindBarView*>(view());
}
