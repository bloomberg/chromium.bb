// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/dropdown_bar_host.h"

#include "chrome/browser/ui/find_bar/find_bar_controller.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "ui/base/keycodes/keyboard_code_conversion_win.h"
#include "ui/views/controls/scrollbar/native_scroll_bar.h"

using content::NativeWebKeyboardEvent;
using content::WebContents;

NativeWebKeyboardEvent DropdownBarHost::GetKeyboardEvent(
     const WebContents* contents,
     const views::KeyEvent& key_event) {
  HWND hwnd = contents->GetContentNativeView();
  WORD key = WindowsKeyCodeForKeyboardCode(key_event.key_code());

  MSG msg = { hwnd, key_event.native_event().message, key, 0 };
  return NativeWebKeyboardEvent(msg);
}

void DropdownBarHost::SetWidgetPositionNative(const gfx::Rect& new_pos,
                                              bool no_redraw) {
  DWORD swp_flags = SWP_NOOWNERZORDER;
  if (no_redraw)
    swp_flags |= SWP_NOREDRAW;
  if (!host_->IsVisible())
    swp_flags |= SWP_SHOWWINDOW;

  ::SetWindowPos(host_->GetNativeView(), HWND_TOP, new_pos.x(), new_pos.y(),
                 new_pos.width(), new_pos.height(), swp_flags);
}
