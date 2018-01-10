// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/system_download_manager_stub.h"

namespace offline_pages {

SystemDownloadManagerStub::SystemDownloadManagerStub(int64_t id_to_use,
                                                     bool installed)
    : download_id_(id_to_use), installed_(installed) {}

SystemDownloadManagerStub::~SystemDownloadManagerStub() {}

bool SystemDownloadManagerStub::IsDownloadManagerInstalled() {
  return installed_;
}

int64_t SystemDownloadManagerStub::AddCompletedDownload(
    const std::string& title,
    const std::string& description,
    const std::string& path,
    int64_t length,
    const std::string& uri,
    const std::string& referer) {
  title_ = title;
  description_ = description;
  path_ = path;
  length_ = length;
  uri_ = uri;
  referer_ = referer;

  return download_id_;
}

int SystemDownloadManagerStub::Remove(
    const std::vector<int64_t>& android_download_manager_ids) {
  return android_download_manager_ids.size();
}

}  // namespace offline_pages
