// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/fileapi/external_file_url_util.h"

#include <string>
#include <vector>

#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/chromeos/drive/drive_integration_service.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/file_manager/app_id.h"
#include "chrome/browser/chromeos/file_manager/fileapi_util.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/components/drivefs/mojom/drivefs.mojom.h"
#include "components/drive/drive_api_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/url_constants.h"
#include "net/base/escape.h"
#include "storage/browser/fileapi/file_system_url.h"

using content::BrowserThread;

namespace chromeos {
namespace {

void ExtractHostedFileUrl(base::OnceCallback<void(GURL)> callback,
                          drive::FileError error,
                          drivefs::mojom::FileMetadataPtr metadata) {
  if (error != drive::FILE_ERROR_OK) {
    std::move(callback).Run({});
    return;
  }
  if (metadata->type != drivefs::mojom::FileMetadata::Type::kHosted) {
    std::move(callback).Run({});
    return;
  }
  GURL hosted_url(metadata->alternate_url);
  std::move(callback).Run(hosted_url.is_valid() ? hosted_url : GURL());
}

}  // namespace

bool IsExternalFileURLType(storage::FileSystemType type) {
  return type == storage::kFileSystemTypeDrive ||
         type == storage::kFileSystemTypeDeviceMediaAsFileStorage ||
         type == storage::kFileSystemTypeProvided ||
         type == storage::kFileSystemTypeArcContent;
}

GURL FileSystemURLToExternalFileURL(
    const storage::FileSystemURL& file_system_url) {
  if (file_system_url.mount_type() != storage::kFileSystemTypeExternal ||
      !IsExternalFileURLType(file_system_url.type())) {
    return GURL();
  }

  return VirtualPathToExternalFileURL(file_system_url.virtual_path());
}

base::FilePath ExternalFileURLToVirtualPath(const GURL& url) {
  if (!url.is_valid() || url.scheme() != content::kExternalFileScheme)
    return base::FilePath();
  std::string path_string;
  net::UnescapeBinaryURLComponent(url.path(), &path_string);
  return base::FilePath::FromUTF8Unsafe(path_string);
}

GURL VirtualPathToExternalFileURL(const base::FilePath& virtual_path) {
  return GURL(
      base::StringPrintf("%s:%s", content::kExternalFileScheme,
                         net::EscapePath(virtual_path.AsUTF8Unsafe()).c_str()));
}

GURL CreateExternalFileURLFromPath(Profile* profile,
                                   const base::FilePath& path) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  GURL raw_file_system_url;
  if (!file_manager::util::ConvertAbsoluteFilePathToFileSystemUrl(
          profile,
          path,
          file_manager::kFileManagerAppId,
          &raw_file_system_url)) {
    return GURL();
  }

  const storage::FileSystemURL file_system_url =
      file_manager::util::GetFileSystemContextForExtensionId(
          profile, file_manager::kFileManagerAppId)
          ->CrackURL(raw_file_system_url);
  if (!file_system_url.is_valid())
    return GURL();

  return FileSystemURLToExternalFileURL(file_system_url);
}

void ResolveExternalFileUrlFromPath(Profile* profile,
                                    const base::FilePath& path,
                                    base::OnceCallback<void(GURL)> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  GURL raw_file_system_url;
  if (!file_manager::util::ConvertAbsoluteFilePathToFileSystemUrl(
          profile, path, file_manager::kFileManagerAppId,
          &raw_file_system_url)) {
    std::move(callback).Run({});
    return;
  }

  const storage::FileSystemURL file_system_url =
      file_manager::util::GetFileSystemContextForExtensionId(
          profile, file_manager::kFileManagerAppId)
          ->CrackURL(raw_file_system_url);
  if (!file_system_url.is_valid()) {
    std::move(callback).Run({});
    return;
  }

  auto external_file_url = FileSystemURLToExternalFileURL(file_system_url);
  if (!external_file_url.is_empty()) {
    std::move(callback).Run(std::move(external_file_url));
    return;
  }

  if (file_system_url.type() != storage::kFileSystemTypeDriveFs ||
      !drive::util::HasHostedDocumentExtension(path)) {
    std::move(callback).Run({});
    return;
  }

  drive::DriveIntegrationService* integration_service =
      drive::util::GetIntegrationServiceByProfile(profile);
  base::FilePath mount_relative_path;
  if (!integration_service || !integration_service->GetDriveFsInterface() ||
      !integration_service->GetRelativeDrivePath(path, &mount_relative_path)) {
    std::move(callback).Run({});
    return;
  }
  integration_service->GetDriveFsInterface()->GetMetadata(
      mount_relative_path,
      base::BindOnce(&ExtractHostedFileUrl, std::move(callback)));
}

}  // namespace chromeos
