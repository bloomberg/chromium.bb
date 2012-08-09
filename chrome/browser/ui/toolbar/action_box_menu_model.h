// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOOLBAR_ACTION_BOX_MENU_MODEL_H_
#define CHROME_BROWSER_UI_TOOLBAR_ACTION_BOX_MENU_MODEL_H_

#include <map>

#include "content/public/browser/notification_observer.h"
#include "chrome/browser/extensions/extension_service.h"
#include "ui/base/models/simple_menu_model.h"

namespace extensions {
class Extension;
}

// A menu model that builds the contents of the action box menu.
class ActionBoxMenuModel : public ui::SimpleMenuModel,
                           public content::NotificationObserver {
 public:
  explicit ActionBoxMenuModel(ExtensionService* extension_service);
  virtual ~ActionBoxMenuModel();

  const extensions::ExtensionList& action_box_menu_items() {
    return extension_service_->toolbar_model()->action_box_menu_items();
  }

 private:
  typedef std::map<int, std::string> IdToEntensionIdMap;

  // Overridden from content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  ExtensionService* extension_service_;

  IdToEntensionIdMap id_to_extension_id_map_;

  DISALLOW_COPY_AND_ASSIGN(ActionBoxMenuModel);
};

#endif  // CHROME_BROWSER_UI_TOOLBAR_ACTION_BOX_MENU_MODEL_H_
