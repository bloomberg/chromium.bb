// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_CREATE_APPLICATION_SHORTCUTS_DIALOG_GTK_H_
#define CHROME_BROWSER_GTK_CREATE_APPLICATION_SHORTCUTS_DIALOG_GTK_H_

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "base/string16.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/shell_integration.h"
#include "googleurl/src/gurl.h"
#include "third_party/skia/include/core/SkBitmap.h"

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

  static void HandleOnResponseCreateDialog(GtkWidget* widget,
      int response, CreateApplicationShortcutsDialogGtk* user_data) {
    user_data->OnCreateDialogResponse(widget, response);
  }
  static void HandleOnResponseErrorDialog(GtkWidget* widget,
      int response, CreateApplicationShortcutsDialogGtk* user_data) {
    user_data->OnErrorDialogResponse(widget, response);
  }

  void OnCreateDialogResponse(GtkWidget* widget, int response);
  void OnErrorDialogResponse(GtkWidget* widget, int response);

  void CreateDesktopShortcut(
      const ShellIntegration::ShortcutInfo& shortcut_info);
  void ShowErrorDialog();

  // UI elements.
  GtkWidget* desktop_checkbox_;
  GtkWidget* menu_checkbox_;

  // TabContents for which the shortcut will be created.
  TabContents* tab_contents_;

  // Target URL of the shortcut.
  GURL url_;

  // Visible title of the shortcut.
  string16 title_;

  // The favicon of the tab contents, used to set the icon on the desktop.
  SkBitmap favicon_;

  // Dialog box that allows the user to create an application shortcut.
  GtkWidget* create_dialog_;

  // Dialog box that shows the error message.
  GtkWidget* error_dialog_;

  DISALLOW_COPY_AND_ASSIGN(CreateApplicationShortcutsDialogGtk);
};

#endif  // CHROME_BROWSER_GTK_CREATE_APPLICATION_SHORTCUTS_DIALOG_GTK_H_
