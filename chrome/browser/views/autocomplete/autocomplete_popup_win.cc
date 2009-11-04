// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "chrome/browser/views/autocomplete/autocomplete_popup_win.h"

#include "app/gfx/insets.h"
#include "app/win_util.h"
#include "chrome/browser/autocomplete/autocomplete_edit_view_win.h"
#include "chrome/browser/autocomplete/autocomplete_popup_model.h"
#include "chrome/browser/views/autocomplete/autocomplete_popup_contents_view.h"

////////////////////////////////////////////////////////////////////////////////
// AutocompletePopupWin, public:

AutocompletePopupWin::AutocompletePopupWin(
    AutocompletePopupContentsView* contents)
    : contents_(contents),
      is_open_(false) {
  set_delete_on_destroy(false);
  set_window_style(WS_POPUP | WS_CLIPCHILDREN);
  set_window_ex_style(WS_EX_TOOLWINDOW | WS_EX_LAYERED);
}

AutocompletePopupWin::~AutocompletePopupWin() {
}

void AutocompletePopupWin::Show() {
  // Move the popup to the place appropriate for the window's current position -
  // it may have been moved since it was last shown.
  SetBounds(contents_->GetPopupBounds());
  WidgetWin::Show();
  is_open_ = true;
}

void AutocompletePopupWin::Hide() {
  WidgetWin::Hide();
  is_open_ = false;
}

void AutocompletePopupWin::Init(AutocompleteEditView* edit_view,
                                views::View* contents) {
  // Create the popup
  WidgetWin::Init(GetAncestor(edit_view->GetNativeView(), GA_ROOT),
                  contents_->GetPopupBounds());
  // The contents is owned by the LocationBarView.
  contents_->SetParentOwned(false);
  SetContentsView(contents_);

  // When an IME is attached to the rich-edit control, retrieve its window
  // handle and show this popup window under the IME windows.
  // Otherwise, show this popup window under top-most windows.
  // TODO(hbono): http://b/1111369 if we exclude this popup window from the
  // display area of IME windows, this workaround becomes unnecessary.
  HWND ime_window = ImmGetDefaultIMEWnd(edit_view->GetNativeView());
  SetWindowPos(ime_window ? ime_window : HWND_NOTOPMOST, 0, 0, 0, 0,
               SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
  is_open_ = true;
}

bool AutocompletePopupWin::IsOpen() const {
  const bool is_open = IsCreated() && IsVisible();
  CHECK(is_open == is_open_);
  return is_open;
}

bool AutocompletePopupWin::IsCreated() const {
  return !!IsWindow();
}

////////////////////////////////////////////////////////////////////////////////
// AutocompletePopupWin, WidgetWin overrides:

LRESULT AutocompletePopupWin::OnMouseActivate(HWND window, UINT hit_test,
                                              UINT mouse_message) {
  return MA_NOACTIVATE;
}
