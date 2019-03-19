// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_COMMON_SIMPLE_DOWNLOAD_MANAGER_COORDINATOR_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_COMMON_SIMPLE_DOWNLOAD_MANAGER_COORDINATOR_H_

#include <memory>
#include <string>
#include <vector>

#include "components/download/public/common/download_export.h"
#include "components/download/public/common/download_url_parameters.h"

namespace download {

class DownloadItem;
class SimpleDownloadManager;

// This object allows swapping between different SimppleDownloadManager
// instances so that callers don't need to know about the swap.
class COMPONENTS_DOWNLOAD_EXPORT SimpleDownloadManagerCoordinator {
 public:
  SimpleDownloadManagerCoordinator();
  ~SimpleDownloadManagerCoordinator();

  void SetSimpleDownloadManager(SimpleDownloadManager* simple_download_manager);

  // See download::DownloadUrlParameters for details about controlling the
  // download.
  void DownloadUrl(std::unique_ptr<DownloadUrlParameters> parameters);

  // Whether downloads are initialized. If |active_downloads_only| is true, this
  // call only checks for all the active downloads. Otherwise, it will check all
  // downloads.
  bool AreDownloadsInitialized(bool active_downloads_only);

  // Gets all the downloads. Caller needs to call AreDownloadsInitialized() to
  // check if all downloads are initialized. If only active downloads are
  // initialized, this method will only return all active downloads.
  std::vector<DownloadItem*> GetAllDownloads();

  // Get the download item for |guid|.
  download::DownloadItem* GetDownloadByGuid(const std::string& guid);

 private:
  SimpleDownloadManager* simple_download_manager_;

  DISALLOW_COPY_AND_ASSIGN(SimpleDownloadManagerCoordinator);
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_COMMON_SIMPLE_DOWNLOAD_MANAGER_COORDINATOR_H_
