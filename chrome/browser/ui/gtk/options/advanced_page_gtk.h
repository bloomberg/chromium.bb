// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_OPTIONS_ADVANCED_PAGE_GTK_H_
#define CHROME_BROWSER_UI_GTK_OPTIONS_ADVANCED_PAGE_GTK_H_
#pragma once

#include <gtk/gtk.h>

#include "chrome/browser/prefs/pref_member.h"
#include "chrome/browser/ui/gtk/options/advanced_contents_gtk.h"
#include "chrome/browser/ui/gtk/options/managed_prefs_banner_gtk.h"
#include "chrome/browser/ui/options/options_page_base.h"
#include "ui/base/gtk/gtk_signal.h"

class Profile;

class AdvancedPageGtk : public OptionsPageBase {
 public:
  explicit AdvancedPageGtk(Profile* profile);
  virtual ~AdvancedPageGtk();

  GtkWidget* get_page_widget() const {
    return page_;
  }

 private:
  void Init();

  // Callback for reset to default button.
  CHROMEGTK_CALLBACK_0(AdvancedPageGtk, void, OnResetToDefaultsClicked);

  // Callback for reset to default confirmation dialog.
  CHROMEGTK_CALLBACK_1(AdvancedPageGtk, void, OnResetToDefaultsResponse, int);

  // The contents of the scroll box.
  AdvancedContentsGtk advanced_contents_;

  // The widget containing the options for this page.
  GtkWidget* page_;

  // Tracks managed preference warning banner state.
  ManagedPrefsBannerGtk managed_prefs_banner_;

  DISALLOW_COPY_AND_ASSIGN(AdvancedPageGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_OPTIONS_ADVANCED_PAGE_GTK_H_
