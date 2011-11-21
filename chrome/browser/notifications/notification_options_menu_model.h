// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_OPTIONS_MENU_MODEL_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_OPTIONS_MENU_MODEL_H_
#pragma once

#include "chrome/browser/notifications/balloon.h"
#include "ui/base/models/simple_menu_model.h"

// Model for the corner-selection submenu.
class CornerSelectionMenuModel : public ui::SimpleMenuModel,
                                 public ui::SimpleMenuModel::Delegate {
 public:
  explicit CornerSelectionMenuModel(Balloon* balloon);
  virtual ~CornerSelectionMenuModel();

  // Overridden from ui::SimpleMenuModel::Delegate:
  virtual bool IsCommandIdChecked(int command_id) const OVERRIDE;
  virtual bool IsCommandIdEnabled(int command_id) const OVERRIDE;
  virtual bool GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) OVERRIDE;
  virtual void ExecuteCommand(int command_id) OVERRIDE;

 private:
  // Not owned.
  Balloon* balloon_;

  DISALLOW_COPY_AND_ASSIGN(CornerSelectionMenuModel);
};

// Model for the notification options menu itself.
class NotificationOptionsMenuModel : public ui::SimpleMenuModel,
                                     public ui::SimpleMenuModel::Delegate {
 public:
  explicit NotificationOptionsMenuModel(Balloon* balloon);
  virtual ~NotificationOptionsMenuModel();

  // Overridden from ui::SimpleMenuModel:
  virtual bool IsItemForCommandIdDynamic(int command_id) const OVERRIDE;
  virtual string16 GetLabelForCommandId(int command_id) const OVERRIDE;

  // Overridden from ui::SimpleMenuModel::Delegate:
  virtual bool IsCommandIdChecked(int command_id) const OVERRIDE;
  virtual bool IsCommandIdEnabled(int command_id) const OVERRIDE;
  virtual bool GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) OVERRIDE;
  virtual void ExecuteCommand(int command_id) OVERRIDE;

 private:
  Balloon* balloon_; // Not owned.

  scoped_ptr<CornerSelectionMenuModel> corner_menu_model_;

  DISALLOW_COPY_AND_ASSIGN(NotificationOptionsMenuModel);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_OPTIONS_MENU_MODEL_H_
