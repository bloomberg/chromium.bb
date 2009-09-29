// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/find_bar_host.h"

#include "chrome/browser/find_bar_controller.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_view.h"
#include "chrome/browser/views/frame/browser_view.h"
#include "views/controls/scrollbar/native_scroll_bar.h"
#include "views/widget/widget_win.h"

// TODO(brettw) this should not be so complicated. The view should really be in
// charge of these regions. CustomFrameWindow will do this for us. It will also
// let us set a path for the window region which will avoid some logic here.
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
  static const POINT polygon[] = {
      {0, 0}, {0, 1}, {2, 3}, {2, 29}, {4, 31},
        {4, 32}, {w+0, 32},
      {w+0, 31}, {w+1, 31}, {w+3, 29}, {w+3, 3}, {w+6, 0}
  };

  // Find the largest x and y value in the polygon.
  int max_x = 0, max_y = 0;
  for (int i = 0; i < arraysize(polygon); i++) {
    max_x = std::max(max_x, static_cast<int>(polygon[i].x));
    max_y = std::max(max_y, static_cast<int>(polygon[i].y));
  }

  // We then create the polygon and use SetWindowRgn to force the window to draw
  // only within that area. This region may get reduced in size below.
  HRGN region = CreatePolygonRgn(polygon, arraysize(polygon), ALTERNATE);

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
    HRGN animation_region = CreateRectRgn(0, y, max_x, max_y);
    // |region| will contain the intersected parts after calling this function:
    CombineRgn(region, animation_region, region, RGN_AND);
    DeleteObject(animation_region);

    // Next, we need to increase the region a little bit to account for the
    // curved edges that the view will draw to make it look like grows out of
    // the toolbar.
    POINT left_curve[] = {
      {0, y+0}, {0, y+1}, {2, y+3}, {2, y+0}, {0, y+0}
    };
    POINT right_curve[] = {
      {w+3, y+3}, {w+6, y+0}, {w+3, y+0}, {w+3, y+3}
    };

    // Combine the region for the curve on the left with our main region.
    HRGN r = CreatePolygonRgn(left_curve, arraysize(left_curve), ALTERNATE);
    CombineRgn(region, r, region, RGN_OR);
    DeleteObject(r);

    // Combine the region for the curve on the right with our main region.
    r = CreatePolygonRgn(right_curve, arraysize(right_curve), ALTERNATE);
    CombineRgn(region, r, region, RGN_OR);
    DeleteObject(r);
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
    POINT exclude[4] = {0};
    exclude[0].x = max_x - difference;  // Top left corner.
    exclude[0].y = 0;

    exclude[1].x = max_x;               // Top right corner.
    exclude[1].y = 0;

    exclude[2].x = max_x;               // Bottom right corner.
    exclude[2].y = max_y;

    exclude[3].x = max_x - difference;  // Bottom left corner.
    exclude[3].y = max_y;

    // Subtract this region from the original region.
    HRGN exclude_rgn = CreatePolygonRgn(exclude, arraysize(exclude), ALTERNATE);
    int result = CombineRgn(region, region, exclude_rgn, RGN_DIFF);
    DeleteObject(exclude_rgn);
  }

  // The system now owns the region, so we do not delete it.
  ::SetWindowRgn(host_->GetNativeView(), region, TRUE);  // TRUE = Redraw.
}

NativeWebKeyboardEvent FindBarHost::GetKeyboardEvent(
     const TabContents* contents,
     const views::Textfield::Keystroke& key_stroke) {
  HWND hwnd = contents->GetContentNativeView();
  return NativeWebKeyboardEvent(
      hwnd, key_stroke.message(), key_stroke.key(), 0);
}

void FindBarHost::AudibleAlert() {
  MessageBeep(MB_OK);
}

views::Widget* FindBarHost::CreateHost() {
  views::WidgetWin* widget = new views::WidgetWin();
  // Don't let WidgetWin manage our lifetime. We want our lifetime to
  // coincide with TabContents.
  widget->set_delete_on_destroy(false);
  widget->set_window_style(WS_CHILD | WS_CLIPCHILDREN);
  widget->set_window_ex_style(WS_EX_TOPMOST);

  return widget;
}

void FindBarHost::SetDialogPositionNative(const gfx::Rect& new_pos,
                                          bool no_redraw) {
  gfx::Rect window_rect;
  host_->GetBounds(&window_rect, true);
  DWORD swp_flags = SWP_NOOWNERZORDER;
  if (!window_rect.IsEmpty())
    swp_flags |= SWP_NOSIZE;
  if (no_redraw)
    swp_flags |= SWP_NOREDRAW;
  if (!host_->IsVisible())
    swp_flags |= SWP_SHOWWINDOW;

  ::SetWindowPos(host_->GetNativeView(), HWND_TOP, new_pos.x(), new_pos.y(),
                 new_pos.width(), new_pos.height(), swp_flags);
}

void FindBarHost::GetDialogPositionNative(gfx::Rect* avoid_overlapping_rect) {
  RECT frame_rect = {0}, webcontents_rect = {0};
  ::GetWindowRect(
      static_cast<views::WidgetWin*>(host_.get())->GetParent(), &frame_rect);
  ::GetWindowRect(
      find_bar_controller_->tab_contents()->view()->GetNativeView(),
      &webcontents_rect);
  avoid_overlapping_rect->Offset(0, webcontents_rect.top - frame_rect.top);
}

gfx::NativeView FindBarHost::GetNativeView(BrowserView* browser_view) {
  return browser_view->GetWidget()->GetNativeView();
}

bool FindBarHost::ShouldForwardKeystrokeToWebpageNative(
    const views::Textfield::Keystroke& key_stroke) {
  // We specifically ignore WM_CHAR. See http://crbug.com/10509.
  return key_stroke.message() == WM_KEYDOWN || key_stroke.message() == WM_KEYUP;
}
