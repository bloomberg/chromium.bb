// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOCOMPLETE_AUTOCOMPLETE_POPUP_WIN_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOCOMPLETE_AUTOCOMPLETE_POPUP_WIN_H_
#pragma once

#include "base/weak_ptr.h"
#include "views/widget/widget_win.h"

class AutocompleteEditView;
class AutocompletePopupContentsView;

class AutocompletePopupWin
    : public views::WidgetWin,
      public base::SupportsWeakPtr<AutocompletePopupWin> {
 public:
  // Creates the popup and shows it. |edit_view| is the edit that created us.
  AutocompletePopupWin(AutocompleteEditView* edit_view,
                       AutocompletePopupContentsView* contents);
  virtual ~AutocompletePopupWin();

 private:
  // Overridden from WidgetWin:
  virtual LRESULT OnMouseActivate(UINT message,
                                  WPARAM w_param,
                                  LPARAM l_param) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(AutocompletePopupWin);
};

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOCOMPLETE_AUTOCOMPLETE_POPUP_WIN_H_
