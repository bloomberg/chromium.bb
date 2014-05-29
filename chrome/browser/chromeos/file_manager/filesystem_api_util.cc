// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/filesystem_api_util.h"

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/drive/file_errors.h"
#include "chrome/browser/chromeos/drive/file_system_interface.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "content/public/browser/browser_thread.h"

namespace file_manager {
namespace util {

namespace {

void GetMimeTypeAfterGetResourceEntry(
    const base::Callback<void(bool, const std::string&)>& callback,
    drive::FileError error,
    scoped_ptr<drive::ResourceEntry> entry) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (error != drive::FILE_ERROR_OK || !entry->has_file_specific_info()) {
    callback.Run(false, std::string());
    return;
  }
  callback.Run(true, entry->file_specific_info().content_mime_type());
}

void CheckDirectoryAfterDriveCheck(const base::Callback<void(bool)>& callback,
                                   drive::FileError error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  return callback.Run(error == drive::FILE_ERROR_OK);
}

void CheckWritableAfterDriveCheck(const base::Callback<void(bool)>& callback,
                                  drive::FileError error,
                                  const base::FilePath& local_path) {
  // This is called on the IO-allowed blocking pool. Call back to UI.
  content::BrowserThread::PostTask(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(callback, error == drive::FILE_ERROR_OK));
}

}  // namespace

bool IsUnderNonNativeLocalPath(Profile* profile,
                        const base::FilePath& path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // TODO(kinaba): support other types of volumes besides Drive.
  return drive::util::IsUnderDriveMountPoint(path);
}

void GetNonNativeLocalPathMimeType(
    Profile* profile,
    const base::FilePath& path,
    const base::Callback<void(bool, const std::string&)>& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // TODO(kinaba): support other types of volumes besides Drive.
  drive::FileSystemInterface* file_system =
      drive::util::GetFileSystemByProfile(profile);
  if (!file_system) {
    content::BrowserThread::PostTask(
        content::BrowserThread::UI,
        FROM_HERE,
        base::Bind(callback, false, std::string()));
    return;
  }

  file_system->GetResourceEntry(
      drive::util::ExtractDrivePath(path),
      base::Bind(&GetMimeTypeAfterGetResourceEntry, callback));
}

void IsNonNativeLocalPathDirectory(
    Profile* profile,
    const base::FilePath& path,
    const base::Callback<void(bool)>& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // TODO(kinaba): support other types of volumes besides Drive.
  drive::util::CheckDirectoryExists(
      profile,
      path,
      base::Bind(&CheckDirectoryAfterDriveCheck, callback));
}

void PrepareNonNativeLocalPathWritableFile(
    Profile* profile,
    const base::FilePath& path,
    const base::Callback<void(bool)>& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // TODO(kinaba): support other types of volumes besides Drive.
  drive::util::PrepareWritableFileAndRun(
      profile,
      path,
      base::Bind(&CheckWritableAfterDriveCheck, callback));
}

}  // namespace util
}  // namespace file_manager
