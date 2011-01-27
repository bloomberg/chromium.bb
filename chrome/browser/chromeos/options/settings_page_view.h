// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_OPTIONS_SETTINGS_PAGE_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_OPTIONS_SETTINGS_PAGE_VIEW_H_
#pragma once

#include <gtk/gtk.h>

#include "base/basictypes.h"
#include "chrome/browser/ui/views/options/options_page_view.h"
#include "views/layout/grid_layout.h"

using views::ColumnSet;
using views::GridLayout;

namespace chromeos {

class SettingsContentsView;

// Settings page for Chrome OS.
class SettingsPageView : public OptionsPageView {
 public:
  explicit SettingsPageView(Profile* profile);

  // Wraps the SettingsPageView in a GtkWidget and returns it. It's up to the
  // caller to delete (unref) the returned widget, which in turn deletes this
  // SettingsPageView.
  GtkWidget* WrapInGtkWidget();

 protected:
  virtual void InitControlLayout() = 0;

  DISALLOW_COPY_AND_ASSIGN(SettingsPageView);
};

// Base section class settings
class SettingsPageSection : public OptionsPageView {
 public:
  explicit SettingsPageSection(Profile* profile, int title_msg_id);
  virtual ~SettingsPageSection() {}

 protected:
  // OptionsPageView overrides:
  virtual void InitControlLayout();
  virtual void InitContents(GridLayout* layout) = 0;

  int single_column_view_set_id() const { return single_column_view_set_id_; }
  int double_column_view_set_id() const { return double_column_view_set_id_; }

 private:
  // The message id for the title of this section.
  int title_msg_id_;

  int single_column_view_set_id_;
  int double_column_view_set_id_;

  DISALLOW_COPY_AND_ASSIGN(SettingsPageSection);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_OPTIONS_SETTINGS_PAGE_VIEW_H_
