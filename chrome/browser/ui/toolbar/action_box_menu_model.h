// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOOLBAR_ACTION_BOX_MENU_MODEL_H_
#define CHROME_BROWSER_UI_TOOLBAR_ACTION_BOX_MENU_MODEL_H_

#include <map>

#include "chrome/common/extensions/extension.h"
#include "content/public/browser/notification_observer.h"
#include "ui/base/models/simple_menu_model.h"

class Browser;

// A menu model that builds the contents of the action box menu. This model
// should be built on demand since its content reflects the state of the browser
// at creation time.
class ActionBoxMenuModel : public ui::SimpleMenuModel,
                           public ui::SimpleMenuModel::Delegate,
                           public content::NotificationObserver {
 public:
  explicit ActionBoxMenuModel(Browser* browser);
  virtual ~ActionBoxMenuModel();

  // Returns true if item associated with an extension.
  bool IsItemExtension(int index);

  // Returns an extension associated with model item at |index|
  // or NULL if it is not an extension item.
  const extensions::Extension* GetExtensionAt(int index);

  // Overridden from ui::SimpleMenuModel::Delegate:
  virtual bool IsCommandIdChecked(int command_id) const OVERRIDE;
  virtual bool IsCommandIdEnabled(int command_id) const OVERRIDE;
  virtual bool GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) OVERRIDE;
  virtual void ExecuteCommand(int command_id) OVERRIDE;

 private:
  const extensions::ExtensionList& GetActionBoxMenuItems();

  typedef std::map<int, std::string> IdToEntensionIdMap;

  // Overridden from content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  Browser* browser_;

  IdToEntensionIdMap id_to_extension_id_map_;

  DISALLOW_COPY_AND_ASSIGN(ActionBoxMenuModel);
};

#endif  // CHROME_BROWSER_UI_TOOLBAR_ACTION_BOX_MENU_MODEL_H_
