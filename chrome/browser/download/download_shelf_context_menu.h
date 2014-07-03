// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_SHELF_CONTEXT_MENU_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_SHELF_CONTEXT_MENU_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "content/public/browser/download_item.h"
#include "ui/base/models/simple_menu_model.h"

namespace content {
class PageNavigator;
}

// This class is responsible for the download shelf context menu. Platform
// specific subclasses are responsible for creating and running the menu.
//
// The DownloadItem corresponding to the context menu is observed for removal or
// destruction.
class DownloadShelfContextMenu : public ui::SimpleMenuModel::Delegate,
                                 public content::DownloadItem::Observer {
 public:
  enum ContextMenuCommands {
    SHOW_IN_FOLDER = 1,    // Open a folder view window with the item selected.
    OPEN_WHEN_COMPLETE,    // Open the download when it's finished.
    ALWAYS_OPEN_TYPE,      // Default this file extension to always open.
    PLATFORM_OPEN,         // Open using platform handler.
    CANCEL,                // Cancel the download.
    TOGGLE_PAUSE,          // Pause or resume a download.
    DISCARD,               // Discard the malicious download.
    KEEP,                  // Keep the malicious download.
    LEARN_MORE_SCANNING,   // Show information about download scanning.
    LEARN_MORE_INTERRUPTED,// Show information about interrupted downloads.
  };

  virtual ~DownloadShelfContextMenu();

  content::DownloadItem* download_item() const { return download_item_; }

 protected:
  DownloadShelfContextMenu(content::DownloadItem* download_item,
                           content::PageNavigator* navigator);

  // Returns the correct menu model depending on the state of the download item.
  // Returns NULL if the download was destroyed.
  ui::SimpleMenuModel* GetMenuModel();

  // ui::SimpleMenuModel::Delegate:
  virtual bool IsCommandIdEnabled(int command_id) const OVERRIDE;
  virtual bool IsCommandIdChecked(int command_id) const OVERRIDE;
  virtual bool IsCommandIdVisible(int command_id) const OVERRIDE;
  virtual void ExecuteCommand(int command_id, int event_flags) OVERRIDE;
  virtual bool GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) OVERRIDE;
  virtual bool IsItemForCommandIdDynamic(int command_id) const OVERRIDE;
  virtual base::string16 GetLabelForCommandId(int command_id) const OVERRIDE;

 private:
  // Detaches self from |download_item_|. Called when the DownloadItem is
  // destroyed or when this object is being destroyed.
  void DetachFromDownloadItem();

  // content::DownloadItem::Observer
  virtual void OnDownloadDestroyed(content::DownloadItem* download) OVERRIDE;

  ui::SimpleMenuModel* GetInProgressMenuModel();
  ui::SimpleMenuModel* GetFinishedMenuModel();
  ui::SimpleMenuModel* GetInterruptedMenuModel();
  ui::SimpleMenuModel* GetMaybeMaliciousMenuModel();
  ui::SimpleMenuModel* GetMaliciousMenuModel();

  int GetAlwaysOpenStringId() const;

#if defined(OS_WIN)
  bool IsDownloadPdf() const;
  bool CanOpenPdfInReader() const;
#endif

  // We show slightly different menus if the download is in progress vs. if the
  // download has finished.
  scoped_ptr<ui::SimpleMenuModel> in_progress_download_menu_model_;
  scoped_ptr<ui::SimpleMenuModel> finished_download_menu_model_;
  scoped_ptr<ui::SimpleMenuModel> interrupted_download_menu_model_;
  scoped_ptr<ui::SimpleMenuModel> maybe_malicious_download_menu_model_;
  scoped_ptr<ui::SimpleMenuModel> malicious_download_menu_model_;

  // Information source.
  content::DownloadItem* download_item_;

  // Used to open tabs.
  content::PageNavigator* navigator_;

#if defined(OS_WIN)
  bool is_pdf_reader_up_to_date_;
#endif

  DISALLOW_COPY_AND_ASSIGN(DownloadShelfContextMenu);
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_SHELF_CONTEXT_MENU_H_
