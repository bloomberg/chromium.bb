// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_CONTENT_INTERNAL_DOWNLOAD_DRIVER_IMPL_H_
#define COMPONENTS_DOWNLOAD_CONTENT_INTERNAL_DOWNLOAD_DRIVER_IMPL_H_

#include <string>

#include "base/files/file_path.h"
#include "components/download/internal/download_driver.h"
#include "components/download/public/download_params.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_manager.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace download {

struct DriverEntry;

// Aggregates and handles all interaction between download service and content
// download logic.
class DownloadDriverImpl : public DownloadDriver,
                           public content::DownloadManager::Observer,
                           public content::DownloadItem::Observer {
 public:
  // Creates a driver entry based on a download item.
  static DriverEntry CreateDriverEntry(const content::DownloadItem* item);

  // Create the driver. All files downloaded will be saved to |dir|.
  DownloadDriverImpl(content::DownloadManager* manager,
                     const base::FilePath& dir);
  ~DownloadDriverImpl() override;

  // DownloadDriver implementation.
  void Initialize(DownloadDriver::Client* client) override;
  bool IsReady() const override;
  void Start(
      const RequestParams& request_params,
      const std::string& guid,
      const net::NetworkTrafficAnnotationTag& traffic_annotation) override;
  void Remove(const std::string& guid) override;
  void Pause(const std::string& guid) override;
  void Resume(const std::string& guid) override;
  base::Optional<DriverEntry> Find(const std::string& guid) override;
  std::set<std::string> GetActiveDownloads() override;

 private:
  // content::DownloadItem::Observer implementation.
  void OnDownloadUpdated(content::DownloadItem* item) override;

  // content::DownloadManager::Observer implementation.
  void OnDownloadCreated(content::DownloadManager* manager,
                         content::DownloadItem* item) override;
  void OnManagerInitialized() override;
  void ManagerGoingDown(content::DownloadManager* manager) override;

  // Low level download handle.
  content::DownloadManager* download_manager_;

  // Target directory of download files.
  base::FilePath file_dir_;

  // The client that receives updates from low level download logic.
  DownloadDriver::Client* client_;

  DISALLOW_COPY_AND_ASSIGN(DownloadDriverImpl);
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_CONTENT_INTERNAL_DOWNLOAD_DRIVER_IMPL_H_
