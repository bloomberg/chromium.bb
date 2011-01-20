// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_CLEAR_BROWSING_DATA_DIALOG_GTK_H_
#define CHROME_BROWSER_UI_GTK_CLEAR_BROWSING_DATA_DIALOG_GTK_H_
#pragma once

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "ui/base/gtk/gtk_signal.h"

typedef struct _GtkWidget GtkWidget;
typedef struct _GtkWindow GtkWindow;

class AccessibleWidgetHelper;
class BrowsingDataRemover;
class Profile;

class ClearBrowsingDataDialogGtk {
 public:
  // Displays the dialog box to clear browsing data from |profile|.
  static void Show(GtkWindow* parent, Profile* profile);

 private:
  ClearBrowsingDataDialogGtk(GtkWindow* parent, Profile* profile);
  ~ClearBrowsingDataDialogGtk();

  // Handler to respond to Ok and Cancel responses from the dialog.
  CHROMEGTK_CALLBACK_1(ClearBrowsingDataDialogGtk, void, OnDialogResponse, int);

  // Handler to respond to widget clicked actions from the dialog.
  CHROMEGTK_CALLBACK_0(ClearBrowsingDataDialogGtk, void, OnDialogWidgetClicked);

  CHROMEGTK_CALLBACK_0(ClearBrowsingDataDialogGtk, void, OnFlashLinkClicked);

  // Enable or disable the dialog buttons depending on the state of the
  // checkboxes.
  void UpdateDialogButtons();

  // Create a bitmask from the checkboxes of the dialog.
  int GetCheckedItems();

  // The dialog window.
  GtkWidget* dialog_;

  // UI elements.
  GtkWidget* del_history_checkbox_;
  GtkWidget* del_downloads_checkbox_;
  GtkWidget* del_cache_checkbox_;
  GtkWidget* del_cookies_checkbox_;
  GtkWidget* del_passwords_checkbox_;
  GtkWidget* del_form_data_checkbox_;
  GtkWidget* time_period_combobox_;

  // Our current profile.
  Profile* profile_;

  // If non-null it means removal is in progress. BrowsingDataRemover takes care
  // of deleting itself when done.
  BrowsingDataRemover* remover_;

  // Helper object to manage accessibility metadata.
  scoped_ptr<AccessibleWidgetHelper> accessible_widget_helper_;

  DISALLOW_COPY_AND_ASSIGN(ClearBrowsingDataDialogGtk);
};


#endif  // CHROME_BROWSER_UI_GTK_CLEAR_BROWSING_DATA_DIALOG_GTK_H_
