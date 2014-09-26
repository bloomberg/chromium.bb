// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/fileapi/external_file_url_util.h"

#include <string>
#include <vector>

#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/chromeos/file_manager/app_id.h"
#include "chrome/browser/chromeos/file_manager/fileapi_util.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/escape.h"
#include "storage/browser/fileapi/file_system_url.h"

using content::BrowserThread;

namespace chromeos {

GURL FileSystemURLToExternalFileURL(
    const storage::FileSystemURL& file_system_url) {
  if (file_system_url.mount_type() != storage::kFileSystemTypeExternal)
    return GURL();

  switch (file_system_url.type()) {
    case storage::kFileSystemTypeDrive:
    case storage::kFileSystemTypeDeviceMediaAsFileStorage:
    case storage::kFileSystemTypeProvided:
      return GURL(base::StringPrintf(
          "%s:%s",
          chrome::kExternalFileScheme,
          file_system_url.virtual_path().AsUTF8Unsafe().c_str()));

    default:
      return GURL();
  }
}

base::FilePath ExternalFileURLToVirtualPath(const GURL& url) {
  if (!url.is_valid() || url.scheme() != chrome::kExternalFileScheme)
    return base::FilePath();
  const std::string path_string =
      net::UnescapeURLComponent(url.GetContent(), net::UnescapeRule::NORMAL);
  return base::FilePath::FromUTF8Unsafe(path_string);
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

}  // namespace chromeos
