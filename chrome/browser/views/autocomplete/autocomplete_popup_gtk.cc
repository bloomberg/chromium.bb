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
    AutocompleteEditView* edit_view,
    AutocompletePopupContentsView* contents)
    : WidgetGtk(WidgetGtk::TYPE_POPUP) {
  // Create the popup.  // Owned by |contents|.
  set_delete_on_destroy(false);
  MakeTransparent();
  WidgetGtk::Init(gtk_widget_get_parent(edit_view->GetNativeView()),
                  contents->GetPopupBounds());
  // The contents is owned by the LocationBarView.
  contents->set_parent_owned(false);
  SetContentsView(contents);

  Show();

  // Restack the popup window directly above the browser's toplevel window.
  GtkWidget* toplevel = gtk_widget_get_toplevel(edit_view->GetNativeView());
  DCHECK(GTK_WIDGET_TOPLEVEL(toplevel));
  gtk_util::StackPopupWindow(GetNativeView(), toplevel);
}

AutocompletePopupGtk::~AutocompletePopupGtk() {
}
