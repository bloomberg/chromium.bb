// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_OPTIONS_MENU_MODEL_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_OPTIONS_MENU_MODEL_H_
#pragma once

#include "app/menus/simple_menu_model.h"
#include "chrome/browser/notifications/balloon.h"

class NotificationOptionsMenuModel : public menus::SimpleMenuModel,
                                     public menus::SimpleMenuModel::Delegate {
 public:
  explicit NotificationOptionsMenuModel(Balloon* balloon);
  virtual ~NotificationOptionsMenuModel();

  // Overridden from menus::SimpleMenuModel::Delegate:
  virtual bool IsCommandIdChecked(int command_id) const;
  virtual bool IsCommandIdEnabled(int command_id) const;
  virtual bool GetAcceleratorForCommandId(int command_id,
                                          menus::Accelerator* accelerator);
  virtual void ExecuteCommand(int command_id);

 private:
  Balloon* balloon_; // Not owned.

  DISALLOW_COPY_AND_ASSIGN(NotificationOptionsMenuModel);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_OPTIONS_MENU_MODEL_H_
