// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOOLBAR_ACTION_BOX_MENU_MODEL_H_
#define CHROME_BROWSER_UI_TOOLBAR_ACTION_BOX_MENU_MODEL_H_

#include <map>

#include "content/public/browser/notification_observer.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/ui/browser_commands.h"
#include "ui/base/models/simple_menu_model.h"

class Browser;

namespace extensions {
class Extension;
}

// A menu model that builds the contents of the action box menu.
class ActionBoxMenuModel : public ui::SimpleMenuModel,
                           public ui::SimpleMenuModel::Delegate,
                           public content::NotificationObserver {
 public:
  // |starred| - true when the current page is bookmarked and thus the star icon
  // should be drawn in the "starred" rather than "unstarred" state.
  ActionBoxMenuModel(Browser* browser,
                     ExtensionService* extension_service,
                     bool starred);
  virtual ~ActionBoxMenuModel();

  // Returns true if item associated with an extension.
  bool IsItemExtension(int index);

  // Returns an extension associated with model item at |index|
  // or NULL if it is not an extension item.
  const extensions::Extension* GetExtensionAt(int index);

 private:
  const extensions::ExtensionList& action_box_menu_items() {
    return extension_service_->toolbar_model()->action_box_menu_items();
  }

  typedef std::map<int, std::string> IdToEntensionIdMap;

  // Overridden from ui::SimpleMenuModel::Delegate:
  virtual bool IsCommandIdChecked(int command_id) const OVERRIDE;
  virtual bool IsCommandIdEnabled(int command_id) const OVERRIDE;
  virtual bool GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) OVERRIDE;
  virtual void ExecuteCommand(int command_id) OVERRIDE;

  // Overridden from content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  Browser* browser_;
  ExtensionService* extension_service_;

  IdToEntensionIdMap id_to_extension_id_map_;

  DISALLOW_COPY_AND_ASSIGN(ActionBoxMenuModel);
};

#endif  // CHROME_BROWSER_UI_TOOLBAR_ACTION_BOX_MENU_MODEL_H_
