// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/find_bar_host.h"

#include "chrome/browser/ui/find_bar/find_bar_controller.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents_view.h"
#include "ui/events/event.h"
#include "ui/views/controls/scrollbar/native_scroll_bar.h"
#include "ui/views/widget/widget.h"

void FindBarHost::AudibleAlert() {
  MessageBeep(MB_OK);
}

bool FindBarHost::ShouldForwardKeyEventToWebpageNative(
    const ui::KeyEvent& key_event) {
  // We specifically ignore WM_CHAR. See http://crbug.com/10509.
  return key_event.native_event().message == WM_KEYDOWN ||
         key_event.native_event().message == WM_KEYUP;
}
