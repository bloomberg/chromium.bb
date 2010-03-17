// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_ACTION_CONTEXT_MENU_MODEL_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_ACTION_CONTEXT_MENU_MODEL_H_

#include "app/menus/simple_menu_model.h"
#include "chrome/browser/extensions/extension_install_ui.h"

class Extension;
class ExtensionAction;
class PrefService;

// The menu model for the context menu for extension action icons (browser and
// page actions).
class ExtensionActionContextMenuModel
    : public menus::SimpleMenuModel,
      public menus::SimpleMenuModel::Delegate,
      public ExtensionInstallUI::Delegate {
 public:
  // Delegate to handle menu commands.
  class MenuDelegate {
   public:
    // Called when the user selects the menu item which requests that the
    // popup be shown and inspected.
    virtual void ShowPopupForDevToolsWindow(Extension* extension,
        ExtensionAction* extension_action) {
    }
  };

  // |extension_action|, |prefs|, & |delegate| call all be NULL. If valid
  // values are provided for all three, and prefs::kExtensionsUIDeveloperMode
  // is enabled in the PrefService, a menu item will be shown for "Inspect
  // Popup" which, when selected, will cause ShowPopupForDevToolsWindow() to be
  // called on |delegate|.
  ExtensionActionContextMenuModel(Extension* extension,
                                  ExtensionAction* extension_action,
                                  PrefService* prefs,
                                  MenuDelegate* delegate);
  ~ExtensionActionContextMenuModel();

  // SimpleMenuModel behavior overrides.
  virtual bool IsCommandIdChecked(int command_id) const;
  virtual bool IsCommandIdEnabled(int command_id) const;
  virtual bool GetAcceleratorForCommandId(int command_id,
                                          menus::Accelerator* accelerator);
  virtual void ExecuteCommand(int command_id);

  // ExtensionInstallUI::Delegate overrides.
  virtual void InstallUIProceed(bool create_app);
  virtual void InstallUIAbort() {}

 private:
  // The extension we are displaying the context menu for.
  Extension* extension_;

  // The extension action we are displaying the context menu for.
  ExtensionAction* extension_action_;

  // The delegate which handles the 'inspect popup' menu command.
  MenuDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionActionContextMenuModel);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_ACTION_CONTEXT_MENU_MODEL_H_
