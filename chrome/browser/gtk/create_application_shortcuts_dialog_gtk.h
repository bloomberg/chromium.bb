// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_CREATE_APPLICATION_SHORTCUTS_DIALOG_GTK_H_
#define CHROME_BROWSER_GTK_CREATE_APPLICATION_SHORTCUTS_DIALOG_GTK_H_

#include "base/basictypes.h"
#include "base/string16.h"
#include "googleurl/src/gurl.h"

typedef struct _GtkWidget GtkWidget;
typedef struct _GtkWindow GtkWindow;

class CreateApplicationShortcutsDialogGtk {
 public:
  // Displays the dialog box to create application shortcuts for |url| with
  // |title|.
  static void Show(GtkWindow* parent, const GURL& url, const string16& title);

 private:
  CreateApplicationShortcutsDialogGtk(GtkWindow* parent, const GURL& url,
                                      const string16& title);
  ~CreateApplicationShortcutsDialogGtk() { }

  // Handler to respond to Ok and Cancel responses from the dialog.
  static void HandleOnResponseDialog(GtkWidget* widget,
      int response, CreateApplicationShortcutsDialogGtk* user_data) {
    user_data->OnDialogResponse(widget, response);
  }

  void OnDialogResponse(GtkWidget* widget, int response);

  // UI elements.
  GtkWidget* desktop_checkbox_;
  GtkWidget* menu_checkbox_;

  // Target URL of the shortcut.
  GURL url_;

  // Visible title of the shortcut.
  string16 title_;

  DISALLOW_COPY_AND_ASSIGN(CreateApplicationShortcutsDialogGtk);
};

#endif  // CHROME_BROWSER_GTK_CREATE_APPLICATION_SHORTCUTS_DIALOG_GTK_H_
