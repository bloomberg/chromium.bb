// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORD_MENU_MODEL_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORD_MENU_MODEL_H_

#include <map>
#include <string>

#include "ui/base/models/simple_menu_model.h"

class ContentSettingBubbleModel;
class ContentSettingBubbleContents;

// A menu model that builds the contents of the password menu in the content
// setting bubble.
class PasswordMenuModel : public ui::SimpleMenuModel,
                          public ui::SimpleMenuModel::Delegate {
 public:
  PasswordMenuModel(
      ContentSettingBubbleModel* bubble_model,
      ContentSettingBubbleContents* bubble_contents);
  virtual ~PasswordMenuModel();

  // ui::SimpleMenuModel::Delegate:
  virtual bool IsCommandIdChecked(int command_id) const OVERRIDE;
  virtual bool IsCommandIdEnabled(int command_id) const OVERRIDE;
  virtual bool GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) OVERRIDE;
  virtual void ExecuteCommand(int command_id, int event_flags) OVERRIDE;

 private:
  enum CommandID {
    COMMAND_ID_NOPE,
    COMMAND_ID_NEVER_FOR_THIS_SITE,
  };

  ContentSettingBubbleModel* media_bubble_model_;        // Weak.
  ContentSettingBubbleContents* media_bubble_contents_;  // Weak.

  DISALLOW_COPY_AND_ASSIGN(PasswordMenuModel);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORD_MENU_MODEL_H_
