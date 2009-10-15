// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_AUTOCOMPLETE_AUTOCOMPLETE_POPUP_GTK_H_
#define CHROME_BROWSER_VIEWS_AUTOCOMPLETE_AUTOCOMPLETE_POPUP_GTK_H_

#include "views/widget/widget_gtk.h"

class AutocompleteEditView;
class AutocompletePopupContentsView;

class AutocompletePopupGtk : public views::WidgetGtk {
 public:
  explicit AutocompletePopupGtk(AutocompletePopupContentsView* contents);
  virtual ~AutocompletePopupGtk();

  // Creates the popup and shows it for the first time. |edit_view| is the edit
  // that created us.
  void Init(AutocompleteEditView* edit_view, views::View* contents);

  // Shows the popup and moves it to the right position.
  void Show();

  // Returns true if the popup is open.
  bool IsOpen() const;

  // Returns true if the popup has been created.
  bool IsCreated() const;

  // Restack the popup window directly above the browser's toplevel window.
  void StackWindow();

 private:
  AutocompletePopupContentsView* contents_;
  AutocompleteEditView* edit_view_;

  DISALLOW_COPY_AND_ASSIGN(AutocompletePopupGtk);
};

#endif  // #ifndef CHROME_BROWSER_VIEWS_AUTOCOMPLETE_AUTOCOMPLETE_POPUP_GTK_H_
