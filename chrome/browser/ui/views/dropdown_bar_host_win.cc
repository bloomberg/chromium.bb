// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/dropdown_bar_host.h"

#include "chrome/browser/ui/find_bar/find_bar_controller.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/tab_contents/tab_contents_view.h"
#include "ui/base/keycodes/keyboard_code_conversion_win.h"
#include "views/controls/scrollbar/native_scroll_bar.h"
#include "views/widget/widget_win.h"

NativeWebKeyboardEvent DropdownBarHost::GetKeyboardEvent(
     const TabContents* contents,
     const views::KeyEvent& key_event) {
  HWND hwnd = contents->GetContentNativeView();
  WORD key = WindowsKeyCodeForKeyboardCode(key_event.key_code());

  return NativeWebKeyboardEvent(hwnd, key_event.native_event().message, key, 0);
}

views::Widget* DropdownBarHost::CreateHost() {
  views::WidgetWin* widget = new views::WidgetWin();
  // Don't let WidgetWin manage our lifetime. We want our lifetime to
  // coincide with TabContents.
  widget->set_delete_on_destroy(false);
  widget->set_window_style(WS_CHILD | WS_CLIPCHILDREN);
  widget->set_window_ex_style(WS_EX_TOPMOST);

  return widget;
}

void DropdownBarHost::SetWidgetPositionNative(const gfx::Rect& new_pos,
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
