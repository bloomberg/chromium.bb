// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOOLBAR_ACTION_BOX_BUTTON_CONTROLLER_H_
#define CHROME_BROWSER_UI_TOOLBAR_ACTION_BOX_BUTTON_CONTROLLER_H_

#include <map>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/base/models/simple_menu_model.h"

class ActionBoxMenuModel;
class Browser;

namespace extensions {
class Extension;
}

namespace ui {
class MenuModel;
}

// Controller for the action box.
//
// This should have the same lifetime as the action box itself -- that is, more
// or less the lifetime of the tab -- unlike ActionBoxMenuModel which is scoped
// to the menu being open.
class ActionBoxButtonController : public ui::SimpleMenuModel::Delegate,
                                  public content::NotificationObserver {
 public:
  class Delegate {
   public:
    // Shows the menu with the given |menu_model|.
    virtual void ShowMenu(scoped_ptr<ActionBoxMenuModel> menu_model);

   protected:
    virtual ~Delegate() {}
  };

  ActionBoxButtonController(Browser* browser, Delegate* delegate);
  virtual ~ActionBoxButtonController();

  // Creates and populates an ActionBoxMenuModel according to the current
  // state of the browser.
  scoped_ptr<ActionBoxMenuModel> CreateMenuModel();

  // Notifies this that the action box button has been clicked.
  // Methods on the Delegate may be called re-entrantly.
  void OnButtonClicked();

  // Overridden from ui::SimpleMenuModel::Delegate:
  virtual bool IsCommandIdChecked(int command_id) const OVERRIDE;
  virtual bool IsCommandIdEnabled(int command_id) const OVERRIDE;
  virtual bool GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) OVERRIDE;
  virtual void ExecuteCommand(int command_id, int event_flags) OVERRIDE;

 private:
  // Gets the command ID for an extension, creating a new one if necessary.
  int GetCommandIdForExtension(const extensions::Extension& extension);

  // Returns the next command ID to be used.
  int GetNextCommandId();

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  Browser* browser_;

  Delegate* delegate_;

  typedef std::map<int, std::string> ExtensionIdCommandMap;
  ExtensionIdCommandMap extension_command_ids_;

  // The command ID to assign to the next dynamic entry that needs one.
  int next_command_id_;

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ActionBoxButtonController);
};

#endif  // CHROME_BROWSER_UI_TOOLBAR_ACTION_BOX_BUTTON_CONTROLLER_H_
