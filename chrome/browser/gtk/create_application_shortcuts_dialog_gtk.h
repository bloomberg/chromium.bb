// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_CREATE_APPLICATION_SHORTCUTS_DIALOG_GTK_H_
#define CHROME_BROWSER_GTK_CREATE_APPLICATION_SHORTCUTS_DIALOG_GTK_H_

#include "app/gtk_signal.h"
#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "base/string16.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/shell_integration.h"
#include "googleurl/src/gurl.h"

typedef struct _GdkPixbuf GdkPixbuf;
typedef struct _GtkWidget GtkWidget;
typedef struct _GtkWindow GtkWindow;

class TabContents;

class CreateApplicationShortcutsDialogGtk
    : public base::RefCountedThreadSafe<CreateApplicationShortcutsDialogGtk,
                                        ChromeThread::DeleteOnUIThread> {
 public:
  // Displays the dialog box to create application shortcuts for |tab_contents|.
  static void Show(GtkWindow* parent, TabContents* tab_contents);

 private:
  friend class ChromeThread;
  friend class DeleteTask<CreateApplicationShortcutsDialogGtk>;

  CreateApplicationShortcutsDialogGtk(GtkWindow* parent,
                                      TabContents* tab_contents);
  ~CreateApplicationShortcutsDialogGtk();

  CHROMEGTK_CALLBACK_1(CreateApplicationShortcutsDialogGtk, void,
                       OnCreateDialogResponse, int);

  CHROMEGTK_CALLBACK_1(CreateApplicationShortcutsDialogGtk, void,
                       OnErrorDialogResponse, int);

  CHROMEGTK_CALLBACK_0(CreateApplicationShortcutsDialogGtk, void,
                       OnToggleCheckbox);

  void CreateDesktopShortcut(
      const ShellIntegration::ShortcutInfo& shortcut_info);
  void ShowErrorDialog();

  // UI elements.
  GtkWidget* desktop_checkbox_;
  GtkWidget* menu_checkbox_;

  // TabContents for which the shortcut will be created.
  TabContents* tab_contents_;

  // ShortcutInfo for the new shortcut.
  ShellIntegration::ShortcutInfo shortcut_info_;

  // Image associated with the site.
  GdkPixbuf* favicon_pixbuf_;

  // Dialog box that allows the user to create an application shortcut.
  GtkWidget* create_dialog_;

  // Dialog box that shows the error message.
  GtkWidget* error_dialog_;

  DISALLOW_COPY_AND_ASSIGN(CreateApplicationShortcutsDialogGtk);
};

#endif  // CHROME_BROWSER_GTK_CREATE_APPLICATION_SHORTCUTS_DIALOG_GTK_H_
