// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PANELS_PANEL_SETTINGS_MENU_MODEL_H_
#define CHROME_BROWSER_UI_PANELS_PANEL_SETTINGS_MENU_MODEL_H_
#pragma once

#include "chrome/browser/extensions/extension_uninstall_dialog.h"
#include "ui/base/models/simple_menu_model.h"

class Extension;
class Panel;

class PanelSettingsMenuModel : public ui::SimpleMenuModel,
                               public ui::SimpleMenuModel::Delegate,
                               public ExtensionUninstallDialog::Delegate {
 public:
  explicit PanelSettingsMenuModel(Panel* panel);
  virtual ~PanelSettingsMenuModel();

 private:
  // Overridden from ui::SimpleMenuModel::Delegate:
  virtual bool IsCommandIdChecked(int command_id) const OVERRIDE;
  virtual bool IsCommandIdEnabled(int command_id) const OVERRIDE;
  virtual bool GetAcceleratorForCommandId(
      int command_id, ui::Accelerator* accelerator) OVERRIDE;
  virtual void ExecuteCommand(int command_id) OVERRIDE;

  // ExtensionUninstallDialog::Delegate:
  virtual void ExtensionDialogAccepted() OVERRIDE;
  virtual void ExtensionDialogCanceled() OVERRIDE;

 private:
  friend class PanelBrowserTest;

  enum {
    COMMAND_NAME = 0,
    COMMAND_CONFIGURE,
    COMMAND_DISABLE,
    COMMAND_UNINSTALL,
    COMMAND_MANAGE
  };

  Panel* panel_;
  scoped_ptr<ExtensionUninstallDialog> extension_uninstall_dialog_;

  DISALLOW_COPY_AND_ASSIGN(PanelSettingsMenuModel);
};

#endif  // CHROME_BROWSER_UI_PANELS_PANEL_SETTINGS_MENU_MODEL_H_
