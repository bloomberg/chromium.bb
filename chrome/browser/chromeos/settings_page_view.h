// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SETTINGS_PAGE_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_SETTINGS_PAGE_VIEW_H_

#include "chrome/browser/views/options/options_page_view.h"

#include <gtk/gtk.h>

// Settings page for Chrome OS.
class SettingsPageView : public OptionsPageView {
 public:
  explicit SettingsPageView(Profile* profile);

  // Wraps the SettingsPageView in a GtkWidget and returns it. It's up to the
  // caller to delete (unref) the returned widget, which in turn deletes this
  // SettingsPageView.
  GtkWidget* WrapInGtkWidget();

 protected:
  virtual void InitControlLayout();

 private:
  DISALLOW_COPY_AND_ASSIGN(SettingsPageView);
};

#endif  // CHROME_BROWSER_CHROMEOS_SETTINGS_PAGE_VIEW_H_
