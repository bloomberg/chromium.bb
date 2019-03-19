// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/common/simple_download_manager_coordinator.h"

#include <utility>

#include "components/download/public/common/download_item.h"
#include "components/download/public/common/simple_download_manager.h"

namespace download {

SimpleDownloadManagerCoordinator::SimpleDownloadManagerCoordinator() = default;

SimpleDownloadManagerCoordinator::~SimpleDownloadManagerCoordinator() = default;

void SimpleDownloadManagerCoordinator::SetSimpleDownloadManager(
    SimpleDownloadManager* simple_download_manager) {
  simple_download_manager_ = simple_download_manager;
}

void SimpleDownloadManagerCoordinator::DownloadUrl(
    std::unique_ptr<download::DownloadUrlParameters> parameters) {
  if (simple_download_manager_)
    simple_download_manager_->DownloadUrl(std::move(parameters));
}

DownloadItem* SimpleDownloadManagerCoordinator::GetDownloadByGuid(
    const std::string& guid) {
  if (simple_download_manager_)
    return simple_download_manager_->GetDownloadByGuid(guid);
  return nullptr;
}

bool SimpleDownloadManagerCoordinator::AreDownloadsInitialized(
    bool active_downloads_only) {
  return simple_download_manager_->AreDownloadsInitialized(
      active_downloads_only);
}

std::vector<DownloadItem*> SimpleDownloadManagerCoordinator::GetAllDownloads() {
  return simple_download_manager_->GetAllDownloads();
}

}  // namespace download
