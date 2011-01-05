// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/find_bar_host.h"

#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/ui/find_bar/find_bar_controller.h"
#include "chrome/browser/views/frame/browser_view.h"
#include "chrome/browser/views/tab_contents/tab_contents_view_gtk.h"
#include "views/widget/widget_gtk.h"

void FindBarHost::AudibleAlert() {
  // TODO(davemoore) implement.
  NOTIMPLEMENTED();
}

void FindBarHost::GetWidgetPositionNative(gfx::Rect* avoid_overlapping_rect) {
  gfx::Rect frame_rect, webcontents_rect;
  host()->GetRootWidget()->GetBounds(&frame_rect, true);
  TabContentsView* tab_view = find_bar_controller_->tab_contents()->view();
  static_cast<TabContentsViewGtk*>(tab_view)->GetBounds(&webcontents_rect,
                                                        true);
  avoid_overlapping_rect->Offset(0, webcontents_rect.y() - frame_rect.y());
}

bool FindBarHost::ShouldForwardKeyEventToWebpageNative(
    const views::KeyEvent& key_event) {
  return true;
}
