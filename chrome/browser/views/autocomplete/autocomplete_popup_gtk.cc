// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "chrome/browser/views/autocomplete/autocomplete_popup_gtk.h"

#include "app/gfx/insets.h"
#include "chrome/browser/autocomplete/autocomplete_edit_view.h"
#include "chrome/browser/autocomplete/autocomplete_popup_model.h"
#include "chrome/browser/views/autocomplete/autocomplete_popup_contents_view.h"
#include "chrome/common/gtk_util.h"

////////////////////////////////////////////////////////////////////////////////
// AutocompletePopupGtk, public:

AutocompletePopupGtk::AutocompletePopupGtk(
    AutocompletePopupContentsView* contents)
    : WidgetGtk(WidgetGtk::TYPE_POPUP),
      contents_(contents),
      edit_view_(NULL) {
  set_delete_on_destroy(false);
}

AutocompletePopupGtk::~AutocompletePopupGtk() {
}

void AutocompletePopupGtk::Init(AutocompleteEditView* edit_view,
                                views::View* contents) {
  MakeTransparent();
  // Create the popup
  WidgetGtk::Init(gtk_widget_get_parent(edit_view->GetNativeView()),
                  contents_->GetPopupBounds());
  // The contents is owned by the LocationBarView.
  contents_->SetParentOwned(false);
  SetContentsView(contents_);

  edit_view_ = edit_view;
}

void AutocompletePopupGtk::Show() {
  // Move the popup to the place appropriate for the window's current position -
  // it may have been moved since it was last shown.
  SetBounds(contents_->GetPopupBounds());
  if (!IsVisible()) {
    WidgetGtk::Show();
    StackWindow();
  }
}

bool AutocompletePopupGtk::IsOpen() const {
  return IsCreated() && IsVisible();
}

bool AutocompletePopupGtk::IsCreated() const {
  return GTK_IS_WIDGET(GetNativeView());
}

void AutocompletePopupGtk::StackWindow() {
  GtkWidget* toplevel = gtk_widget_get_toplevel(edit_view_->GetNativeView());
  DCHECK(GTK_WIDGET_TOPLEVEL(toplevel));
  gtk_util::StackPopupWindow(GetNativeView(), toplevel);
}
