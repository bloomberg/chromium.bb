// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/fake_drive_file_system.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/file_errors.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/drive/resource_entry_conversion.h"
#include "chrome/browser/google_apis/drive_api_parser.h"
#include "chrome/browser/google_apis/gdata_wapi_parser.h"
#include "content/public/browser/browser_thread.h"

namespace drive {
namespace test_util {

using content::BrowserThread;

FakeDriveFileSystem::FakeDriveFileSystem(
    google_apis::DriveServiceInterface* drive_service)
    : drive_service_(drive_service),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
}

FakeDriveFileSystem::~FakeDriveFileSystem() {
}

bool FakeDriveFileSystem::InitializeForTesting() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return cache_dir_.CreateUniqueTempDir();
}

void FakeDriveFileSystem::Initialize() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  InitializeForTesting();
}

void FakeDriveFileSystem::AddObserver(FileSystemObserver* observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeDriveFileSystem::RemoveObserver(FileSystemObserver* observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeDriveFileSystem::CheckForUpdates() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeDriveFileSystem::GetEntryInfoByResourceId(
    const std::string& resource_id,
    const GetEntryInfoWithFilePathCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  drive_service_->GetResourceEntry(
      resource_id,
      base::Bind(
          &FakeDriveFileSystem::GetEntryInfoByResourceIdAfterGetResourceEntry,
          weak_ptr_factory_.GetWeakPtr(), callback));
}

void FakeDriveFileSystem::TransferFileFromRemoteToLocal(
    const base::FilePath& remote_src_file_path,
    const base::FilePath& local_dest_file_path,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeDriveFileSystem::TransferFileFromLocalToRemote(
    const base::FilePath& local_src_file_path,
    const base::FilePath& remote_dest_file_path,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeDriveFileSystem::OpenFile(const base::FilePath& file_path,
                                   const OpenFileCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeDriveFileSystem::CloseFile(const base::FilePath& file_path,
                                    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeDriveFileSystem::Copy(const base::FilePath& src_file_path,
                               const base::FilePath& dest_file_path,
                               const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeDriveFileSystem::Move(const base::FilePath& src_file_path,
                               const base::FilePath& dest_file_path,
                               const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeDriveFileSystem::Remove(const base::FilePath& file_path,
                                 bool is_recursive,
                                 const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeDriveFileSystem::CreateDirectory(
    const base::FilePath& directory_path,
    bool is_exclusive,
    bool is_recursive,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeDriveFileSystem::CreateFile(const base::FilePath& file_path,
                                     bool is_exclusive,
                                     const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeDriveFileSystem::Pin(const base::FilePath& file_path,
                              const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeDriveFileSystem::Unpin(const base::FilePath& file_path,
                                const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeDriveFileSystem::GetFileByPath(const base::FilePath& file_path,
                                        const GetFileCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeDriveFileSystem::GetFileByResourceId(
    const std::string& resource_id,
    const DriveClientContext& context,
    const GetFileCallback& get_file_callback,
    const google_apis::GetContentCallback& get_content_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeDriveFileSystem::GetFileContentByPath(
    const base::FilePath& file_path,
    const GetFileContentInitializedCallback& initialized_callback,
    const google_apis::GetContentCallback& get_content_callback,
    const FileOperationCallback& completion_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  GetEntryInfoByPath(
      file_path,
      base::Bind(&FakeDriveFileSystem::GetFileContentByPathAfterGetEntryInfo,
                 weak_ptr_factory_.GetWeakPtr(),
                 file_path, initialized_callback, get_content_callback,
                 completion_callback));
}

void FakeDriveFileSystem::UpdateFileByResourceId(
    const std::string& resource_id,
    const DriveClientContext& context,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeDriveFileSystem::GetEntryInfoByPath(
    const base::FilePath& file_path,
    const GetEntryInfoCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Now, we only support files under my drive.
  DCHECK(!util::IsUnderDriveMountPoint(file_path));

  if (file_path == util::GetDriveMyDriveRootPath()) {
    // Specialized for the root entry.
    drive_service_->GetAboutResource(
        base::Bind(
            &FakeDriveFileSystem::GetEntryInfoByPathAfterGetAboutResource,
            weak_ptr_factory_.GetWeakPtr(), callback));
    return;
  }

  GetEntryInfoByPath(
      file_path.DirName(),
      base::Bind(
          &FakeDriveFileSystem::GetEntryInfoByPathAfterGetParentEntryInfo,
          weak_ptr_factory_.GetWeakPtr(), file_path.BaseName(), callback));
}

void FakeDriveFileSystem::ReadDirectoryByPath(
    const base::FilePath& file_path,
    const ReadDirectoryWithSettingCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeDriveFileSystem::RefreshDirectory(
    const base::FilePath& file_path,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeDriveFileSystem::Search(const std::string& search_query,
                                 const GURL& next_feed,
                                 const SearchCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeDriveFileSystem::SearchMetadata(
    const std::string& query,
    int options,
    int at_most_num_matches,
    const SearchMetadataCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeDriveFileSystem::GetAvailableSpace(
    const GetAvailableSpaceCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeDriveFileSystem::AddUploadedFile(
    scoped_ptr<google_apis::ResourceEntry> doc_entry,
    const base::FilePath& file_content_path,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeDriveFileSystem::GetMetadata(
    const GetFilesystemMetadataCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeDriveFileSystem::MarkCacheFileAsMounted(
    const base::FilePath& drive_file_path,
    const OpenFileCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeDriveFileSystem::MarkCacheFileAsUnmounted(
    const base::FilePath& cache_file_path,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeDriveFileSystem::GetCacheEntryByResourceId(
    const std::string& resource_id,
    const std::string& md5,
    const GetCacheEntryCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeDriveFileSystem::IterateCache(
    const CacheIterateCallback& iteration_callback,
    const base::Closure& completion_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeDriveFileSystem::Reload() {
}

// Implementation of GetFilePath.
void FakeDriveFileSystem::GetFilePath(const std::string& resource_id,
                                      const GetFilePathCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  drive_service_->GetAboutResource(
      base::Bind(
          &FakeDriveFileSystem::GetFilePathAfterGetAboutResource,
          weak_ptr_factory_.GetWeakPtr(), resource_id, callback));
}

void FakeDriveFileSystem::GetFilePathAfterGetAboutResource(
    const std::string& resource_id,
    const GetFilePathCallback& callback,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::AboutResource> about_resource) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // We assume the call always success for test.
  DCHECK_EQ(util::GDataToFileError(error), FILE_ERROR_OK);
  DCHECK(about_resource);

  GetFilePathInternal(about_resource->root_folder_id(), resource_id,
                      base::FilePath(), callback);
}

void FakeDriveFileSystem::GetFilePathInternal(
    const std::string& root_resource_id,
    const std::string& resource_id,
    const base::FilePath& file_path,
    const GetFilePathCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (resource_id == root_resource_id) {
    // Reached to the root. Append the drive root path, and run |callback|.
    callback.Run(util::GetDriveMyDriveRootPath().Append(file_path));
    return;
  }

  drive_service_->GetResourceEntry(
      resource_id,
      base::Bind(
          &FakeDriveFileSystem::GetFilePathAfterGetResourceEntry,
          weak_ptr_factory_.GetWeakPtr(),
          root_resource_id, file_path, callback));
}

void FakeDriveFileSystem::GetFilePathAfterGetResourceEntry(
    const std::string& root_resource_id,
    const base::FilePath& remaining_file_path,
    const GetFilePathCallback& callback,
    google_apis::GDataErrorCode error_in,
    scoped_ptr<google_apis::ResourceEntry> resource_entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // We assume the call always success for test.
  DCHECK_EQ(util::GDataToFileError(error_in), FILE_ERROR_OK);
  DCHECK(resource_entry);

  DriveEntryProto entry_proto =
      ConvertResourceEntryToDriveEntryProto(*resource_entry);
  base::FilePath file_path =
      base::FilePath::FromUTF8Unsafe(entry_proto.base_name()).Append(
          remaining_file_path);

  GetFilePathInternal(root_resource_id, entry_proto.parent_resource_id(),
                      file_path, callback);
}

// Implementation of GetEntryInfoByResourceId.
void FakeDriveFileSystem::GetEntryInfoByResourceIdAfterGetResourceEntry(
    const GetEntryInfoWithFilePathCallback& callback,
    google_apis::GDataErrorCode error_in,
    scoped_ptr<google_apis::ResourceEntry> resource_entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  FileError error = util::GDataToFileError(error_in);
  if (error != FILE_ERROR_OK) {
    callback.Run(error, base::FilePath(), scoped_ptr<DriveEntryProto>());
    return;
  }

  DCHECK(resource_entry);
  scoped_ptr<DriveEntryProto> entry_proto(new DriveEntryProto(
      ConvertResourceEntryToDriveEntryProto(*resource_entry)));

  const std::string parent_resource_id = entry_proto->parent_resource_id();
  GetFilePath(
      parent_resource_id,
      base::Bind(
          &FakeDriveFileSystem::GetEntryInfoByResourceIdAfterGetFilePath,
          weak_ptr_factory_.GetWeakPtr(),
          callback, error, base::Passed(&entry_proto)));
}

void FakeDriveFileSystem::GetEntryInfoByResourceIdAfterGetFilePath(
    const GetEntryInfoWithFilePathCallback& callback,
    FileError error,
    scoped_ptr<DriveEntryProto> entry_proto,
    const base::FilePath& parent_file_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  base::FilePath file_path = parent_file_path.Append(
      base::FilePath::FromUTF8Unsafe(entry_proto->base_name()));
  callback.Run(error, file_path, entry_proto.Pass());
}

// Implementation of GetFileContentByPath.
void FakeDriveFileSystem::GetFileContentByPathAfterGetEntryInfo(
    const base::FilePath& file_path,
    const GetFileContentInitializedCallback& initialized_callback,
    const google_apis::GetContentCallback& get_content_callback,
    const FileOperationCallback& completion_callback,
    FileError error,
    scoped_ptr<DriveEntryProto> entry_proto) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (error != FILE_ERROR_OK) {
    completion_callback.Run(error);
    return;
  }
  DCHECK(entry_proto);

  // We're only interested in a file.
  if (entry_proto->file_info().is_directory()) {
    completion_callback.Run(FILE_ERROR_NOT_A_FILE);
    return;
  }

  base::FilePath cache_path =
      cache_dir_.path().AppendASCII(entry_proto->resource_id());
  if (file_util::PathExists(cache_path)) {
    // Cache file is found.
    initialized_callback.Run(FILE_ERROR_OK, entry_proto.Pass(), cache_path,
                             base::Closure());
    completion_callback.Run(FILE_ERROR_OK);
    return;
  }

  // Copy the URL here before passing |entry_proto| to the callback.
  const GURL download_url(entry_proto->download_url());
  initialized_callback.Run(FILE_ERROR_OK, entry_proto.Pass(), base::FilePath(),
                           base::Bind(&base::DoNothing));
  drive_service_->DownloadFile(
      file_path,
      cache_path,
      download_url,
      base::Bind(&FakeDriveFileSystem::GetFileContentByPathAfterDownloadFile,
                 weak_ptr_factory_.GetWeakPtr(),
                 completion_callback),
      get_content_callback,
      google_apis::ProgressCallback());
}

void FakeDriveFileSystem::GetFileContentByPathAfterDownloadFile(
    const FileOperationCallback& completion_callback,
    google_apis::GDataErrorCode gdata_error,
    const base::FilePath& temp_file) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  completion_callback.Run(util::GDataToFileError(gdata_error));
}

// Implementation of GetEntryInfoByPath.
void FakeDriveFileSystem::GetEntryInfoByPathAfterGetAboutResource(
    const GetEntryInfoCallback& callback,
    google_apis::GDataErrorCode gdata_error,
    scoped_ptr<google_apis::AboutResource> about_resource) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  FileError error = util::GDataToFileError(gdata_error);
  if (error != FILE_ERROR_OK) {
    callback.Run(error, scoped_ptr<DriveEntryProto>());
    return;
  }

  DCHECK(about_resource);
  scoped_ptr<DriveEntryProto> root(new DriveEntryProto);
  root->mutable_file_info()->set_is_directory(true);
  root->set_resource_id(about_resource->root_folder_id());
  root->set_title(util::kDriveMyDriveRootDirName);
  callback.Run(error, root.Pass());
}

void FakeDriveFileSystem::GetEntryInfoByPathAfterGetParentEntryInfo(
    const base::FilePath& base_name,
    const GetEntryInfoCallback& callback,
    FileError error,
    scoped_ptr<DriveEntryProto> parent_entry_proto) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (error != FILE_ERROR_OK) {
    callback.Run(error, scoped_ptr<DriveEntryProto>());
    return;
  }

  DCHECK(parent_entry_proto);
  drive_service_->GetResourceListInDirectory(
      parent_entry_proto->resource_id(),
      base::Bind(
          &FakeDriveFileSystem::GetEntryInfoByPathAfterGetResourceList,
          weak_ptr_factory_.GetWeakPtr(), base_name, callback));
}

void FakeDriveFileSystem::GetEntryInfoByPathAfterGetResourceList(
    const base::FilePath& base_name,
    const GetEntryInfoCallback& callback,
    google_apis::GDataErrorCode gdata_error,
    scoped_ptr<google_apis::ResourceList> resource_list) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  FileError error = util::GDataToFileError(gdata_error);
  if (error != FILE_ERROR_OK) {
    callback.Run(error, scoped_ptr<DriveEntryProto>());
    return;
  }

  DCHECK(resource_list);
  const ScopedVector<google_apis::ResourceEntry>& entries =
      resource_list->entries();
  for (size_t i = 0; i < entries.size(); ++i) {
    scoped_ptr<DriveEntryProto> entry(new DriveEntryProto(
        ConvertResourceEntryToDriveEntryProto(*entries[i])));
    if (entry->base_name() == base_name.AsUTF8Unsafe()) {
      // Found the target entry.
      callback.Run(FILE_ERROR_OK, entry.Pass());
      return;
    }
  }

  callback.Run(FILE_ERROR_NOT_FOUND, scoped_ptr<DriveEntryProto>());
}

}  // namespace test_util
}  // namespace drive
