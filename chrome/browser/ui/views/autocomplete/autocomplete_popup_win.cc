// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autocomplete/autocomplete_popup_win.h"

////////////////////////////////////////////////////////////////////////////////
// AutocompletePopupWin, public:

AutocompletePopupWin::AutocompletePopupWin() {
}

AutocompletePopupWin::~AutocompletePopupWin() {
}

gfx::NativeView AutocompletePopupWin::GetRelativeWindowForPopup(
    gfx::NativeView edit_native_view) const {
  // When an IME is attached to the rich-edit control, retrieve its window
  // handle and show this popup window under the IME windows.
  // Otherwise, show this popup window under top-most windows.
  // TODO(hbono): http://b/1111369 if we exclude this popup window from the
  // display area of IME windows, this workaround becomes unnecessary.
  HWND ime_window = ImmGetDefaultIMEWnd(edit_native_view);
  return ime_window ? ime_window : HWND_NOTOPMOST;
}
