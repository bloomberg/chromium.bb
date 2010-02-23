// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_CLEAR_BROWSING_DATA_DIALOG_GTK_H_
#define CHROME_BROWSER_GTK_CLEAR_BROWSING_DATA_DIALOG_GTK_H_

#include "base/basictypes.h"
#include "base/scoped_ptr.h"

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
  static void HandleOnResponseDialog(GtkWidget* widget,
                                     int response,
                                     ClearBrowsingDataDialogGtk* user_data) {
    user_data->OnDialogResponse(widget, response);
  }

  // Handler to respond to widget clicked actions from the dialog.
  static void HandleOnClickedWidget(GtkWidget* widget,
                                    ClearBrowsingDataDialogGtk* user_data) {
    user_data->OnDialogWidgetClicked(widget);
  }
  void OnDialogResponse(GtkWidget* widget, int response);
  void OnDialogWidgetClicked(GtkWidget* widget);

  static void HandleOnFlashLinkClicked(GtkWidget* button,
                                       ClearBrowsingDataDialogGtk* user_data) {
    user_data->OnFlashLinkClicked(button);
  }
  void OnFlashLinkClicked(GtkWidget* widget);

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


#endif  // CHROME_BROWSER_GTK_CLEAR_BROWSING_DATA_DIALOG_GTK_H_
