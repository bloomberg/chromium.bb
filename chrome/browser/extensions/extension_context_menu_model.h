// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_CONTEXT_MENU_MODEL_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_CONTEXT_MENU_MODEL_H_
#pragma once

#include <string>

#include "app/menus/simple_menu_model.h"
#include "chrome/browser/extensions/extension_install_ui.h"

class Browser;
class Extension;
class ExtensionAction;
class Profile;

// The menu model for the context menu for extension action icons (browser and
// page actions).
class ExtensionContextMenuModel
    : public base::RefCounted<ExtensionContextMenuModel>,
      public menus::SimpleMenuModel,
      public menus::SimpleMenuModel::Delegate,
      public ExtensionInstallUI::Delegate {
 public:
  // Delegate to handle showing an ExtensionAction popup.
  class PopupDelegate {
   public:
    // Called when the user selects the menu item which requests that the
    // popup be shown and inspected.
    virtual void InspectPopup(ExtensionAction* action) = 0;

   protected:
    virtual ~PopupDelegate() {}
  };

  // Creates a menu model for the given extension action. If
  // prefs::kExtensionsUIDeveloperMode is enabled then a menu item
  // will be shown for "Inspect Popup" which, when selected, will cause
  // ShowPopupForDevToolsWindow() to be called on |delegate|.
  ExtensionContextMenuModel(const Extension* extension,
                            Browser* browser,
                            PopupDelegate* delegate);
  virtual ~ExtensionContextMenuModel();

  // SimpleMenuModel::Delegate overrides.
  virtual bool IsCommandIdChecked(int command_id) const;
  virtual bool IsCommandIdEnabled(int command_id) const;
  virtual bool GetAcceleratorForCommandId(int command_id,
                                          menus::Accelerator* accelerator);
  virtual void ExecuteCommand(int command_id);

  // ExtensionInstallUI::Delegate overrides.
  virtual void InstallUIProceed();
  virtual void InstallUIAbort();

 private:
  void InitCommonCommands();

  // Gets the extension we are displaying the menu for. Returns NULL if the
  // extension has been uninstalled and no longer exists.
  const Extension* GetExtension() const;

  // A copy of the extension's id.
  std::string extension_id_;

  // The extension action we are displaying the menu for (or NULL).
  ExtensionAction* extension_action_;

  Browser* browser_;

  Profile* profile_;

  // The delegate which handles the 'inspect popup' menu command (or NULL).
  PopupDelegate* delegate_;

  // Keeps track of the extension install UI.
  scoped_ptr<ExtensionInstallUI> install_ui_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionContextMenuModel);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_CONTEXT_MENU_MODEL_H_
