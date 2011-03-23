// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autocomplete/autocomplete_popup_win.h"

#include "chrome/browser/autocomplete/autocomplete_edit_view_win.h"
#include "chrome/browser/autocomplete/autocomplete_popup_model.h"
#include "chrome/browser/ui/views/autocomplete/autocomplete_popup_contents_view.h"
#include "ui/gfx/insets.h"

////////////////////////////////////////////////////////////////////////////////
// AutocompletePopupWin, public:

AutocompletePopupWin::AutocompletePopupWin(
    AutocompleteEditView* edit_view,
    AutocompletePopupContentsView* contents) {
  // Create the popup.
  set_window_style(WS_POPUP | WS_CLIPCHILDREN);
  set_window_ex_style(WS_EX_TOOLWINDOW | WS_EX_LAYERED);
  WidgetWin::Init(GetAncestor(edit_view->GetNativeView(), GA_ROOT),
                  contents->GetPopupBounds());
  // The contents is owned by the LocationBarView.
  contents->set_parent_owned(false);
  SetContentsView(contents);

  // When an IME is attached to the rich-edit control, retrieve its window
  // handle and show this popup window under the IME windows.
  // Otherwise, show this popup window under top-most windows.
  // TODO(hbono): http://b/1111369 if we exclude this popup window from the
  // display area of IME windows, this workaround becomes unnecessary.
  HWND ime_window = ImmGetDefaultIMEWnd(edit_view->GetNativeView());
  SetWindowPos(ime_window ? ime_window : HWND_NOTOPMOST, 0, 0, 0, 0,
               SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

AutocompletePopupWin::~AutocompletePopupWin() {
}

////////////////////////////////////////////////////////////////////////////////
// AutocompletePopupWin, WidgetWin overrides:

LRESULT AutocompletePopupWin::OnMouseActivate(UINT message,
                                              WPARAM w_param,
                                              LPARAM l_param) {
  return MA_NOACTIVATE;
}
