// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_OPTIONS_MANAGED_PREFS_BANNER_GTK_H_
#define CHROME_BROWSER_UI_GTK_OPTIONS_MANAGED_PREFS_BANNER_GTK_H_
#pragma once

#include <gtk/gtk.h>

#include "chrome/browser/policy/managed_prefs_banner_base.h"

// Constructs and maintains a GTK widget displaying a warning banner. The banner
// is displayed on the preferences dialog whenever there are options that are
// not settable by the user due to policy.
class ManagedPrefsBannerGtk : public policy::ManagedPrefsBannerBase {
 public:
  ManagedPrefsBannerGtk(PrefService* prefs, OptionsPage page);
  virtual ~ManagedPrefsBannerGtk() { }

  GtkWidget* banner_widget() { return banner_widget_; }

 protected:
  // Update widget visibility.
  virtual void OnUpdateVisibility();

 private:
  // Construct the widget.
  void InitWidget();

  GtkWidget* banner_widget_;

  DISALLOW_COPY_AND_ASSIGN(ManagedPrefsBannerGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_OPTIONS_MANAGED_PREFS_BANNER_GTK_H_
