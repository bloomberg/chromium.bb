// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ASH_APP_LIST_EXTENSION_APP_ITEM_H_
#define CHROME_BROWSER_UI_VIEWS_ASH_APP_LIST_EXTENSION_APP_ITEM_H_
#pragma once

#include <string>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/image_loading_tracker.h"
#include "chrome/browser/ui/views/ash/app_list/chrome_app_list_item.h"
#include "ui/base/models/simple_menu_model.h"

class Extension;
class ExtensionResource;
class Profile;
class SkBitmap;

// ExtensionAppItem represents an extension app in app list.
class ExtensionAppItem : public ChromeAppListItem,
                         public ImageLoadingTracker::Observer,
                         public ui::SimpleMenuModel::Delegate {
 public:
  ExtensionAppItem(Profile* profile, const Extension* extension);
  virtual ~ExtensionAppItem();

  // Gets extension associated with this model. Returns NULL if extension
  // no longer exists.
  const Extension* GetExtension() const;

  const std::string& extension_id() const {
    return extension_id_;
  }

 private:
  // Loads extension icon.
  void LoadImage(const Extension* extension);

  void ShowExtensionOptions();
  void StartExtensionUninstall();

  // Overridden from ImageLoadingTracker::Observer:
  virtual void OnImageLoaded(const gfx::Image& image,
                             const std::string& extension_id,
                             int tracker_index) OVERRIDE;

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

  scoped_ptr<ImageLoadingTracker> tracker_;
  scoped_ptr<ui::SimpleMenuModel> context_menu_model_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionAppItem);
};

#endif  // CHROME_BROWSER_UI_VIEWS_ASH_APP_LIST_EXTENSION_APP_ITEM_H_
