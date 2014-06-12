// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APPS_APP_INFO_DIALOG_APP_INFO_SUMMARY_PANEL_H_
#define CHROME_BROWSER_UI_VIEWS_APPS_APP_INFO_DIALOG_APP_INFO_SUMMARY_PANEL_H_

#include "base/callback_forward.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_panel.h"
#include "chrome/common/extensions/extension_constants.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/combobox/combobox_listener.h"

class LaunchOptionsComboboxModel;
class Profile;

namespace extensions {
class Extension;
}

namespace ui {
class Event;
}

namespace views {
class Combobox;
class Label;
class LabelButton;
}

// The summary panel of the app info dialog, which provides basic information
// and controls related to the app.
class AppInfoSummaryPanel : public AppInfoPanel,
                            public views::ComboboxListener,
                            public views::ButtonListener {
 public:
  AppInfoSummaryPanel(gfx::NativeWindow parent_window,
                      Profile* profile,
                      const extensions::Extension* app,
                      const base::Closure& close_callback);

  virtual ~AppInfoSummaryPanel();

 private:
  // Internal initialisation methods.
  void CreateDescriptionControl();
  void CreateLaunchOptionControl();
  void CreateShortcutsButton();

  void LayoutDescriptionControl();
  void LayoutShortcutsButton();

  // Overridden from views::ComboboxListener:
  virtual void OnPerformAction(views::Combobox* combobox) OVERRIDE;

  // Overridden from views::ButtonListener.
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  // Returns the launch type of the app (e.g. pinned tab, fullscreen, etc).
  extensions::LaunchType GetLaunchType() const;

  // Sets the launch type of the app to the given type. Must only be called if
  // CanSetLaunchType() returns true.
  void SetLaunchType(extensions::LaunchType) const;
  bool CanSetLaunchType() const;

  // Create Shortcuts for the app. Must only be called if CanCreateShortcuts()
  // returns true.
  void CreateShortcuts();
  bool CanCreateShortcuts() const;

  // UI elements on the dialog.
  views::Label* description_heading_;
  views::Label* description_label_;

  views::LabelButton* create_shortcuts_button_;

  scoped_ptr<LaunchOptionsComboboxModel> launch_options_combobox_model_;
  views::Combobox* launch_options_combobox_;

  DISALLOW_COPY_AND_ASSIGN(AppInfoSummaryPanel);
};

#endif  // CHROME_BROWSER_UI_VIEWS_APPS_APP_INFO_DIALOG_APP_INFO_SUMMARY_PANEL_H_
