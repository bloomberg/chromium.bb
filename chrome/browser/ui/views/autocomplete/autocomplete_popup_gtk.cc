// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autocomplete/autocomplete_popup_gtk.h"

#include "chrome/browser/autocomplete/autocomplete_edit_view.h"
#include "chrome/browser/autocomplete/autocomplete_popup_model.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/browser/ui/views/autocomplete/autocomplete_popup_contents_view.h"
#include "ui/gfx/insets.h"

////////////////////////////////////////////////////////////////////////////////
// AutocompletePopupGtk, public:

AutocompletePopupGtk::AutocompletePopupGtk(
    AutocompleteEditView* edit_view,
    AutocompletePopupContentsView* contents)
    : WidgetGtk(WidgetGtk::TYPE_POPUP) {
  // Create the popup.
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
