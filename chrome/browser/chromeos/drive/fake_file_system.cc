// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/fake_file_system.h"

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

FakeFileSystem::FakeFileSystem(
    google_apis::DriveServiceInterface* drive_service)
    : drive_service_(drive_service),
      weak_ptr_factory_(this) {
}

FakeFileSystem::~FakeFileSystem() {
}

bool FakeFileSystem::InitializeForTesting() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return cache_dir_.CreateUniqueTempDir();
}

void FakeFileSystem::Initialize() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  InitializeForTesting();
}

void FakeFileSystem::AddObserver(FileSystemObserver* observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeFileSystem::RemoveObserver(FileSystemObserver* observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeFileSystem::CheckForUpdates() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeFileSystem::GetEntryInfoByResourceId(
    const std::string& resource_id,
    const GetEntryInfoWithFilePathCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  drive_service_->GetResourceEntry(
      resource_id,
      base::Bind(
          &FakeFileSystem::GetEntryInfoByResourceIdAfterGetResourceEntry,
          weak_ptr_factory_.GetWeakPtr(), callback));
}

void FakeFileSystem::TransferFileFromRemoteToLocal(
    const base::FilePath& remote_src_file_path,
    const base::FilePath& local_dest_file_path,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeFileSystem::TransferFileFromLocalToRemote(
    const base::FilePath& local_src_file_path,
    const base::FilePath& remote_dest_file_path,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeFileSystem::OpenFile(const base::FilePath& file_path,
                              const OpenFileCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeFileSystem::CloseFile(const base::FilePath& file_path,
                               const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeFileSystem::Copy(const base::FilePath& src_file_path,
                          const base::FilePath& dest_file_path,
                          const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeFileSystem::Move(const base::FilePath& src_file_path,
                          const base::FilePath& dest_file_path,
                          const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeFileSystem::Remove(const base::FilePath& file_path,
                            bool is_recursive,
                            const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeFileSystem::CreateDirectory(
    const base::FilePath& directory_path,
    bool is_exclusive,
    bool is_recursive,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeFileSystem::CreateFile(const base::FilePath& file_path,
                                bool is_exclusive,
                                const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeFileSystem::Pin(const base::FilePath& file_path,
                         const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeFileSystem::Unpin(const base::FilePath& file_path,
                           const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeFileSystem::GetFileByPath(const base::FilePath& file_path,
                                   const GetFileCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeFileSystem::GetFileByResourceId(
    const std::string& resource_id,
    const DriveClientContext& context,
    const GetFileCallback& get_file_callback,
    const google_apis::GetContentCallback& get_content_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeFileSystem::GetFileContentByPath(
    const base::FilePath& file_path,
    const GetFileContentInitializedCallback& initialized_callback,
    const google_apis::GetContentCallback& get_content_callback,
    const FileOperationCallback& completion_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  GetEntryInfoByPath(
      file_path,
      base::Bind(&FakeFileSystem::GetFileContentByPathAfterGetEntryInfo,
                 weak_ptr_factory_.GetWeakPtr(),
                 file_path, initialized_callback, get_content_callback,
                 completion_callback));
}

void FakeFileSystem::UpdateFileByResourceId(
    const std::string& resource_id,
    const DriveClientContext& context,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeFileSystem::GetEntryInfoByPath(
    const base::FilePath& file_path,
    const GetEntryInfoCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Now, we only support files under my drive.
  DCHECK(!util::IsUnderDriveMountPoint(file_path));

  if (file_path == util::GetDriveMyDriveRootPath()) {
    // Specialized for the root entry.
    drive_service_->GetAboutResource(
        base::Bind(
            &FakeFileSystem::GetEntryInfoByPathAfterGetAboutResource,
            weak_ptr_factory_.GetWeakPtr(), callback));
    return;
  }

  GetEntryInfoByPath(
      file_path.DirName(),
      base::Bind(
          &FakeFileSystem::GetEntryInfoByPathAfterGetParentEntryInfo,
          weak_ptr_factory_.GetWeakPtr(), file_path.BaseName(), callback));
}

void FakeFileSystem::ReadDirectoryByPath(
    const base::FilePath& file_path,
    const ReadDirectoryWithSettingCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeFileSystem::RefreshDirectory(
    const base::FilePath& file_path,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeFileSystem::Search(const std::string& search_query,
                            const GURL& next_feed,
                            const SearchCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeFileSystem::SearchMetadata(
    const std::string& query,
    int options,
    int at_most_num_matches,
    const SearchMetadataCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeFileSystem::GetAvailableSpace(
    const GetAvailableSpaceCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeFileSystem::AddUploadedFile(
    scoped_ptr<google_apis::ResourceEntry> doc_entry,
    const base::FilePath& file_content_path,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeFileSystem::GetMetadata(
    const GetFilesystemMetadataCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeFileSystem::MarkCacheFileAsMounted(
    const base::FilePath& drive_file_path,
    const OpenFileCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeFileSystem::MarkCacheFileAsUnmounted(
    const base::FilePath& cache_file_path,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeFileSystem::GetCacheEntryByResourceId(
    const std::string& resource_id,
    const std::string& md5,
    const GetCacheEntryCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeFileSystem::IterateCache(
    const CacheIterateCallback& iteration_callback,
    const base::Closure& completion_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeFileSystem::Reload() {
}

// Implementation of GetFilePath.
void FakeFileSystem::GetFilePath(const std::string& resource_id,
                                 const GetFilePathCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  drive_service_->GetAboutResource(
      base::Bind(
          &FakeFileSystem::GetFilePathAfterGetAboutResource,
          weak_ptr_factory_.GetWeakPtr(), resource_id, callback));
}

void FakeFileSystem::GetFilePathAfterGetAboutResource(
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

void FakeFileSystem::GetFilePathInternal(
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
          &FakeFileSystem::GetFilePathAfterGetResourceEntry,
          weak_ptr_factory_.GetWeakPtr(),
          root_resource_id, file_path, callback));
}

void FakeFileSystem::GetFilePathAfterGetResourceEntry(
    const std::string& root_resource_id,
    const base::FilePath& remaining_file_path,
    const GetFilePathCallback& callback,
    google_apis::GDataErrorCode error_in,
    scoped_ptr<google_apis::ResourceEntry> resource_entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // We assume the call always success for test.
  DCHECK_EQ(util::GDataToFileError(error_in), FILE_ERROR_OK);
  DCHECK(resource_entry);

  ResourceEntry entry = ConvertToResourceEntry(*resource_entry);
  base::FilePath file_path =
      base::FilePath::FromUTF8Unsafe(entry.base_name()).Append(
          remaining_file_path);

  GetFilePathInternal(root_resource_id, entry.parent_resource_id(),
                      file_path, callback);
}

// Implementation of GetEntryInfoByResourceId.
void FakeFileSystem::GetEntryInfoByResourceIdAfterGetResourceEntry(
    const GetEntryInfoWithFilePathCallback& callback,
    google_apis::GDataErrorCode error_in,
    scoped_ptr<google_apis::ResourceEntry> resource_entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  FileError error = util::GDataToFileError(error_in);
  if (error != FILE_ERROR_OK) {
    callback.Run(error, base::FilePath(), scoped_ptr<ResourceEntry>());
    return;
  }

  DCHECK(resource_entry);
  scoped_ptr<ResourceEntry> entry(new ResourceEntry(
      ConvertToResourceEntry(*resource_entry)));

  const std::string parent_resource_id = entry->parent_resource_id();
  GetFilePath(
      parent_resource_id,
      base::Bind(
          &FakeFileSystem::GetEntryInfoByResourceIdAfterGetFilePath,
          weak_ptr_factory_.GetWeakPtr(),
          callback, error, base::Passed(&entry)));
}

void FakeFileSystem::GetEntryInfoByResourceIdAfterGetFilePath(
    const GetEntryInfoWithFilePathCallback& callback,
    FileError error,
    scoped_ptr<ResourceEntry> entry,
    const base::FilePath& parent_file_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  base::FilePath file_path = parent_file_path.Append(
      base::FilePath::FromUTF8Unsafe(entry->base_name()));
  callback.Run(error, file_path, entry.Pass());
}

// Implementation of GetFileContentByPath.
void FakeFileSystem::GetFileContentByPathAfterGetEntryInfo(
    const base::FilePath& file_path,
    const GetFileContentInitializedCallback& initialized_callback,
    const google_apis::GetContentCallback& get_content_callback,
    const FileOperationCallback& completion_callback,
    FileError error,
    scoped_ptr<ResourceEntry> entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (error != FILE_ERROR_OK) {
    completion_callback.Run(error);
    return;
  }
  DCHECK(entry);

  // We're only interested in a file.
  if (entry->file_info().is_directory()) {
    completion_callback.Run(FILE_ERROR_NOT_A_FILE);
    return;
  }

  base::FilePath cache_path =
      cache_dir_.path().AppendASCII(entry->resource_id());
  if (file_util::PathExists(cache_path)) {
    // Cache file is found.
    initialized_callback.Run(FILE_ERROR_OK, entry.Pass(), cache_path,
                             base::Closure());
    completion_callback.Run(FILE_ERROR_OK);
    return;
  }

  // Copy the URL here before passing |entry| to the callback.
  const GURL download_url(entry->download_url());
  initialized_callback.Run(FILE_ERROR_OK, entry.Pass(), base::FilePath(),
                           base::Bind(&base::DoNothing));
  drive_service_->DownloadFile(
      file_path,
      cache_path,
      download_url,
      base::Bind(&FakeFileSystem::GetFileContentByPathAfterDownloadFile,
                 weak_ptr_factory_.GetWeakPtr(),
                 completion_callback),
      get_content_callback,
      google_apis::ProgressCallback());
}

void FakeFileSystem::GetFileContentByPathAfterDownloadFile(
    const FileOperationCallback& completion_callback,
    google_apis::GDataErrorCode gdata_error,
    const base::FilePath& temp_file) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  completion_callback.Run(util::GDataToFileError(gdata_error));
}

// Implementation of GetEntryInfoByPath.
void FakeFileSystem::GetEntryInfoByPathAfterGetAboutResource(
    const GetEntryInfoCallback& callback,
    google_apis::GDataErrorCode gdata_error,
    scoped_ptr<google_apis::AboutResource> about_resource) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  FileError error = util::GDataToFileError(gdata_error);
  if (error != FILE_ERROR_OK) {
    callback.Run(error, scoped_ptr<ResourceEntry>());
    return;
  }

  DCHECK(about_resource);
  scoped_ptr<ResourceEntry> root(new ResourceEntry);
  root->mutable_file_info()->set_is_directory(true);
  root->set_resource_id(about_resource->root_folder_id());
  root->set_title(util::kDriveMyDriveRootDirName);
  callback.Run(error, root.Pass());
}

void FakeFileSystem::GetEntryInfoByPathAfterGetParentEntryInfo(
    const base::FilePath& base_name,
    const GetEntryInfoCallback& callback,
    FileError error,
    scoped_ptr<ResourceEntry> parent_entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (error != FILE_ERROR_OK) {
    callback.Run(error, scoped_ptr<ResourceEntry>());
    return;
  }

  DCHECK(parent_entry);
  drive_service_->GetResourceListInDirectory(
      parent_entry->resource_id(),
      base::Bind(
          &FakeFileSystem::GetEntryInfoByPathAfterGetResourceList,
          weak_ptr_factory_.GetWeakPtr(), base_name, callback));
}

void FakeFileSystem::GetEntryInfoByPathAfterGetResourceList(
    const base::FilePath& base_name,
    const GetEntryInfoCallback& callback,
    google_apis::GDataErrorCode gdata_error,
    scoped_ptr<google_apis::ResourceList> resource_list) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  FileError error = util::GDataToFileError(gdata_error);
  if (error != FILE_ERROR_OK) {
    callback.Run(error, scoped_ptr<ResourceEntry>());
    return;
  }

  DCHECK(resource_list);
  const ScopedVector<google_apis::ResourceEntry>& entries =
      resource_list->entries();
  for (size_t i = 0; i < entries.size(); ++i) {
    scoped_ptr<ResourceEntry> entry(new ResourceEntry(
        ConvertToResourceEntry(*entries[i])));
    if (entry->base_name() == base_name.AsUTF8Unsafe()) {
      // Found the target entry.
      callback.Run(FILE_ERROR_OK, entry.Pass());
      return;
    }
  }

  callback.Run(FILE_ERROR_NOT_FOUND, scoped_ptr<ResourceEntry>());
}

}  // namespace test_util
}  // namespace drive
