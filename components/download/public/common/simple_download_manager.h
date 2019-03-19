// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_COMMON_SIMPLE_DOWNLOAD_MANAGER_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_COMMON_SIMPLE_DOWNLOAD_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "components/download/public/common/download_export.h"
#include "components/download/public/common/download_url_parameters.h"

namespace download {

class DownloadItem;

// Download manager that provides simple functionalities for callers to carry
// out a download task.
class COMPONENTS_DOWNLOAD_EXPORT SimpleDownloadManager {
 public:
  class Observer {
   public:
    Observer() = default;
    virtual ~Observer() = default;

    virtual void OnDownloadsInitialized(bool active_downloads_only) {}
    virtual void OnManagerGoingDown() {}
    virtual void OnDownloadCreated(DownloadItem* item) {}
    virtual void OnDownloadUpdated(DownloadItem* item) {}
    virtual void OnDownloadOpened(DownloadItem* item) {}
    virtual void OnDownloadRemoved(DownloadItem* item) {}

   private:
    DISALLOW_COPY_AND_ASSIGN(Observer);
  };

  // See DownloadUrlParameters for details about controlling the download.
  virtual void DownloadUrl(
      std::unique_ptr<download::DownloadUrlParameters> parameters) = 0;

  // Whether downloads are initialized. If |active_downloads_only| is true, this
  // call only checks for all the active downloads. Otherwise, it will check all
  // downloads.
  virtual bool AreDownloadsInitialized(bool active_downloads_only) = 0;

  // Gets all downloads.
  virtual std::vector<DownloadItem*> GetAllDownloads() = 0;

  // Get the download item for |guid|.
  virtual download::DownloadItem* GetDownloadByGuid(
      const std::string& guid) = 0;
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_COMMON_SIMPLE_DOWNLOAD_MANAGER_H_
