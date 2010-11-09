// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/dropdown_bar_host.h"

#include <gdk/gdkkeysyms.h>

#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/views/frame/browser_view.h"
#include "views/widget/widget_gtk.h"
#include "views/controls/textfield/textfield.h"

views::Widget* DropdownBarHost::CreateHost() {
  views::WidgetGtk* host = new views::WidgetGtk(views::WidgetGtk::TYPE_CHILD);
  // We own the host.
  host->set_delete_on_destroy(false);
  return host;
}

void DropdownBarHost::SetWidgetPositionNative(const gfx::Rect& new_pos,
                                              bool no_redraw) {
  host_->SetBounds(new_pos);
  host_->Show();
}

NativeWebKeyboardEvent DropdownBarHost::GetKeyboardEvent(
     const TabContents* contents,
     const views::Textfield::Keystroke& key_stroke) {
  return NativeWebKeyboardEvent(key_stroke.event());
}
