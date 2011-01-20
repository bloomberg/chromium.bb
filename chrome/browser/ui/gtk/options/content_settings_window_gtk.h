// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_OPTIONS_CONTENT_SETTINGS_WINDOW_GTK_H_
#define CHROME_BROWSER_UI_GTK_OPTIONS_CONTENT_SETTINGS_WINDOW_GTK_H_
#pragma once

#include <gtk/gtk.h>

#include "base/scoped_ptr.h"
#include "chrome/browser/prefs/pref_member.h"
#include "chrome/browser/ui/gtk/options/content_filter_page_gtk.h"
#include "chrome/browser/ui/gtk/options/cookie_filter_page_gtk.h"
#include "chrome/common/content_settings_types.h"
#include "ui/base/gtk/gtk_signal.h"

class AccessibleWidgetHelper;

// A window that presents options to the user for blocking various kinds of
// content in webpages (cookies, javascript, images, popups).
class ContentSettingsWindowGtk {
 public:
  // Shows the current content settings page, opening a new one if it doesn't
  // exist.
  static void Show(GtkWindow* parent, ContentSettingsType page,
                   Profile* profile);
  static void RegisterUserPrefs(PrefService* prefs);

  explicit ContentSettingsWindowGtk(GtkWindow* parent, Profile* profile);
  virtual ~ContentSettingsWindowGtk();

 private:
  // Shows the Tab corresponding to the specified Content Settings page.
  void ShowContentSettingsTab(ContentSettingsType page);

  CHROMEGTK_CALLBACK_2(ContentSettingsWindowGtk, void, OnSwitchPage,
                       GtkNotebookPage*, guint);
  CHROMEGTK_CALLBACK_0(ContentSettingsWindowGtk, void, OnWindowDestroy);
  CHROMEG_CALLBACK_0(ContentSettingsWindowGtk, void, OnListSelectionChanged,
                     GtkTreeSelection*);

  // The options dialog.
  GtkWidget* dialog_;

  // The container of the option pages.
  GtkWidget* notebook_;
  GtkWidget* list_;

  // The Profile associated with these options.
  Profile* profile_;

  // The last page the user was on when they opened the ContentSettings window.
  IntegerPrefMember last_selected_page_;

  // The individual page implementations. Note that we have a specialized one
  // for cookies (which have more complex rules) and use the same basic page
  // layout for each other type.
  CookieFilterPageGtk cookie_page_;
  ContentFilterPageGtk image_page_;
  ContentFilterPageGtk javascript_page_;
  ContentFilterPageGtk plugin_page_;
  ContentFilterPageGtk popup_page_;
  ContentFilterPageGtk geolocation_page_;
  ContentFilterPageGtk notifications_page_;

  // Helper object to manage accessibility metadata.
  scoped_ptr<AccessibleWidgetHelper> accessible_widget_helper_;

  DISALLOW_COPY_AND_ASSIGN(ContentSettingsWindowGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_OPTIONS_CONTENT_SETTINGS_WINDOW_GTK_H_
