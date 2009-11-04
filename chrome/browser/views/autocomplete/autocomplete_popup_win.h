// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_AUTOCOMPLETE_AUTOCOMPLETE_POPUP_WIN_H_
#define CHROME_BROWSER_VIEWS_AUTOCOMPLETE_AUTOCOMPLETE_POPUP_WIN_H_

#include "views/widget/widget_win.h"

class AutocompleteEditView;
class AutocompletePopupContentsView;

class AutocompletePopupWin : public views::WidgetWin {
 public:
  explicit AutocompletePopupWin(AutocompletePopupContentsView* contents);
  virtual ~AutocompletePopupWin();

  // Overridden from WidgetWin:
  virtual void Show();
  virtual void Hide();

  // Creates the popup and shows it for the first time. |edit_view| is the edit
  // that created us.
  void Init(AutocompleteEditView* edit_view, views::View* contents);

  // Returns true if the popup is open.
  bool IsOpen() const;

  // Returns true if the popup has been created.
  bool IsCreated() const;

 protected:
  // Overridden from WidgetWin:
  virtual LRESULT OnMouseActivate(HWND window,
                                  UINT hit_test,
                                  UINT mouse_message);

 private:
  AutocompletePopupContentsView* contents_;

  bool is_open_;  // Used only for sanity-checking.

  DISALLOW_COPY_AND_ASSIGN(AutocompletePopupWin);
};

#endif  // #ifndef CHROME_BROWSER_VIEWS_AUTOCOMPLETE_AUTOCOMPLETE_POPUP_WIN_H_
