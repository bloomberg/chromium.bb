// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_SHELF_CONTEXT_MENU_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_SHELF_CONTEXT_MENU_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "ui/base/models/simple_menu_model.h"

class BaseDownloadItemModel;
class DownloadItem;

// This class is responsible for the download shelf context menu. Platform
// specific subclasses are responsible for creating and running the menu.
class DownloadShelfContextMenu : public ui::SimpleMenuModel::Delegate {
 public:
  enum ContextMenuCommands {
    SHOW_IN_FOLDER = 1,  // Open a file explorer window with the item selected.
    OPEN_WHEN_COMPLETE,  // Open the download when it's finished.
    ALWAYS_OPEN_TYPE,    // Default this file extension to always open.
    CANCEL,              // Cancel the download.
    TOGGLE_PAUSE,        // Temporarily pause a download.
    DISCARD,             // Discard the malicious download.
    KEEP,                // Keep the malicious download.
    LEARN_MORE,          // Show information about download scanning.
    MENU_LAST
  };

  virtual ~DownloadShelfContextMenu();

  DownloadItem* download_item() const { return download_item_; }
  void set_download_item(DownloadItem* item) { download_item_ = item; }

 protected:
  explicit DownloadShelfContextMenu(BaseDownloadItemModel* download_model);

  // Returns the correct menu model depending whether the download item is
  // completed or not.
  ui::SimpleMenuModel* GetMenuModel();

  // ui::SimpleMenuModel::Delegate:
  virtual bool IsCommandIdEnabled(int command_id) const OVERRIDE;
  virtual bool IsCommandIdChecked(int command_id) const OVERRIDE;
  virtual void ExecuteCommand(int command_id) OVERRIDE;
  virtual bool GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) OVERRIDE;
  virtual bool IsItemForCommandIdDynamic(int command_id) const OVERRIDE;
  virtual string16 GetLabelForCommandId(int command_id) const OVERRIDE;

 private:
  ui::SimpleMenuModel* GetInProgressMenuModel();
  ui::SimpleMenuModel* GetFinishedMenuModel();
  ui::SimpleMenuModel* GetMaliciousMenuModel();

  // We show slightly different menus if the download is in progress vs. if the
  // download has finished.
  scoped_ptr<ui::SimpleMenuModel> in_progress_download_menu_model_;
  scoped_ptr<ui::SimpleMenuModel> finished_download_menu_model_;
  scoped_ptr<ui::SimpleMenuModel> malicious_download_menu_model_;

  // A model to control the cancel behavior.
  BaseDownloadItemModel* download_model_;

  // Information source.
  DownloadItem* download_item_;

  DISALLOW_COPY_AND_ASSIGN(DownloadShelfContextMenu);
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_SHELF_CONTEXT_MENU_H_
