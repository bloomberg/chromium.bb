// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_CONTEXT_MENU_MODEL_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_CONTEXT_MENU_MODEL_H_
#pragma once

#include <string>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/extension_uninstall_dialog.h"
#include "ui/base/models/simple_menu_model.h"

class Browser;
class Extension;
class ExtensionAction;
class Profile;

// The menu model for the context menu for extension action icons (browser and
// page actions).
class ExtensionContextMenuModel
    : public base::RefCounted<ExtensionContextMenuModel>,
      public ui::SimpleMenuModel,
      public ui::SimpleMenuModel::Delegate,
      public ExtensionUninstallDialog::Delegate {
 public:
  // Creates a menu model for the given extension action.
  ExtensionContextMenuModel(const Extension* extension, Browser* browser);

  // SimpleMenuModel::Delegate overrides.
  virtual bool IsCommandIdChecked(int command_id) const OVERRIDE;
  virtual bool IsCommandIdEnabled(int command_id) const OVERRIDE;
  virtual bool GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) OVERRIDE;
  virtual void ExecuteCommand(int command_id) OVERRIDE;

  // ExtensionUninstallDialog::Delegate:
  virtual void ExtensionUninstallAccepted() OVERRIDE;
  virtual void ExtensionUninstallCanceled() OVERRIDE;

 private:
  friend class base::RefCounted<ExtensionContextMenuModel>;
  virtual ~ExtensionContextMenuModel();

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

  // Keeps track of the extension uninstall dialog.
  scoped_ptr<ExtensionUninstallDialog> extension_uninstall_dialog_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionContextMenuModel);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_CONTEXT_MENU_MODEL_H_
