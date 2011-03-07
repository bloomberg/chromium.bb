// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_OPTIONS_CONTENT_FILTER_PAGE_GTK_H_
#define CHROME_BROWSER_UI_GTK_OPTIONS_CONTENT_FILTER_PAGE_GTK_H_
#pragma once

#include <gtk/gtk.h>

#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/ui/options/options_page_base.h"
#include "chrome/common/content_settings.h"
#include "chrome/common/content_settings_types.h"
#include "content/common/notification_registrar.h"
#include "ui/base/gtk/gtk_signal.h"

class ContentSettingsDetails;

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

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  // This method is called during initialization to set the initial state of the
  // buttons and called after a default content setting change (either value
  // change or "is managed" state).
  virtual void UpdateButtonsState();

  virtual void NotifyContentSettingsChanged(
      const ContentSettingsDetails* details);

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

  GtkWidget* exceptions_button_;

  NotificationRegistrar registrar_;

  // If state of the UI is not changed by a user-action we need to ignore
  // "toggled" events.
  bool ignore_toggle_;

  DISALLOW_COPY_AND_ASSIGN(ContentFilterPageGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_OPTIONS_CONTENT_FILTER_PAGE_GTK_H_
