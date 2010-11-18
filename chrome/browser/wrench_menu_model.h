// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WRENCH_MENU_MODEL_H_
#define CHROME_BROWSER_WRENCH_MENU_MODEL_H_
#pragma once

#include "app/menus/accelerator.h"
#include "app/menus/button_menu_item_model.h"
#include "app/menus/simple_menu_model.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/tabs/tab_strip_model_observer.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"

class Browser;
class TabStripModel;

namespace {
class MockWrenchMenuModel;
}  // namespace

// A menu model that builds the contents of an encoding menu.
class EncodingMenuModel : public menus::SimpleMenuModel,
                          public menus::SimpleMenuModel::Delegate {
 public:
  explicit EncodingMenuModel(Browser* browser);
  virtual ~EncodingMenuModel();

  // Overridden from menus::SimpleMenuModel::Delegate:
  virtual bool IsCommandIdChecked(int command_id) const;
  virtual bool IsCommandIdEnabled(int command_id) const;
  virtual bool GetAcceleratorForCommandId(int command_id,
                                          menus::Accelerator* accelerator);
  virtual void ExecuteCommand(int command_id);

 private:
  void Build();

  Browser* browser_;  // weak

  DISALLOW_COPY_AND_ASSIGN(EncodingMenuModel);
};

// A menu model that builds the contents of the zoom menu.
class ZoomMenuModel : public menus::SimpleMenuModel {
 public:
  explicit ZoomMenuModel(menus::SimpleMenuModel::Delegate* delegate);
  virtual ~ZoomMenuModel();

 private:
  void Build();

  DISALLOW_COPY_AND_ASSIGN(ZoomMenuModel);
};

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
                        public menus::SimpleMenuModel::Delegate,
                        public menus::ButtonMenuItemModel::Delegate,
                        public TabStripModelObserver,
                        public NotificationObserver {
 public:
  WrenchMenuModel(menus::AcceleratorProvider* provider, Browser* browser);
  virtual ~WrenchMenuModel();

  // Overridden for ButtonMenuItemModel::Delegate:
  virtual bool DoesCommandIdDismissMenu(int command_id) const;

  // Overridden for both ButtonMenuItemModel::Delegate and SimpleMenuModel:
  virtual bool IsLabelForCommandIdDynamic(int command_id) const;
  virtual string16 GetLabelForCommandId(int command_id) const;
  virtual void ExecuteCommand(int command_id);
  virtual bool IsCommandIdChecked(int command_id) const;
  virtual bool IsCommandIdEnabled(int command_id) const;
  virtual bool IsCommandIdVisible(int command_id) const;
  virtual bool GetAcceleratorForCommandId(
      int command_id,
      menus::Accelerator* accelerator);

  // Overridden from TabStripModelObserver:
  virtual void TabSelectedAt(TabContentsWrapper* old_contents,
                             TabContentsWrapper* new_contents,
                             int index,
                             bool user_gesture);
  virtual void TabReplacedAt(TabContentsWrapper* old_contents,
                             TabContentsWrapper* new_contents, int index);
  virtual void TabStripModelDeleted();

  // Overridden from NotificationObserver:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Getters.
  Browser* browser() const { return browser_; }

  // Calculates |zoom_label_| in response to a zoom change.
  void UpdateZoomControls();

 private:
  // Testing constructor used for mocking.
  friend class ::MockWrenchMenuModel;
  WrenchMenuModel();

  void Build();

  // Adds custom items to the menu. Deprecated in favor of a cross platform
  // model for button items.
  void CreateCutCopyPaste();
  void CreateZoomFullscreen();

  string16 GetSyncMenuLabel() const;

  // Models for the special menu items with buttons.
  scoped_ptr<menus::ButtonMenuItemModel> edit_menu_item_model_;
  scoped_ptr<menus::ButtonMenuItemModel> zoom_menu_item_model_;

  // Label of the zoom label in the zoom menu item.
  string16 zoom_label_;

  // Tools menu.
  scoped_ptr<ToolsMenuModel> tools_menu_model_;

  menus::AcceleratorProvider* provider_;  // weak

  Browser* browser_;  // weak
  TabStripModel* tabstrip_model_; // weak

  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(WrenchMenuModel);
};

#endif  // CHROME_BROWSER_WRENCH_MENU_MODEL_H_
