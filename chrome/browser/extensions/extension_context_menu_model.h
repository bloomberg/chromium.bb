// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_CONTEXT_MENU_MODEL_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_CONTEXT_MENU_MODEL_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/extension_uninstall_dialog.h"
#include "ui/base/models/simple_menu_model.h"

class Browser;
class ExtensionAction;
class Profile;

namespace extensions {
class Extension;
}

// The context menu model for extension icons.
class ExtensionContextMenuModel
    : public base::RefCounted<ExtensionContextMenuModel>,
      public ui::SimpleMenuModel,
      public ui::SimpleMenuModel::Delegate,
      public extensions::ExtensionUninstallDialog::Delegate {
 public:
  enum MenuEntries {
    NAME = 0,
    CONFIGURE,
    HIDE,
    UNINSTALL,
    MANAGE,
    INSPECT_POPUP
  };

  // Delegate to handle showing an ExtensionAction popup.
  class PopupDelegate {
   public:
    // Called when the user selects the menu item which requests that the
    // popup be shown and inspected.
    // The delegate should know which popup to display.
    virtual void InspectPopup() = 0;

   protected:
    virtual ~PopupDelegate() {}
  };

  // Creates a menu model for the given extension. If
  // prefs::kExtensionsUIDeveloperMode is enabled then a menu item
  // will be shown for "Inspect Popup" which, when selected, will cause
  // ShowPopupForDevToolsWindow() to be called on |delegate|.
  ExtensionContextMenuModel(const extensions::Extension* extension,
                            Browser* browser,
                            PopupDelegate* delegate);

  // Create a menu model for the given extension, without support
  // for the "Inspect Popup" command.
  ExtensionContextMenuModel(const extensions::Extension* extension,
                            Browser* browser);

  // SimpleMenuModel::Delegate overrides.
  virtual bool IsCommandIdChecked(int command_id) const OVERRIDE;
  virtual bool IsCommandIdEnabled(int command_id) const OVERRIDE;
  virtual bool GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) OVERRIDE;
  virtual void ExecuteCommand(int command_id, int event_flags) OVERRIDE;

  // ExtensionUninstallDialog::Delegate:
  virtual void ExtensionUninstallAccepted() OVERRIDE;
  virtual void ExtensionUninstallCanceled() OVERRIDE;

 private:
  friend class base::RefCounted<ExtensionContextMenuModel>;
  virtual ~ExtensionContextMenuModel();

  void InitMenu(const extensions::Extension* extension);

  // Gets the extension we are displaying the menu for. Returns NULL if the
  // extension has been uninstalled and no longer exists.
  const extensions::Extension* GetExtension() const;

  // A copy of the extension's id.
  std::string extension_id_;

  // The extension action of the extension we are displaying the menu for (if
  // it has one, otherwise NULL).
  ExtensionAction* extension_action_;

  Browser* browser_;

  Profile* profile_;

  // The delegate which handles the 'inspect popup' menu command (or NULL).
  PopupDelegate* delegate_;

  // Keeps track of the extension uninstall dialog.
  scoped_ptr<extensions::ExtensionUninstallDialog> extension_uninstall_dialog_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionContextMenuModel);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_CONTEXT_MENU_MODEL_H_
