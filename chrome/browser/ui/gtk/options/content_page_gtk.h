// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_OPTIONS_CONTENT_PAGE_GTK_H_
#define CHROME_BROWSER_UI_GTK_OPTIONS_CONTENT_PAGE_GTK_H_
#pragma once

#include <gtk/gtk.h>

#include <string>

#include "chrome/browser/autofill/personal_data_manager.h"
#include "chrome/browser/prefs/pref_member.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/ui/gtk/options/managed_prefs_banner_gtk.h"
#include "chrome/browser/ui/options/options_page_base.h"
#include "ui/base/gtk/gtk_signal.h"

class Profile;

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
  virtual void NotifyPrefChanged(const std::string* pref_name);

  // Overridden from OptionsPageBase.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Update content area after a theme changed.
  void ObserveThemeChanged();

  // Initialize the option group widgets, return their container.
  GtkWidget* InitPasswordSavingGroup();
  GtkWidget* InitFormAutoFillGroup();
  GtkWidget* InitBrowsingDataGroup();
  GtkWidget* InitThemesGroup();
  GtkWidget* InitSyncGroup();

  CHROMEGTK_CALLBACK_0(ContentPageGtk, void, OnImportButtonClicked);
  CHROMEGTK_CALLBACK_0(ContentPageGtk, void, OnGtkThemeButtonClicked);
  CHROMEGTK_CALLBACK_0(ContentPageGtk, void, OnResetDefaultThemeButtonClicked);
  CHROMEGTK_CALLBACK_0(ContentPageGtk, void, OnGetThemesButtonClicked);
  CHROMEGTK_CALLBACK_0(ContentPageGtk, void, OnSystemTitleBarRadioToggled);
  CHROMEGTK_CALLBACK_0(ContentPageGtk, void, OnShowPasswordsButtonClicked);
  CHROMEGTK_CALLBACK_0(ContentPageGtk, void, OnPasswordRadioToggled);
  CHROMEGTK_CALLBACK_0(ContentPageGtk, void, OnAutoFillButtonClicked);
  CHROMEGTK_CALLBACK_0(ContentPageGtk, void, OnSyncStartStopButtonClicked);
  CHROMEGTK_CALLBACK_0(ContentPageGtk, void, OnSyncCustomizeButtonClicked);
  CHROMEGTK_CALLBACK_0(ContentPageGtk, void, OnSyncActionLinkClicked);
  CHROMEGTK_CALLBACK_1(ContentPageGtk, void, OnStopSyncDialogResponse, int);
  CHROMEGTK_CALLBACK_0(ContentPageGtk, void, OnPrivacyDashboardLinkClicked);

  // Widgets for the Password saving group.
  GtkWidget* passwords_asktosave_radio_;
  GtkWidget* passwords_neversave_radio_;
  GtkWidget* show_passwords_button_;

  // Widgets for the AutoFill group.
  GtkWidget* autofill_button_;

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
  GtkWidget* sync_customize_button_;
  GtkWidget* privacy_dashboard_link_;

  // The parent GtkTable widget
  GtkWidget* page_;

  // Pref members.
  BooleanPrefMember ask_to_save_passwords_;
  BooleanPrefMember form_autofill_enabled_;
  BooleanPrefMember use_custom_chrome_frame_;

  // Flag to ignore gtk callbacks while we are loading prefs, to avoid
  // then turning around and saving them again.
  bool initializing_;

  NotificationRegistrar registrar_;

  // Cached pointer to ProfileSyncService, if it exists. Kept up to date
  // and NULL-ed out on destruction.
  ProfileSyncService* sync_service_;

  // Tracks managed preference warning banner state.
  ManagedPrefsBannerGtk managed_prefs_banner_;

  DISALLOW_COPY_AND_ASSIGN(ContentPageGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_OPTIONS_CONTENT_PAGE_GTK_H_
