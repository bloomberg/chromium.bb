// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_OPTIONS_CONTENT_PAGE_GTK_H_
#define CHROME_BROWSER_GTK_OPTIONS_CONTENT_PAGE_GTK_H_

#include <gtk/gtk.h>

#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/options_page_base.h"
#include "chrome/browser/profile.h"
#include "chrome/common/pref_member.h"

class ContentPageGtk : public OptionsPageBase,
                       public ProfileSyncServiceObserver {
 public:
  explicit ContentPageGtk(Profile* profile);
  ~ContentPageGtk();

  GtkWidget* get_page_widget() const {
    return page_;
  }

  // ProfileSyncServiceObserver method.
  virtual void OnStateChanged();

 private:
  // Updates various sync controls based on the current sync state.
  void UpdateSyncControls();

  // Overridden from OptionsPageBase.
  virtual void NotifyPrefChanged(const std::wstring* pref_name);

  // Overridden from OptionsPageBase.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Update content area after a theme changed.
  void ObserveThemeChanged();

  // Initialize the option group widgets, return their container.
  GtkWidget* InitPasswordSavingGroup();
  GtkWidget* InitFormAutofillGroup();
  GtkWidget* InitBrowsingDataGroup();
  GtkWidget* InitThemesGroup();
  GtkWidget* InitSyncGroup();

  // Callback for import button.
  static void OnImportButtonClicked(GtkButton* widget, ContentPageGtk* page);

  // Callback for clear data button.
  static void OnClearBrowsingDataButtonClicked(GtkButton* widget,
                                               ContentPageGtk* page);

  // Callback for the GTK theme button.
  static void OnGtkThemeButtonClicked(GtkButton* widget,
                                      ContentPageGtk* page);

  // Callback for reset default theme button.
  static void OnResetDefaultThemeButtonClicked(GtkButton* widget,
                                               ContentPageGtk* page);

  // Callback for get themes button.
  static void OnGetThemesButtonClicked(GtkButton* widget,
                                       ContentPageGtk* page);

  // Callback for system title bar radio buttons.
  static void OnSystemTitleBarRadioToggled(GtkToggleButton* widget,
                                           ContentPageGtk* page);

  // Callback for passwords exceptions button.
  static void OnPasswordsExceptionsButtonClicked(GtkButton* widget,
                                                 ContentPageGtk* page);

  // Callback for password radio buttons.
  static void OnPasswordRadioToggled(GtkToggleButton* widget,
                                     ContentPageGtk* page);

  // Callback for form autofill radio buttons.
  static void OnAutofillRadioToggled(GtkToggleButton* widget,
                                     ContentPageGtk* page);

  // Callback for sync start/stop button.
  static void OnSyncStartStopButtonClicked(GtkButton* widget,
                                           ContentPageGtk* page);

  // Callback for sync action link.
  static void OnSyncActionLinkClicked(GtkButton* widget,
                                      ContentPageGtk* page);

  // Callback for stop sync dialog.
  static void OnStopSyncDialogResponse(GtkWidget* widget,
                                       int response,
                                       ContentPageGtk* page);

  // Widgets for the Password saving group.
  GtkWidget* passwords_asktosave_radio_;
  GtkWidget* passwords_neversave_radio_;

  // Widgets for the Form Autofill group.
  GtkWidget* form_autofill_asktosave_radio_;
  GtkWidget* form_autofill_neversave_radio_;

  // Widgets for the Appearance group.
  GtkWidget* system_title_bar_show_radio_;
  GtkWidget* system_title_bar_hide_radio_;
  GtkWidget* themes_reset_button_;
#if defined(TOOLKIT_GTK)
  GtkWidget* gtk_theme_button_;
#endif

  // Widgets for the Sync group.
  GtkWidget* sync_status_label_background_;
  GtkWidget* sync_status_label_;
  GtkWidget* sync_action_link_background_;
  GtkWidget* sync_action_link_;
  GtkWidget* sync_start_stop_button_;

  // The parent GtkTable widget
  GtkWidget* page_;

  // Pref members.
  BooleanPrefMember ask_to_save_passwords_;
  BooleanPrefMember ask_to_save_form_autofill_;
  BooleanPrefMember use_custom_chrome_frame_;

  // Flag to ignore gtk callbacks while we are loading prefs, to avoid
  // then turning around and saving them again.
  bool initializing_;

  NotificationRegistrar registrar_;

  // Cached pointer to ProfileSyncService, if it exists. Kept up to date
  // and NULL-ed out on destruction.
  ProfileSyncService* sync_service_;

  DISALLOW_COPY_AND_ASSIGN(ContentPageGtk);
};

#endif  // CHROME_BROWSER_GTK_OPTIONS_CONTENT_PAGE_GTK_H_
