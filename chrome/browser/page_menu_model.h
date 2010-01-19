// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef CHROME_BROWSER_PAGE_MENU_MODEL_H_
#define CHROME_BROWSER_PAGE_MENU_MODEL_H_

#include "app/menus/simple_menu_model.h"
#include "base/scoped_ptr.h"

class Browser;

// A menu model that builds the contents of an encoding menu.
class EncodingMenuModel : public menus::SimpleMenuModel,
                          public menus::SimpleMenuModel::Delegate {
 public:
  explicit EncodingMenuModel(Browser* browser);
  virtual ~EncodingMenuModel() {}

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
  virtual ~ZoomMenuModel() {}

 private:
  void Build();

  DISALLOW_COPY_AND_ASSIGN(ZoomMenuModel);
};

// A menu model that builds the contents of the dev tools menu.
class DevToolsMenuModel : public menus::SimpleMenuModel {
 public:
  explicit DevToolsMenuModel(menus::SimpleMenuModel::Delegate* delegate);
  virtual ~DevToolsMenuModel() {}

 private:
  void Build();

  DISALLOW_COPY_AND_ASSIGN(DevToolsMenuModel);
};

// A menu model that builds the contents of the page menu and all of its
// submenus.
class PageMenuModel : public menus::SimpleMenuModel {
 public:
  explicit PageMenuModel(menus::SimpleMenuModel::Delegate* delegate,
                         Browser* browser);
  virtual ~PageMenuModel() { }

 private:
  void Build();

  // Models for submenus referenced by this model. SimpleMenuModel only uses
  // weak references so these must be kept for the lifetime of the top-level
  // model.
  scoped_ptr<ZoomMenuModel> zoom_menu_model_;
  scoped_ptr<EncodingMenuModel> encoding_menu_model_;
  scoped_ptr<DevToolsMenuModel> devtools_menu_model_;
  Browser* browser_;  // weak

  DISALLOW_COPY_AND_ASSIGN(PageMenuModel);
};

// A menu model that builds the contents of the menu shown for popups (when the
// user clicks on the favicon) and all of its submenus.
class PopupPageMenuModel : public menus::SimpleMenuModel {
 public:
  explicit PopupPageMenuModel(menus::SimpleMenuModel::Delegate* delegate,
                              Browser* browser);
  virtual ~PopupPageMenuModel() { }

 private:
  void Build();

  // Models for submenus referenced by this model. SimpleMenuModel only uses
  // weak references so these must be kept for the lifetime of the top-level
  // model.
  scoped_ptr<ZoomMenuModel> zoom_menu_model_;
  scoped_ptr<EncodingMenuModel> encoding_menu_model_;
  Browser* browser_;  // weak

  DISALLOW_COPY_AND_ASSIGN(PopupPageMenuModel);
};

#endif  // CHROME_BROWSER_PAGE_MENU_MODEL_H_
