// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APPS_APP_INFO_DIALOG_APP_INFO_SUMMARY_TAB_H_
#define CHROME_BROWSER_UI_VIEWS_APPS_APP_INFO_DIALOG_APP_INFO_SUMMARY_TAB_H_

#include "chrome/browser/extensions/extension_uninstall_dialog.h"
#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_tab.h"
#include "chrome/common/extensions/extension_constants.h"
#include "ui/base/models/combobox_model.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/combobox/combobox_listener.h"
#include "ui/views/controls/link_listener.h"

class Profile;

namespace extensions {
class Extension;
}
namespace gfx {
class Image;
}
namespace ui {
class Event;
}
namespace views {
class Combobox;
class ImageView;
class Label;
class LabelButton;
}

class LaunchOptionsComboboxModel;

// The Summary tab of the app info dialog, which provides basic information and
// controls related to the app.
class AppInfoSummaryTab : public AppInfoTab,
                          public views::ComboboxListener,
                          public views::ButtonListener,
                          public ExtensionUninstallDialog::Delegate {
 public:
  AppInfoSummaryTab(gfx::NativeWindow parent_window,
                    Profile* profile,
                    const extensions::Extension* app,
                    const base::Closure& close_callback);

  virtual ~AppInfoSummaryTab();

 private:
  // Internal initialisation methods.
  void CreateDescriptionControl();
  void CreateLaunchOptionControl();
  void CreateButtons();

  void LayoutButtons();

  // Overridden from views::ComboboxListener:
  virtual void OnPerformAction(views::Combobox* combobox) OVERRIDE;

  // Overridden from views::ButtonListener.
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  // Overridden from ExtensionUninstallDialog::Delegate.
  virtual void ExtensionUninstallAccepted() OVERRIDE;
  virtual void ExtensionUninstallCanceled() OVERRIDE;

  // Returns the launch type of the app (e.g. pinned tab, fullscreen, etc).
  extensions::LaunchType GetLaunchType() const;

  // Sets the launch type of the app to the given type. Must only be called if
  // CanSetLaunchType() returns true.
  void SetLaunchType(extensions::LaunchType) const;
  bool CanSetLaunchType() const;

  // Uninstall the app. Must only be called if CanUninstallApp() returns true.
  void UninstallApp();
  bool CanUninstallApp() const;

  // Create Shortcuts for the app. Must only be called if CanCreateShortcuts()
  // returns true.
  void CreateShortcuts();
  bool CanCreateShortcuts() const;

  // UI elements on the dialog.
  views::View* app_summary_panel_;
  views::Label* app_description_label_;
  views::LabelButton* create_shortcuts_button_;

  scoped_ptr<ExtensionUninstallDialog> extension_uninstall_dialog_;
  views::LabelButton* uninstall_button_;

  scoped_ptr<LaunchOptionsComboboxModel> launch_options_combobox_model_;
  views::Combobox* launch_options_combobox_;

  DISALLOW_COPY_AND_ASSIGN(AppInfoSummaryTab);
};

#endif  // CHROME_BROWSER_UI_VIEWS_APPS_APP_INFO_DIALOG_APP_INFO_SUMMARY_TAB_H_
