// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/fileapi/recent_drive_source.h"

#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/file_manager/fileapi_util.h"
#include "chrome/browser/chromeos/fileapi/recent_context.h"
#include "chrome/browser/chromeos/fileapi/recent_model.h"
#include "content/public/browser/browser_thread.h"
#include "storage/browser/fileapi/file_system_url.h"
#include "storage/common/fileapi/file_system_types.h"

using content::BrowserThread;

namespace chromeos {

RecentDriveSource::RecentDriveSource(Profile* profile)
    : profile_(profile), weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

RecentDriveSource::~RecentDriveSource() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void RecentDriveSource::GetRecentFiles(RecentContext context,
                                       GetRecentFilesCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  drive::FileSystemInterface* file_system =
      drive::util::GetFileSystemByProfile(profile_);
  if (!file_system) {
    // |file_system| is nullptr if Drive is disabled.
    OnSearchMetadata(std::move(context), std::move(callback),
                     drive::FILE_ERROR_FAILED, nullptr);
    return;
  }

  file_system->SearchMetadata(
      "" /* query */, drive::SEARCH_METADATA_EXCLUDE_DIRECTORIES,
      kMaxFilesFromSingleSource, drive::MetadataSearchOrder::LAST_MODIFIED,
      base::Bind(
          &RecentDriveSource::OnSearchMetadata, weak_ptr_factory_.GetWeakPtr(),
          base::Passed(std::move(context)), base::Passed(std::move(callback))));
}

void RecentDriveSource::OnSearchMetadata(
    RecentContext context,
    GetRecentFilesCallback callback,
    drive::FileError error,
    std::unique_ptr<drive::MetadataSearchResultVector> results) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (error != drive::FILE_ERROR_OK) {
    std::move(callback).Run({});
    return;
  }

  DCHECK(results.get());

  std::string extension_id = context.origin().host();

  std::vector<storage::FileSystemURL> files;

  for (const auto& result : *results) {
    if (result.is_directory)
      continue;

    base::FilePath virtual_path =
        file_manager::util::ConvertDrivePathToRelativeFileSystemPath(
            profile_, extension_id, result.path);
    storage::FileSystemURL file =
        context.file_system_context()->CreateCrackedFileSystemURL(
            context.origin(), storage::kFileSystemTypeExternal, virtual_path);
    files.emplace_back(file);
  }

  std::move(callback).Run(std::move(files));
}

}  // namespace chromeos
