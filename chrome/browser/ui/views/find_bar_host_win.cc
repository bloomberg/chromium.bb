// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/find_bar_host.h"

#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_view.h"
#include "chrome/browser/ui/find_bar/find_bar_controller.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "views/controls/scrollbar/native_scroll_bar.h"
#include "views/widget/widget_win.h"

void FindBarHost::AudibleAlert() {
  MessageBeep(MB_OK);
}

void FindBarHost::GetWidgetPositionNative(gfx::Rect* avoid_overlapping_rect) {
  RECT frame_rect = {0}, webcontents_rect = {0};
  ::GetWindowRect(
      static_cast<views::WidgetWin*>(host())->GetParent(), &frame_rect);
  ::GetWindowRect(
      find_bar_controller_->
          tab_contents()->tab_contents()->view()->GetNativeView(),
      &webcontents_rect);
  avoid_overlapping_rect->Offset(0, webcontents_rect.top - frame_rect.top);
}

bool FindBarHost::ShouldForwardKeyEventToWebpageNative(
    const views::KeyEvent& key_event) {
  // We specifically ignore WM_CHAR. See http://crbug.com/10509.
  return key_event.native_event().message == WM_KEYDOWN ||
         key_event.native_event().message == WM_KEYUP;
}
