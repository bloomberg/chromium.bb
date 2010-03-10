// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APP_GTK_UTIL_H_
#define APP_GTK_UTIL_H_

typedef struct _GtkWidget GtkWidget;

namespace gtk_util {

// Calculates the size of given widget based on the size specified in
// number of characters/lines (in locale specific resource file) and
// font metrics.
// NOTE: Make sure to realize |widget| before using this method, or a
// default font size will be used instead of the actual font size.
void GetWidgetSizeFromResources(
    GtkWidget* widget, int width_chars, int height_lines,
    int* width, int* height);

// As above, but uses number of characters/lines directly rather than looking
// up a resource.
void GetWidgetSizeFromCharacters(
    GtkWidget* widget, double width_chars, double height_lines,
    int* width, int* height);

// A helper function for gtk_message_dialog_new() to work around a KDE 3
// window manager bugs. You should always call it after creating a dialog
// with gtk_message_dialog_new.
void ApplyMessageDialogQuirks(GtkWidget* dialog);

}  // namespace gtk_util

#endif  // APP_GTK_UTIL_H_
