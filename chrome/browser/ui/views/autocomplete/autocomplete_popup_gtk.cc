// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autocomplete/autocomplete_popup_gtk.h"

#include "chrome/browser/ui/gtk/gtk_util.h"

////////////////////////////////////////////////////////////////////////////////
// AutocompletePopupGtk, public:

AutocompletePopupGtk::AutocompletePopupGtk()
    : WidgetGtk(WidgetGtk::TYPE_POPUP) {
}

AutocompletePopupGtk::~AutocompletePopupGtk() {
}

gfx::NativeView AutocompletePopupGtk::GetRelativeWindowForPopup(
    gfx::NativeView edit_native_view) const {
  GtkWidget* toplevel = gtk_widget_get_toplevel(edit_native_view);
  DCHECK(GTK_WIDGET_TOPLEVEL(toplevel));
  return toplevel;
}
