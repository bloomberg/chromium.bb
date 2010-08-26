// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_OPTIONS_COOKIE_FILTER_PAGE_GTK_H_
#define CHROME_BROWSER_GTK_OPTIONS_COOKIE_FILTER_PAGE_GTK_H_
#pragma once

#include <gtk/gtk.h>

#include <string>

#include "app/gtk_signal.h"
#include "chrome/browser/options_page_base.h"
#include "chrome/browser/prefs/pref_member.h"

class Profile;

// A page in the content settings window for cookie options. This dialog has
// more options as is more complicated then all the other pages implemented
// with ContentPageGtk.
class CookieFilterPageGtk : public OptionsPageBase {
 public:
  explicit CookieFilterPageGtk(Profile* profile);
  virtual ~CookieFilterPageGtk();

  GtkWidget* get_page_widget() const {
    return page_;
  }

 private:
  // Overridden from OptionsPageBase
  virtual void NotifyPrefChanged(const std::string* pref_name);
  virtual void HighlightGroup(OptionsGroup highlight_group);

  CHROMEGTK_CALLBACK_0(CookieFilterPageGtk, void, OnCookiesAllowToggled);
  CHROMEGTK_CALLBACK_0(CookieFilterPageGtk, void, OnExceptionsClicked);
  CHROMEGTK_CALLBACK_0(CookieFilterPageGtk, void, OnBlockThirdPartyToggled);
  CHROMEGTK_CALLBACK_0(CookieFilterPageGtk, void, OnClearOnCloseToggled);
  CHROMEGTK_CALLBACK_0(CookieFilterPageGtk, void, OnShowCookiesClicked);
  CHROMEGTK_CALLBACK_0(CookieFilterPageGtk, void, OnFlashLinkClicked);

  GtkWidget* InitCookieStoringGroup();

  // Widgets of the cookie storing group
  GtkWidget* allow_radio_;
  GtkWidget* ask_every_time_radio_;
  GtkWidget* block_radio_;

  GtkWidget* exceptions_button_;
  GtkWidget* block_3rdparty_check_;
  GtkWidget* clear_on_close_check_;
  GtkWidget* show_cookies_button_;

  // The parent GtkTable widget
  GtkWidget* page_;

  // Whether we're currently setting values (and thus should ignore "toggled"
  // events).
  bool initializing_;

  // Clear locally stored site data on exit pref.
  BooleanPrefMember clear_site_data_on_exit_;

  DISALLOW_COPY_AND_ASSIGN(CookieFilterPageGtk);
};

#endif  // CHROME_BROWSER_GTK_OPTIONS_COOKIE_FILTER_PAGE_GTK_H_
