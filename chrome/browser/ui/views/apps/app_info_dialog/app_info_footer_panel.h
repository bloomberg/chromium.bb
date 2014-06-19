// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APPS_APP_INFO_DIALOG_APP_INFO_FOOTER_PANEL_H_
#define CHROME_BROWSER_UI_VIEWS_APPS_APP_INFO_DIALOG_APP_INFO_FOOTER_PANEL_H_

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/extensions/extension_uninstall_dialog.h"
#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_panel.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/controls/button/button.h"

class Profile;

namespace extensions {
class Extension;
}

namespace ui {
class Event;
}

namespace views {
class LabelButton;
}

// A small summary panel with buttons to control the app that is displayed at
// the bottom of the app info dialog.
class AppInfoFooterPanel
    : public AppInfoPanel,
      public views::ButtonListener,
      public extensions::ExtensionUninstallDialog::Delegate,
      public base::SupportsWeakPtr<AppInfoFooterPanel> {
 public:
  AppInfoFooterPanel(gfx::NativeWindow parent_window,
                     Profile* profile,
                     const extensions::Extension* app);
  virtual ~AppInfoFooterPanel();

 private:
  void CreateButtons();
  void LayoutButtons();

  // Updates the visibility of the pin/unpin buttons so that only one is visible
  // at a time.
  void UpdatePinButtons();

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  // Overridden from ExtensionUninstallDialog::Delegate:
  virtual void ExtensionUninstallAccepted() OVERRIDE;
  virtual void ExtensionUninstallCanceled() OVERRIDE;

  // Create Shortcuts for the app. Must only be called if CanCreateShortcuts()
  // returns true.
  void CreateShortcuts();
  bool CanCreateShortcuts() const;

  // Pins and unpins the app from the shelf. Must only be called if
  // CanSetPinnedToShelf() returns true.
  void SetPinnedToShelf(bool value);
  bool CanSetPinnedToShelf() const;

  // Uninstall the app. Must only be called if CanUninstallApp() returns true.
  void UninstallApp();
  bool CanUninstallApp() const;

  gfx::NativeWindow parent_window_;

  // UI elements on the dialog. Elements are NULL if they are not displayed.
  views::LabelButton* create_shortcuts_button_;
  views::LabelButton* pin_to_shelf_button_;
  views::LabelButton* unpin_from_shelf_button_;
  views::LabelButton* remove_button_;

  scoped_ptr<extensions::ExtensionUninstallDialog> extension_uninstall_dialog_;

  base::WeakPtrFactory<AppInfoFooterPanel> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(AppInfoFooterPanel);
};

#endif  // CHROME_BROWSER_UI_VIEWS_APPS_APP_INFO_DIALOG_APP_INFO_FOOTER_PANEL_H_
