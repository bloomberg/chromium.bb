// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WRENCH_MENU_MODEL_H_
#define CHROME_BROWSER_WRENCH_MENU_MODEL_H_

#include <set>
#include <vector>

#include "app/menus/button_menu_item_model.h"
#include "app/menus/simple_menu_model.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"

class Browser;
class EncodingMenuModel;

namespace menus {
class ButtonMenuItemModel;
}  // namespace menus

class ToolsMenuModel : public menus::SimpleMenuModel {
 public:
  ToolsMenuModel(menus::SimpleMenuModel::Delegate* delegate, Browser* browser);
  virtual ~ToolsMenuModel();

 private:
  void Build(Browser* browser);

  scoped_ptr<EncodingMenuModel> encoding_menu_model_;

  DISALLOW_COPY_AND_ASSIGN(ToolsMenuModel);
};

// A menu model that builds the contents of the wrench menu.
class WrenchMenuModel : public menus::SimpleMenuModel,
                        public menus::ButtonMenuItemModel::Delegate,
                        public TabStripModelObserver,
                        public NotificationObserver {
 public:
  WrenchMenuModel(menus::SimpleMenuModel::Delegate* delegate,
                  Browser* browser);
  virtual ~WrenchMenuModel();

  // Returns true if the WrenchMenuModel is enabled.
  static bool IsEnabled();

  // Overridden from menus::SimpleMenuModel:
  virtual bool IsLabelDynamicAt(int index) const;
  virtual string16 GetLabelAt(int index) const;
  virtual bool HasIcons() const { return true; }
  virtual bool GetIconAt(int index, SkBitmap* icon) const;

  // Overridden from menus::ButtonMenuItemModel::Delegate:
  virtual bool IsLabelForCommandIdDynamic(int command_id) const;
  virtual string16 GetLabelForCommandId(int command_id) const;
  virtual void ExecuteCommand(int command_id);

  // Overridden from TabStripModelObserver:
  virtual void TabSelectedAt(TabContents* old_contents,
                             TabContents* new_contents,
                             int index,
                             bool user_gesture);
  virtual void TabReplacedAt(TabContents* old_contents,
                             TabContents* new_contents, int index);
  virtual void TabStripModelDeleted();

  // Overridden from NotificationObserver:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  void Build();

  // Adds custom items to the menu. Deprecated in favor of a cross platform
  // model for button items.
  void CreateCutCopyPaste();
  void CreateZoomFullscreen();

  // Calculates |zoom_label_| in response to a zoom change.
  void UpdateZoomControls();
  double GetZoom(bool* enable_increment, bool* enable_decrement);

  string16 GetSyncMenuLabel() const;
  string16 GetAboutEntryMenuLabel() const;
  bool IsDynamicItem(int index) const;

  // Models for the special menu items with buttons.
  scoped_ptr<menus::ButtonMenuItemModel> edit_menu_item_model_;
  scoped_ptr<menus::ButtonMenuItemModel> zoom_menu_item_model_;

  // Label of the zoom label in the zoom menu item.
  string16 zoom_label_;

  // Tools menu.
  scoped_ptr<ToolsMenuModel> tools_menu_model_;

  menus::SimpleMenuModel::Delegate* delegate_; // weak

  Browser* browser_;  // weak
  TabStripModel* tabstrip_model_; // weak

  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(WrenchMenuModel);
};

#endif  // CHROME_BROWSER_WRENCH_MENU_MODEL_H_
