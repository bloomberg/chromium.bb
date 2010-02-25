// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_SHELF_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_SHELF_H_

#include <string>

#include "app/menus/simple_menu_model.h"
#include "base/basictypes.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"

class BaseDownloadItemModel;
class Browser;
class DownloadItem;

// DownloadShelf is an interface for platform-specific download shelf views.
class DownloadShelf {
 public:
  virtual ~DownloadShelf() { }

  // A new download has started, so add it to our shelf. This object will
  // take ownership of |download_model|. Also make the shelf visible.
  virtual void AddDownload(BaseDownloadItemModel* download_model) = 0;

  // The browser view needs to know when we are going away to properly return
  // the resize corner size to WebKit so that we don't draw on top of it.
  // This returns the showing state of our animation which is set to true at
  // the beginning Show and false at the beginning of a Hide.
  virtual bool IsShowing() const = 0;

  // Returns whether the download shelf is showing the close animation.
  virtual bool IsClosing() const = 0;

  // Opens the shelf.
  virtual void Show() = 0;

  // Closes the shelf.
  virtual void Close() = 0;

  virtual Browser* browser() const = 0;
};

// Logic for the download shelf context menu. Platform specific subclasses are
// responsible for creating and running the menu.
class DownloadShelfContextMenu : public menus::SimpleMenuModel::Delegate {
 public:
  virtual ~DownloadShelfContextMenu();

  virtual DownloadItem* download() const { return download_; }

  enum ContextMenuCommands {
    SHOW_IN_FOLDER = 1,  // Open a file explorer window with the item selected.
    OPEN_WHEN_COMPLETE,  // Open the download when it's finished.
    ALWAYS_OPEN_TYPE,    // Default this file extension to always open.
    CANCEL,              // Cancel the download.
    TOGGLE_PAUSE,        // Temporarily pause a download.
    MENU_LAST
  };

 protected:
  explicit DownloadShelfContextMenu(BaseDownloadItemModel* download_model);

  menus::SimpleMenuModel* GetInProgressMenuModel();
  menus::SimpleMenuModel* GetFinishedMenuModel();
  // Information source.
  DownloadItem* download_;

  // menus::SimpleMenuModel::Delegate implementation:
  virtual bool IsCommandIdEnabled(int command_id) const;
  virtual bool IsCommandIdChecked(int command_id) const;
  virtual void ExecuteCommand(int command_id);
  virtual bool GetAcceleratorForCommandId(int command_id,
                                          menus::Accelerator* accelerator);
  virtual bool IsLabelForCommandIdDynamic(int command_id) const;
  virtual string16 GetLabelForCommandId(int command_id) const;

  // A model to control the cancel behavior.
  BaseDownloadItemModel* model_;

 private:
  // We show slightly different menus if the download is in progress vs. if the
  // download has finished.
  scoped_ptr<menus::SimpleMenuModel> in_progress_download_menu_model_;
  scoped_ptr<menus::SimpleMenuModel> finished_download_menu_model_;

  DISALLOW_COPY_AND_ASSIGN(DownloadShelfContextMenu);
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_SHELF_H_
