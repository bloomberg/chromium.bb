// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/dropdown_bar_host.h"

#include <gdk/gdkkeysyms.h>

#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/views/frame/browser_view.h"
#include "views/widget/widget_gtk.h"
#include "views/controls/textfield/textfield.h"

#if defined(TOUCH_UI)
#include "app/keyboard_code_conversion_gtk.h"
#endif

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
#if defined(TOUCH_UI)
  // TODO(oshima): This is a copy from
  // RenderWidgetHostViewViews::OnKeyPressed().
  // Refactor and eliminate the dup code.
  const views::KeyEvent& e = key_stroke.key_event();
  NativeWebKeyboardEvent wke;
  wke.type = WebKit::WebInputEvent::KeyDown;
  wke.windowsKeyCode = e.GetKeyCode();
  wke.setKeyIdentifierFromWindowsKeyCode();

  wke.text[0] = wke.unmodifiedText[0] =
    static_cast<unsigned short>(gdk_keyval_to_unicode(
          app::GdkKeyCodeForWindowsKeyCode(e.GetKeyCode(),
              e.IsShiftDown() ^ e.IsCapsLockDown())));
  return wke;
#else
  return NativeWebKeyboardEvent(key_stroke.event());
#endif
}
