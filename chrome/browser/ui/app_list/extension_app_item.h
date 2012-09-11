// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_EXTENSION_APP_ITEM_H_
#define CHROME_BROWSER_UI_APP_LIST_EXTENSION_APP_ITEM_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/extension_icon_image.h"
#include "chrome/browser/ui/app_list/chrome_app_list_item.h"
#include "ui/base/models/simple_menu_model.h"

class AppListController;
class ExtensionResource;
class Profile;
class SkBitmap;

namespace extensions {
class Extension;
}

// ExtensionAppItem represents an extension app in app list.
class ExtensionAppItem : public ChromeAppListItem,
                         public extensions::IconImage::Observer,
                         public ui::SimpleMenuModel::Delegate {
 public:
  ExtensionAppItem(Profile* profile,
                   const extensions::Extension* extension,
                   AppListController* controller);
  virtual ~ExtensionAppItem();

  // Gets extension associated with this model. Returns NULL if extension
  // no longer exists.
  const extensions::Extension* GetExtension() const;

  const std::string& extension_id() const {
    return extension_id_;
  }

 private:
  // Returns true if this item represents a version of the Talk extension.
  bool IsTalkExtension() const;

  // Loads extension icon.
  void LoadImage(const extensions::Extension* extension);

  void ShowExtensionOptions();
  void ShowExtensionDetails();
  void StartExtensionUninstall();

  // Overridden from extensions::IconImage::Observer:
  virtual void OnExtensionIconImageChanged(
      extensions::IconImage* image) OVERRIDE;

  // Overridden from ui::SimpleMenuModel::Delegate:
  virtual bool IsItemForCommandIdDynamic(int command_id) const OVERRIDE;
  virtual string16 GetLabelForCommandId(int command_id) const OVERRIDE;
  virtual bool IsCommandIdChecked(int command_id) const OVERRIDE;
  virtual bool IsCommandIdEnabled(int command_id) const OVERRIDE;
  virtual bool GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* acclelrator) OVERRIDE;
  virtual void ExecuteCommand(int command_id) OVERRIDE;

  // Overridden from ChromeAppListItem:
  virtual void Activate(int event_flags) OVERRIDE;
  virtual ui::MenuModel* GetContextMenuModel() OVERRIDE;

  Profile* profile_;
  const std::string extension_id_;
  AppListController* controller_;

  scoped_ptr<extensions::IconImage> icon_;
  scoped_ptr<ui::SimpleMenuModel> context_menu_model_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionAppItem);
};

#endif  // CHROME_BROWSER_UI_APP_LIST_EXTENSION_APP_ITEM_H_
