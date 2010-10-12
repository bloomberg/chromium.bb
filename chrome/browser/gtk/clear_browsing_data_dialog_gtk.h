// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_CLEAR_BROWSING_DATA_DIALOG_GTK_H_
#define CHROME_BROWSER_GTK_CLEAR_BROWSING_DATA_DIALOG_GTK_H_
#pragma once

#include "app/gtk_signal.h"
#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/sync/profile_sync_service.h"

typedef struct _GtkWidget GtkWidget;
typedef struct _GtkWindow GtkWindow;

class AccessibleWidgetHelper;
class BrowsingDataRemover;
class Profile;

class ClearBrowsingDataDialogGtk : public ProfileSyncServiceObserver {
 public:
  // Displays the dialog box to clear browsing data from |profile|.
  static void Show(GtkWindow* parent, Profile* profile);

 private:
  ClearBrowsingDataDialogGtk(GtkWindow* parent, Profile* profile);
  ~ClearBrowsingDataDialogGtk();

  // Builds the first notebook page in the dialog.
  GtkWidget* BuildClearBrowsingDataPage();

  // Builds the second notebook page in the dialog.
  GtkWidget* BuildOtherDataPage();

  // Creates a paragraph of text, followed by a link.
  GtkWidget* AddDescriptionAndLink(GtkWidget* box,
                                   int description,
                                   int link);

  // Handler to respond to Close responses from the dialog.
  CHROMEGTK_CALLBACK_1(ClearBrowsingDataDialogGtk, void, OnDialogResponse, int);

  // Handler that handles clearing browsing data.
  CHROMEGTK_CALLBACK_0(ClearBrowsingDataDialogGtk,
                       void, OnClearBrowsingDataClick);

  // Handles the appearance of the clear server side conformation box, and
  // handling the response of the "are you sure you want to do this?" message
  // box.
  CHROMEGTK_CALLBACK_0(ClearBrowsingDataDialogGtk,
                       void, OnClearSyncDataClick);
  CHROMEGTK_CALLBACK_1(ClearBrowsingDataDialogGtk, void,
                       OnClearSyncDataConfirmResponse, int);

  // ProfileSyncServiceObserver method. Called in response to profile sync
  // status changes, most likely initiated by OnClearSyncDataConfirmResponse().
  virtual void OnStateChanged();

  // Handler to respond to widget clicked actions from the dialog.
  CHROMEGTK_CALLBACK_0(ClearBrowsingDataDialogGtk, void, OnDialogWidgetClicked);

  CHROMEGTK_CALLBACK_0(ClearBrowsingDataDialogGtk, void, OnFlashLinkClicked);
  CHROMEGTK_CALLBACK_0(ClearBrowsingDataDialogGtk, void, OnPrivacyLinkClicked);

  // Enable or disable the dialog buttons depending on the state of the
  // checkboxes.
  void UpdateDialogButtons();

  // Create a bitmask from the checkboxes of the dialog.
  int GetCheckedItems();

  // Changes the enabled state of |clear_server_data_button_|.
  void UpdateClearButtonState(bool delete_in_progress);

  // The dialog window.
  GtkWidget* dialog_;

  // The notebook in the dialog.
  GtkWidget* notebook_;

  // UI elements on the first page.
  GtkWidget* del_history_checkbox_;
  GtkWidget* del_downloads_checkbox_;
  GtkWidget* del_cache_checkbox_;
  GtkWidget* del_cookies_checkbox_;
  GtkWidget* del_passwords_checkbox_;
  GtkWidget* del_form_data_checkbox_;
  GtkWidget* time_period_combobox_;
  GtkWidget* clear_browsing_data_button_;

  // Widgets on the second page.
  GtkWidget* clear_server_data_button_;
  GtkWidget* clear_server_status_label_;

  // Our current profile.
  Profile* profile_;
  ProfileSyncService* sync_service_;

  // If non-null it means removal is in progress. BrowsingDataRemover takes care
  // of deleting itself when done.
  BrowsingDataRemover* remover_;

  // Helper object to manage accessibility metadata.
  scoped_ptr<AccessibleWidgetHelper> accessible_widget_helper_;

  DISALLOW_COPY_AND_ASSIGN(ClearBrowsingDataDialogGtk);
};


#endif  // CHROME_BROWSER_GTK_CLEAR_BROWSING_DATA_DIALOG_GTK_H_
