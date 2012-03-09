// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_GDATA_DOWNLOAD_OBSERVER_H__
#define CHROME_BROWSER_CHROMEOS_GDATA_GDATA_DOWNLOAD_OBSERVER_H__
#pragma once

#include <set>

#include "content/public/browser/download_item.h"
#include "content/public/browser/download_manager.h"

namespace gdata {

class GDataUploader;

// Observes downloads to temporary local gdata folder. Schedules these
// downloads for upload to gdata service.
class GDataDownloadObserver : public content::DownloadManager::Observer,
                              public content::DownloadItem::Observer {
 public:
  GDataDownloadObserver();
  virtual ~GDataDownloadObserver();

  // Become an observer of  DownloadManager.
  void Initialize(GDataUploader* gdata_uploader,
                  content::DownloadManager* download_manager);

 private:
  // DownloadManager overrides.
  virtual void ManagerGoingDown(content::DownloadManager* manager) OVERRIDE;
  virtual void ModelChanged(content::DownloadManager* manager) OVERRIDE;

  // DownloadItem overrides.
  virtual void OnDownloadUpdated(content::DownloadItem* download) OVERRIDE;
  virtual void OnDownloadOpened(content::DownloadItem* download) OVERRIDE {}

  // Adds/Removes |download| to pending_downloads_.
  // Also start/stop observing |download|.
  void AddPendingDownload(content::DownloadItem* download);
  void RemovePendingDownload(content::DownloadItem* download);

  // Starts the upload of a downloaded/downloading file.
  void UploadDownloadItem(content::DownloadItem* download);

  // Updates metadata of ongoing upload if it exists.
  void UpdateUpload(content::DownloadItem* download);

  // Checks if this DownloadItem should be uploaded.
  bool ShouldUpload(content::DownloadItem* download);

  // Private data.
  // Use GDataUploader to trigger file uploads.
  GDataUploader* gdata_uploader_;
  // Observe the DownloadManager for new downloads.
  content::DownloadManager* download_manager_;

  typedef std::set<content::DownloadItem*> DownloadSet;
  DownloadSet pending_downloads_;

  DISALLOW_COPY_AND_ASSIGN(GDataDownloadObserver);
};

}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_GDATA_DOWNLOAD_OBSERVER_H__
