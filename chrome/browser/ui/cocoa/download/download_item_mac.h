// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_DOWNLOAD_DOWNLOAD_ITEM_MAC_H_
#define CHROME_BROWSER_UI_COCOA_DOWNLOAD_DOWNLOAD_ITEM_MAC_H_
#pragma once

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/icon_manager.h"
#include "content/browser/cancelable_request.h"
#include "content/browser/download/download_item.h"
#include "content/browser/download/download_manager.h"

class BaseDownloadItemModel;
@class DownloadItemController;

namespace gfx{
class Image;
}

// A class that bridges the visible mac download items to chromium's download
// model. The owning object (DownloadItemController) must explicitly call
// |LoadIcon| if it wants to display the icon associated with this download.

class DownloadItemMac : DownloadItem::Observer {
 public:
  // DownloadItemMac takes ownership of |download_model|.
  DownloadItemMac(BaseDownloadItemModel* download_model,
                  DownloadItemController* controller);

  // Destructor.
  virtual ~DownloadItemMac();

  // DownloadItem::Observer implementation
  virtual void OnDownloadUpdated(DownloadItem* download) OVERRIDE;
  virtual void OnDownloadOpened(DownloadItem* download) OVERRIDE;

  BaseDownloadItemModel* download_model() { return download_model_.get(); }

  // Asynchronous icon loading support.
  void LoadIcon();

 private:
  // Callback for asynchronous icon loading.
  void OnExtractIconComplete(IconManager::Handle handle,
                             gfx::Image* icon_bitmap);

  // The download item model we represent.
  scoped_ptr<BaseDownloadItemModel> download_model_;

  // The objective-c controller object.
  DownloadItemController* item_controller_;  // weak, owns us.

  // For canceling an in progress icon request.
  CancelableRequestConsumerT<int, 0> icon_consumer_;

  // Stores the last known path where the file will be saved.
  FilePath lastFilePath_;

  DISALLOW_COPY_AND_ASSIGN(DownloadItemMac);
};

#endif  // CHROME_BROWSER_UI_COCOA_DOWNLOAD_DOWNLOAD_ITEM_MAC_H_
