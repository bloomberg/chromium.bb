// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APPS_APP_INFO_DIALOG_APP_INFO_SUMMARY_PANEL_H_
#define CHROME_BROWSER_UI_VIEWS_APPS_APP_INFO_DIALOG_APP_INFO_SUMMARY_PANEL_H_

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_panel.h"
#include "chrome/common/extensions/extension_constants.h"
#include "ui/views/controls/combobox/combobox_listener.h"

class LaunchOptionsComboboxModel;
class Profile;

namespace extensions {
class Extension;
}

namespace views {
class Combobox;
class Label;
}

// The summary panel of the app info dialog, which provides basic information
// and controls related to the app.
class AppInfoSummaryPanel : public AppInfoPanel,
                            public views::ComboboxListener {
 public:
  AppInfoSummaryPanel(Profile* profile, const extensions::Extension* app);

  virtual ~AppInfoSummaryPanel();

 private:
  // Internal initialisation methods.
  void CreateDescriptionControl();
  void CreateDetailsControl();
  void CreateLaunchOptionControl();

  void LayoutDescriptionControl();
  void LayoutDetailsControl();

  // Overridden from views::ComboboxListener:
  virtual void OnPerformAction(views::Combobox* combobox) OVERRIDE;

  // Returns the time this app was installed.
  base::Time GetInstalledTime() const;

  // Returns the time the app was last launched, or base::Time() if it's never
  // been launched.
  base::Time GetLastLaunchedTime() const;

  // Returns the launch type of the app (e.g. pinned tab, fullscreen, etc).
  extensions::LaunchType GetLaunchType() const;

  // Sets the launch type of the app to the given type. Must only be called if
  // CanSetLaunchType() returns true.
  void SetLaunchType(extensions::LaunchType) const;
  bool CanSetLaunchType() const;

  // UI elements on the dialog.
  views::Label* description_heading_;
  views::Label* description_label_;

  views::Label* details_heading_;
  views::Label* version_title_;
  views::Label* version_value_;
  views::Label* installed_time_title_;
  views::Label* installed_time_value_;
  views::Label* last_run_time_title_;
  views::Label* last_run_time_value_;

  scoped_ptr<LaunchOptionsComboboxModel> launch_options_combobox_model_;
  views::Combobox* launch_options_combobox_;

  DISALLOW_COPY_AND_ASSIGN(AppInfoSummaryPanel);
};

#endif  // CHROME_BROWSER_UI_VIEWS_APPS_APP_INFO_DIALOG_APP_INFO_SUMMARY_PANEL_H_
