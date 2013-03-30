// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOOLBAR_ACTION_BOX_CONTEXT_MENU_CONTROLLER_H__
#define CHROME_BROWSER_UI_TOOLBAR_ACTION_BOX_CONTEXT_MENU_CONTROLLER_H__

#include <string>

#include "base/basictypes.h"
#include "ui/base/models/simple_menu_model.h"

class Browser;

namespace extensions {
class Extension;
}  // namespace extensions

namespace ui {
class Accelerator;
class MenuModel;
}  // namespace ui

// This class handles building a context menu for extensions in the action
// box. It also contains the logic necessary for executing the actions
// associated with each menu entry.
class ActionBoxContextMenuController : public ui::SimpleMenuModel::Delegate {
 public:
  // |browser| and |extension| must not be NULL.
  ActionBoxContextMenuController(Browser* browser,
                                 const extensions::Extension* extension);
  virtual ~ActionBoxContextMenuController();

  ui::MenuModel* menu_model() { return &menu_model_; }

  // ui::SimpleMenuModel::Delegate:
  virtual bool IsCommandIdChecked(int command_id) const OVERRIDE;
  virtual bool IsCommandIdEnabled(int command_id) const OVERRIDE;
  virtual bool GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) OVERRIDE;
  virtual void ExecuteCommand(int command_id, int event_flags) OVERRIDE;

 private:
  const extensions::Extension* GetExtension() const;

  Browser* browser_;
  std::string extension_id_;
  ui::SimpleMenuModel menu_model_;

  DISALLOW_COPY_AND_ASSIGN(ActionBoxContextMenuController);
};

#endif  // CHROME_BROWSER_UI_TOOLBAR_ACTION_BOX_CONTEXT_MENU_CONTROLLER_H__
