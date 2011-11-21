// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/dropdown_bar_host.h"

#include <gdk/gdkkeysyms.h>

#include "chrome/browser/ui/views/frame/browser_view.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "ui/views/widget/widget.h"
#include "views/controls/textfield/textfield.h"

#if defined(TOUCH_UI)
#include "ui/base/keycodes/keyboard_code_conversion_gtk.h"
#endif

void DropdownBarHost::SetWidgetPositionNative(const gfx::Rect& new_pos,
                                              bool no_redraw) {
  host_->SetBounds(new_pos);
  host_->Show();
}

NativeWebKeyboardEvent DropdownBarHost::GetKeyboardEvent(
     const TabContents* contents,
     const views::KeyEvent& key_event) {
#if defined(TOUCH_UI)
  // TODO(oshima): This is a copy from
  // RenderWidgetHostViewViews::OnKeyPressed().
  // Refactor and eliminate the dup code.
  NativeWebKeyboardEvent wke;
  wke.type = WebKit::WebInputEvent::KeyDown;
  wke.windowsKeyCode = key_event.key_code();
  wke.setKeyIdentifierFromWindowsKeyCode();

  wke.text[0] = wke.unmodifiedText[0] =
    static_cast<unsigned short>(gdk_keyval_to_unicode(
          ui::GdkKeyCodeForWindowsKeyCode(key_event.key_code(),
              key_event.IsShiftDown() ^ key_event.IsCapsLockDown())));

  // Due to a bug in GDK, gdk_keyval_to_unicode(keyval) returns 0 if keyval
  // is GDK_Return. It should instead return '\r'. This is causing
  // http://code.google.com/p/chromium/issues/detail?id=75779
  // Hence, the ugly hack below.
  // TODO(varunjain): remove the hack when the GDK bug
  // https://bugzilla.gnome.org/show_bug.cgi?id=644836 gets sorted out.
  if (key_event.key_code() == ui::VKEY_RETURN) {
    wke.text[0] = wke.unmodifiedText[0] = '\r';
  }

  return wke;
#else
  return NativeWebKeyboardEvent(key_event.gdk_event());
#endif
}
