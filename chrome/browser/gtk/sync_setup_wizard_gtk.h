// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_SYNC_SETUP_WIZARD_GTK_H_
#define CHROME_BROWSER_GTK_SYNC_SETUP_WIZARD_GTK_H_

#include <gtk/gtk.h>

#include "base/basictypes.h"
#include "base/string16.h"
#include "googleurl/src/gurl.h"

typedef struct _GtkWidget GtkWidget;
typedef struct _GtkWindow GtkWindow;

class ProfileSyncService;

class SyncSetupWizard;

// This class is used as a temporary solution to allow login to bookmark sync on
// Linux.  It will be replaced with the HtmlDialog based solution when
// window->ShowHtmlDialog() is implemented on Linux.
// See: http://code.google.com/p/chromium/issues/detail?id=25260

class SyncSetupWizardGtk {
 public:
  // Displays the dialog box to setup sync.
  static void Show(ProfileSyncService* service,
                   SyncSetupWizard *wizard);

 private:
  SyncSetupWizardGtk(GtkWindow* parent, ProfileSyncService* service,
                     SyncSetupWizard *wizard);
  ~SyncSetupWizardGtk() { }

  // Handler to respond to Ok and Cancel responses from the dialog.
  static void HandleOnResponseDialog(GtkWidget* widget,
      int response, SyncSetupWizardGtk* setup_wizard) {
    setup_wizard->OnDialogResponse(widget, response);
  }

  void OnDialogResponse(GtkWidget* widget, int response);

  // UI elements.
  GtkWidget* username_textbox_;
  GtkWidget* password_textbox_;

  // We need this to write the sentinel "setup completed" pref.
  ProfileSyncService* service_;
  SyncSetupWizard* wizard_;

  DISALLOW_COPY_AND_ASSIGN(SyncSetupWizardGtk);
};

#endif  // CHROME_BROWSER_GTK_SYNC_SETUP_WIZARD_GTK_H_
