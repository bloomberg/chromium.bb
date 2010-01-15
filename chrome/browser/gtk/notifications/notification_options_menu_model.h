// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_NOTIFICATIONS_NOTIFICATION_OPTIONS_MENU_MODEL_H_
#define CHROME_BROWSER_GTK_NOTIFICATIONS_NOTIFICATION_OPTIONS_MENU_MODEL_H_

#include "app/menus/menu_model.h"

class Balloon;

// Model for the options menu on the notification balloon.
class NotificationOptionsMenuModel : public menus::MenuModel {
 public:
  explicit NotificationOptionsMenuModel(Balloon* balloon);
  ~NotificationOptionsMenuModel();

  // menus::MenuModel methods.
  virtual bool HasIcons() const;
  virtual int GetItemCount() const;
  virtual ItemType GetTypeAt(int index) const;
  virtual int GetCommandIdAt(int index) const;
  virtual string16 GetLabelAt(int index) const;
  virtual bool IsLabelDynamicAt(int index) const;
  virtual bool GetAcceleratorAt(int index,
                                menus::Accelerator* accelerator) const;
  virtual bool IsItemCheckedAt(int index) const;
  virtual int GetGroupIdAt(int index) const;
  virtual bool GetIconAt(int index, SkBitmap* icon) const;
  virtual bool IsEnabledAt(int index) const;
  virtual MenuModel* GetSubmenuModelAt(int index) const;
  virtual void HighlightChangedTo(int index);
  virtual void ActivatedAt(int index);

 private:
  // Non-owned pointer to the balloon involved.
  Balloon* balloon_;

  DISALLOW_COPY_AND_ASSIGN(NotificationOptionsMenuModel);
};

#endif  // CHROME_BROWSER_GTK_NOTIFICATIONS_NOTIFICATION_OPTIONS_MENU_MODEL_H_
