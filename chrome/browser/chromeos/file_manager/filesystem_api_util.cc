// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/filesystem_api_util.h"

#include <memory>

#include "base/callback.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_content_file_system_url_util.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_file_system_operation_runner.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/file_manager/app_id.h"
#include "chrome/browser/chromeos/file_manager/fileapi_util.h"
#include "chrome/browser/chromeos/file_system_provider/mount_path_util.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_interface.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "components/arc/arc_service_manager.h"
#include "components/drive/chromeos/file_system_interface.h"
#include "components/drive/file_errors.h"
#include "components/drive/file_system_core_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "google_apis/drive/task_util.h"
#include "storage/browser/fileapi/file_system_context.h"

namespace file_manager {
namespace util {
namespace {

// Helper function used to implement GetNonNativeLocalPathMimeType. It extracts
// the mime type from the passed Drive resource entry.
void GetMimeTypeAfterGetResourceEntryForDrive(
    const base::Callback<void(bool, const std::string&)>& callback,
    drive::FileError error,
    std::unique_ptr<drive::ResourceEntry> entry) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (error != drive::FILE_ERROR_OK || !entry->has_file_specific_info() ||
      entry->file_specific_info().content_mime_type().empty()) {
    callback.Run(false, std::string());
    return;
  }
  callback.Run(true, entry->file_specific_info().content_mime_type());
}

// Helper function used to implement GetNonNativeLocalPathMimeType. It extracts
// the mime type from the passed metadata from a providing extension.
void GetMimeTypeAfterGetMetadataForProvidedFileSystem(
    const base::Callback<void(bool, const std::string&)>& callback,
    std::unique_ptr<chromeos::file_system_provider::EntryMetadata> metadata,
    base::File::Error result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (result != base::File::FILE_OK || !metadata->mime_type.get()) {
    callback.Run(false, std::string());
    return;
  }
  callback.Run(true, *metadata->mime_type);
}

// Helper function used to implement GetNonNativeLocalPathMimeType. It passes
// the returned mime type to the callback.
void GetMimeTypeAfterGetMimeTypeForArcContentFileSystem(
    const base::Callback<void(bool, const std::string&)>& callback,
    const base::Optional<std::string>& mime_type) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (mime_type.has_value()) {
    callback.Run(true, mime_type.value());
  } else {
    callback.Run(false, std::string());
  }
}

// Helper function to converts a callback that takes boolean value to that takes
// File::Error, by regarding FILE_OK as the only successful value.
void BoolCallbackAsFileErrorCallback(
    const base::Callback<void(bool)>& callback,
    base::File::Error error) {
  return callback.Run(error == base::File::FILE_OK);
}

// Part of PrepareFileOnIOThread. It tries to create a new file if the given
// |url| is not already inhabited.
void PrepareFileAfterCheckExistOnIOThread(
    scoped_refptr<storage::FileSystemContext> file_system_context,
    const storage::FileSystemURL& url,
    const storage::FileSystemOperation::StatusCallback& callback,
    base::File::Error error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (error != base::File::FILE_ERROR_NOT_FOUND) {
    callback.Run(error);
    return;
  }

  // Call with the second argument |exclusive| set to false, meaning that it
  // is not an error even if the file already exists (it can happen if the file
  // is created after the previous FileExists call and before this CreateFile.)
  //
  // Note that the preceding call to FileExists is necessary for handling
  // read only filesystems that blindly rejects handling CreateFile().
  file_system_context->operation_runner()->CreateFile(url, false, callback);
}

// Checks whether a file exists at the given |url|, and try creating it if it
// is not already there.
void PrepareFileOnIOThread(
    scoped_refptr<storage::FileSystemContext> file_system_context,
    const storage::FileSystemURL& url,
    const base::Callback<void(bool)>& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  file_system_context->operation_runner()->FileExists(
      url,
      base::Bind(&PrepareFileAfterCheckExistOnIOThread,
                 file_system_context,
                 url,
                 base::Bind(&BoolCallbackAsFileErrorCallback, callback)));
}

}  // namespace

bool IsNonNativeFileSystemType(storage::FileSystemType type) {
  switch (type) {
    case storage::kFileSystemTypeNativeLocal:
    case storage::kFileSystemTypeRestrictedNativeLocal:
      return false;
    default:
      // The path indeed corresponds to a mount point not associated with a
      // native local path.
      return true;
  }
}

bool IsUnderNonNativeLocalPath(Profile* profile,
                        const base::FilePath& path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  GURL url;
  if (!util::ConvertAbsoluteFilePathToFileSystemUrl(
           profile, path, kFileManagerAppId, &url)) {
    return false;
  }

  storage::FileSystemURL filesystem_url =
      GetFileSystemContextForExtensionId(profile, kFileManagerAppId)
          ->CrackURL(url);
  if (!filesystem_url.is_valid())
    return false;

  return IsNonNativeFileSystemType(filesystem_url.type());
}

void GetNonNativeLocalPathMimeType(
    Profile* profile,
    const base::FilePath& path,
    const base::Callback<void(bool, const std::string&)>& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(IsUnderNonNativeLocalPath(profile, path));

  if (drive::util::IsUnderDriveMountPoint(path)) {
    drive::FileSystemInterface* file_system =
        drive::util::GetFileSystemByProfile(profile);
    if (!file_system) {
      content::BrowserThread::PostTask(
          content::BrowserThread::UI, FROM_HERE,
          base::BindOnce(callback, false, std::string()));
      return;
    }

    file_system->GetResourceEntry(
        drive::util::ExtractDrivePath(path),
        base::Bind(&GetMimeTypeAfterGetResourceEntryForDrive, callback));
    return;
  }

  if (chromeos::file_system_provider::util::IsFileSystemProviderLocalPath(
          path)) {
    chromeos::file_system_provider::util::LocalPathParser parser(profile, path);
    if (!parser.Parse()) {
      content::BrowserThread::PostTask(
          content::BrowserThread::UI, FROM_HERE,
          base::BindOnce(callback, false, std::string()));
      return;
    }

    parser.file_system()->GetMetadata(
        parser.file_path(),
        chromeos::file_system_provider::ProvidedFileSystemInterface::
            METADATA_FIELD_MIME_TYPE,
        base::Bind(&GetMimeTypeAfterGetMetadataForProvidedFileSystem,
                   callback));
    return;
  }

  if (arc::IsArcAllowedForProfile(profile) &&
      base::FilePath(arc::kContentFileSystemMountPointPath).IsParent(path)) {
    GURL arc_url = arc::PathToArcUrl(path);
    auto* runner =
        arc::ArcFileSystemOperationRunner::GetForBrowserContext(profile);
    if (!runner) {
      content::BrowserThread::PostTask(
          content::BrowserThread::UI, FROM_HERE,
          base::BindOnce(callback, false, std::string()));
      return;
    }
    runner->GetMimeType(
        arc_url, base::Bind(&GetMimeTypeAfterGetMimeTypeForArcContentFileSystem,
                            callback));
    return;
  }

  // We don't have a way to obtain metadata other than drive and FSP. Returns an
  // error with empty MIME type, that leads fallback guessing mime type from
  // file extensions.
  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::BindOnce(callback, false /* failure */, std::string()));
}

void IsNonNativeLocalPathDirectory(
    Profile* profile,
    const base::FilePath& path,
    const base::Callback<void(bool)>& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(IsUnderNonNativeLocalPath(profile, path));

  util::CheckIfDirectoryExists(
      GetFileSystemContextForExtensionId(profile, kFileManagerAppId), path,
      base::Bind(&BoolCallbackAsFileErrorCallback, callback));
}

void PrepareNonNativeLocalFileForWritableApp(
    Profile* profile,
    const base::FilePath& path,
    const base::Callback<void(bool)>& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(IsUnderNonNativeLocalPath(profile, path));

  GURL url;
  if (!util::ConvertAbsoluteFilePathToFileSystemUrl(
           profile, path, kFileManagerAppId, &url)) {
    // Posting to the current thread, so that we always call back asynchronously
    // independent from whether or not the operation succeeds.
    content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
                                     base::BindOnce(callback, false));
    return;
  }

  scoped_refptr<storage::FileSystemContext> const file_system_context =
      GetFileSystemContextForExtensionId(profile, kFileManagerAppId);
  DCHECK(file_system_context);
  storage::ExternalFileSystemBackend* const backend =
      file_system_context->external_backend();
  DCHECK(backend);
  const storage::FileSystemURL internal_url =
      backend->CreateInternalURL(file_system_context.get(), path);

  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::BindOnce(&PrepareFileOnIOThread, file_system_context, internal_url,
                     google_apis::CreateRelayCallback(callback)));
}

}  // namespace util
}  // namespace file_manager
