// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_OPTIONS_CONTENT_FILTER_PAGE_GTK_H_
#define CHROME_BROWSER_GTK_OPTIONS_CONTENT_FILTER_PAGE_GTK_H_
#pragma once

#include <gtk/gtk.h>

#include "app/gtk_signal.h"
#include "chrome/browser/options_page_base.h"
#include "chrome/common/content_settings_types.h"

// A page in the content settings window. Used for everything but the Cookies
// page (which has a much more complex dialog). A |content_type| is passed into
// the constructor and the correct strings and settings are used.
class ContentFilterPageGtk : public OptionsPageBase {
 public:
  ContentFilterPageGtk(Profile* profile, ContentSettingsType content_type);
  virtual ~ContentFilterPageGtk();

  GtkWidget* get_page_widget() const {
    return page_;
  }

 private:
  // Builds the content of the dialog.
  GtkWidget* InitGroup();

  CHROMEGTK_CALLBACK_0(ContentFilterPageGtk, void, OnAllowToggled);
  CHROMEGTK_CALLBACK_0(ContentFilterPageGtk, void, OnExceptionsClicked);
  CHROMEGTK_CALLBACK_0(ContentFilterPageGtk, void, OnPluginsPageLinkClicked);

  ContentSettingsType content_type_;

  GtkWidget* page_;

  // Controls for the content filter tab page.
  GtkWidget* allow_radio_;
  GtkWidget* ask_radio_;
  GtkWidget* block_radio_;

  DISALLOW_COPY_AND_ASSIGN(ContentFilterPageGtk);
};

#endif  // CHROME_BROWSER_GTK_OPTIONS_CONTENT_FILTER_PAGE_GTK_H_
