// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_STATUS_FEEDBACK_MENU_BUTTON_H_
#define CHROME_BROWSER_CHROMEOS_STATUS_FEEDBACK_MENU_BUTTON_H_
#pragma once

#include "chrome/browser/chromeos/status/status_area_button.h"
#include "views/controls/menu/menu_2.h"
#include "views/controls/menu/view_menu_delegate.h"

class SkBitmap;

namespace chromeos {

class StatusAreaHost;

// The language menu button in the status area.
// This class will handle getting the IME/XKB status and populating the menu.
class FeedbackMenuButton : public StatusAreaButton,
                           public views::ViewMenuDelegate,
                           public menus::MenuModel {
 public:
  explicit FeedbackMenuButton(StatusAreaHost* host);
  virtual ~FeedbackMenuButton();

 private:
  // views::ViewMenuDelegate implementation.
  virtual void RunMenu(views::View* source, const gfx::Point& pt);

  // menus::MenuModel implementation.
  virtual int GetItemCount() const { return 0; }
  virtual bool HasIcons() const  { return false; }
  virtual menus::MenuModel::ItemType GetTypeAt(int index) const {
    return menus::MenuModel::TYPE_COMMAND;
  }
  virtual int GetCommandIdAt(int index) const { return index; }
  virtual string16 GetLabelAt(int index) const { return string16(); }
  virtual bool IsLabelDynamicAt(int index) const { return true; }
  virtual bool GetAcceleratorAt(int index,
      menus::Accelerator* accelerator) const { return false; }
  virtual bool IsItemCheckedAt(int index) const { return false; }
  virtual int GetGroupIdAt(int index) const { return 0; }
  virtual bool GetIconAt(int index, SkBitmap* icon) const { return false; }
  virtual menus::ButtonMenuItemModel* GetButtonMenuItemAt(int index) const {
    return NULL;
  }
  virtual bool IsEnabledAt(int index) const { return false; }
  virtual menus::MenuModel* GetSubmenuModelAt(int index) const { return NULL; }
  virtual void HighlightChangedTo(int index) {}
  virtual void ActivatedAt(int index) {}
  virtual void MenuWillShow() {}

  StatusAreaHost* host_;

  DISALLOW_COPY_AND_ASSIGN(FeedbackMenuButton);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_STATUS_FEEDBACK_MENU_BUTTON_H_
