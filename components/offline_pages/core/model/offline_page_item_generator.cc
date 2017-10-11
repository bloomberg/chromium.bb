// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/model/offline_page_item_generator.h"

#include "base/files/file_util.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "components/offline_pages/core/offline_store_utils.h"
#include "url/gurl.h"

namespace offline_pages {

OfflinePageItemGenerator::OfflinePageItemGenerator() = default;

OfflinePageItemGenerator::~OfflinePageItemGenerator() {}

OfflinePageItem OfflinePageItemGenerator::CreateItem() {
  OfflinePageItem item;
  item.offline_id = store_utils::GenerateOfflineId();
  item.client_id.name_space = namespace_;
  item.client_id.id = base::Int64ToString(item.offline_id);
  item.request_origin = request_origin_;
  item.url = url_;
  item.original_url = original_url_;
  item.file_size = file_size_;
  item.last_access_time = last_access_time_;
  return item;
}

OfflinePageItem OfflinePageItemGenerator::CreateItemWithTempFile() {
  // If hitting this DCHECK, please call SetArchiveDirectory before calling
  // this method for creating files with page.
  DCHECK(!archive_dir_.empty());
  OfflinePageItem item = CreateItem();
  base::FilePath path;
  base::CreateTemporaryFileInDir(archive_dir_, &path);
  item.file_path = path;
  return item;
}

void OfflinePageItemGenerator::SetNamespace(const std::string& name_space) {
  namespace_ = name_space;
}

void OfflinePageItemGenerator::SetRequestOrigin(
    const std::string& request_origin) {
  request_origin_ = request_origin;
}

void OfflinePageItemGenerator::SetUrl(const GURL& url) {
  url_ = url;
}

void OfflinePageItemGenerator::SetOriginalUrl(const GURL& url) {
  original_url_ = url;
}

void OfflinePageItemGenerator::SetFileSize(int64_t file_size) {
  file_size_ = file_size;
}

void OfflinePageItemGenerator::SetLastAccessTime(
    const base::Time& last_access_time) {
  last_access_time_ = last_access_time;
}

void OfflinePageItemGenerator::SetArchiveDirectory(
    const base::FilePath& archive_dir) {
  archive_dir_ = archive_dir;
}

}  // namespace offline_pages
