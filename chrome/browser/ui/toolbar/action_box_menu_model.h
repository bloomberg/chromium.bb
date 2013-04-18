// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOOLBAR_ACTION_BOX_MENU_MODEL_H_
#define CHROME_BROWSER_UI_TOOLBAR_ACTION_BOX_MENU_MODEL_H_

#include <map>

#include "chrome/common/extensions/extension.h"
#include "content/public/browser/notification_observer.h"
#include "ui/base/models/simple_menu_model.h"

class Profile;

// A menu model that builds the contents of the action box menu. Effectively,
// a ui::SimpleMenuModel with methods specifically for dealing with extension
// content.
//
// This model should be built on demand since its content reflects the state of
// the browser at creation time.
class ActionBoxMenuModel : public ui::SimpleMenuModel {
 public:
  ActionBoxMenuModel(Profile* profile, ui::SimpleMenuModel::Delegate* delegate);
  virtual ~ActionBoxMenuModel();

  // Adds an extension to the model with a given command ID.
  void AddExtension(const extensions::Extension& extension, int command_id);

  // Returns true if item associated with an extension.
  bool IsItemExtension(int index);

  // Returns an extension associated with model item at |index|
  // or NULL if it is not an extension item.
  const extensions::Extension* GetExtensionAt(int index);

  // Calls ExecuteCommand on the delegate.
  void ExecuteCommand(int command_id);

 private:
  friend class ActionBoxMenuModelTest;
  // Gets the index of the first extension. This may be equal to the number of
  // total items in the model if there are no extensions installed.
  int GetFirstExtensionIndex();

  Profile* profile_;

  // The list of extensions added to the menu, in order, if any.
  extensions::ExtensionIdList extension_ids_;

  DISALLOW_COPY_AND_ASSIGN(ActionBoxMenuModel);
};

#endif  // CHROME_BROWSER_UI_TOOLBAR_ACTION_BOX_MENU_MODEL_H_
