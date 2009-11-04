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
