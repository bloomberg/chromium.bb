// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/file_system.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/platform_file.h"
#include "base/prefs/pref_change_registrar.h"
#include "base/prefs/pref_service.h"
#include "base/strings/stringprintf.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/chromeos/drive/change_list_loader.h"
#include "chrome/browser/chromeos/drive/change_list_processor.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/file_cache.h"
#include "chrome/browser/chromeos/drive/file_system/copy_operation.h"
#include "chrome/browser/chromeos/drive/file_system/create_directory_operation.h"
#include "chrome/browser/chromeos/drive/file_system/create_file_operation.h"
#include "chrome/browser/chromeos/drive/file_system/download_operation.h"
#include "chrome/browser/chromeos/drive/file_system/move_operation.h"
#include "chrome/browser/chromeos/drive/file_system/remove_operation.h"
#include "chrome/browser/chromeos/drive/file_system/search_operation.h"
#include "chrome/browser/chromeos/drive/file_system/touch_operation.h"
#include "chrome/browser/chromeos/drive/file_system/update_operation.h"
#include "chrome/browser/chromeos/drive/file_system_observer.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/drive/job_scheduler.h"
#include "chrome/browser/chromeos/drive/remove_stale_cache_files.h"
#include "chrome/browser/chromeos/drive/search_metadata.h"
#include "chrome/browser/chromeos/drive/sync_client.h"
#include "chrome/browser/drive/drive_api_util.h"
#include "chrome/browser/drive/drive_service_interface.h"
#include "chrome/browser/google_apis/drive_api_parser.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace drive {
namespace {

//================================ Helper functions ============================

// Callback for ResourceMetadata::GetLargestChangestamp.
// |callback| must not be null.
void OnGetLargestChangestamp(
    FileSystemMetadata metadata,  // Will be modified.
    const GetFilesystemMetadataCallback& callback,
    int64 largest_changestamp) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  metadata.largest_changestamp = largest_changestamp;
  callback.Run(metadata);
}

// Thin adapter to map GetFileCallback to FileOperationCallback.
void GetFileCallbackToFileOperationCallbackAdapter(
    const FileOperationCallback& callback,
    FileError error,
    const base::FilePath& unused_file_path,
    scoped_ptr<ResourceEntry> unused_entry) {
  callback.Run(error);
}

}  // namespace

FileSystem::FileSystem(
    Profile* profile,
    internal::FileCache* cache,
    DriveServiceInterface* drive_service,
    JobScheduler* scheduler,
    internal::ResourceMetadata* resource_metadata,
    base::SequencedTaskRunner* blocking_task_runner,
    const base::FilePath& temporary_file_directory)
    : profile_(profile),
      cache_(cache),
      drive_service_(drive_service),
      scheduler_(scheduler),
      resource_metadata_(resource_metadata),
      last_update_check_error_(FILE_ERROR_OK),
      hide_hosted_docs_(false),
      blocking_task_runner_(blocking_task_runner),
      temporary_file_directory_(temporary_file_directory),
      weak_ptr_factory_(this) {
  // Should be created from the file browser extension API on UI thread.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FileSystem::Reload() {
  resource_metadata_->ResetOnUIThread(base::Bind(
      &FileSystem::ReloadAfterReset,
      weak_ptr_factory_.GetWeakPtr()));
}

void FileSystem::Initialize() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  SetupChangeListLoader();

  file_system::OperationObserver* observer = this;
  copy_operation_.reset(
      new file_system::CopyOperation(blocking_task_runner_.get(),
                                     observer,
                                     scheduler_,
                                     resource_metadata_,
                                     cache_,
                                     drive_service_,
                                     temporary_file_directory_));
  create_directory_operation_.reset(new file_system::CreateDirectoryOperation(
      blocking_task_runner_.get(), observer, scheduler_, resource_metadata_));
  create_file_operation_.reset(
      new file_system::CreateFileOperation(blocking_task_runner_.get(),
                                           observer,
                                           scheduler_,
                                           resource_metadata_,
                                           cache_));
  move_operation_.reset(
      new file_system::MoveOperation(observer, scheduler_, resource_metadata_));
  remove_operation_.reset(
      new file_system::RemoveOperation(blocking_task_runner_.get(),
                                       observer,
                                       scheduler_,
                                       resource_metadata_,
                                       cache_));
  touch_operation_.reset(new file_system::TouchOperation(
      blocking_task_runner_.get(), observer, scheduler_, resource_metadata_));
  download_operation_.reset(
      new file_system::DownloadOperation(blocking_task_runner_.get(),
                                         observer,
                                         scheduler_,
                                         resource_metadata_,
                                         cache_,
                                         temporary_file_directory_));
  update_operation_.reset(
      new file_system::UpdateOperation(blocking_task_runner_.get(),
                                       observer,
                                       scheduler_,
                                       resource_metadata_,
                                       cache_));
  search_operation_.reset(new file_system::SearchOperation(
      blocking_task_runner_.get(), scheduler_, resource_metadata_));
  sync_client_.reset(new internal::SyncClient(blocking_task_runner_.get(),
                                              observer,
                                              scheduler_,
                                              resource_metadata_,
                                              cache_,
                                              temporary_file_directory_));

  PrefService* pref_service = profile_->GetPrefs();
  hide_hosted_docs_ = pref_service->GetBoolean(prefs::kDisableDriveHostedFiles);

  InitializePreferenceObserver();
}

void FileSystem::ReloadAfterReset(FileError error) {
  if (error != FILE_ERROR_OK) {
    LOG(ERROR) << "Failed to reset the resource metadata: "
               << FileErrorToString(error);
    return;
  }

  SetupChangeListLoader();

  change_list_loader_->LoadIfNeeded(
      DirectoryFetchInfo(),
      base::Bind(&FileSystem::OnUpdateChecked,
                 weak_ptr_factory_.GetWeakPtr()));
}

void FileSystem::SetupChangeListLoader() {
  change_list_loader_.reset(new internal::ChangeListLoader(
      blocking_task_runner_.get(), resource_metadata_, scheduler_));
  change_list_loader_->AddObserver(this);
}

void FileSystem::CheckForUpdates() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DVLOG(1) << "CheckForUpdates";

  if (change_list_loader_) {
    change_list_loader_->CheckForUpdates(
        base::Bind(&FileSystem::OnUpdateChecked,
                   weak_ptr_factory_.GetWeakPtr()));
  }
}

void FileSystem::OnUpdateChecked(FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DVLOG(1) << "CheckForUpdates finished: " << FileErrorToString(error);
  last_update_check_time_ = base::Time::Now();
  last_update_check_error_ = error;
}

FileSystem::~FileSystem() {
  // This should be called from UI thread, from DriveIntegrationService
  // shutdown.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  change_list_loader_->RemoveObserver(this);
}

void FileSystem::AddObserver(FileSystemObserver* observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  observers_.AddObserver(observer);
}

void FileSystem::RemoveObserver(FileSystemObserver* observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  observers_.RemoveObserver(observer);
}

void FileSystem::GetResourceEntryById(
    const std::string& resource_id,
    const GetResourceEntryCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!resource_id.empty());
  DCHECK(!callback.is_null());

  resource_metadata_->GetResourceEntryByIdOnUIThread(
      resource_id,
      base::Bind(&FileSystem::GetResourceEntryByIdAfterGetEntry,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback));
}

void FileSystem::GetResourceEntryByIdAfterGetEntry(
    const GetResourceEntryCallback& callback,
    FileError error,
    scoped_ptr<ResourceEntry> entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (error != FILE_ERROR_OK) {
    callback.Run(error, scoped_ptr<ResourceEntry>());
    return;
  }
  DCHECK(entry.get());

  CheckLocalModificationAndRun(entry.Pass(), callback);
}

void FileSystem::TransferFileFromRemoteToLocal(
    const base::FilePath& remote_src_file_path,
    const base::FilePath& local_dest_file_path,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  copy_operation_->TransferFileFromRemoteToLocal(remote_src_file_path,
                                                 local_dest_file_path,
                                                 callback);
}

void FileSystem::TransferFileFromLocalToRemote(
    const base::FilePath& local_src_file_path,
    const base::FilePath& remote_dest_file_path,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  copy_operation_->TransferFileFromLocalToRemote(local_src_file_path,
                                                 remote_dest_file_path,
                                                 callback);
}

void FileSystem::Copy(const base::FilePath& src_file_path,
                      const base::FilePath& dest_file_path,
                      const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  copy_operation_->Copy(src_file_path, dest_file_path, callback);
}

void FileSystem::Move(const base::FilePath& src_file_path,
                      const base::FilePath& dest_file_path,
                      const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  move_operation_->Move(src_file_path, dest_file_path, callback);
}

void FileSystem::Remove(const base::FilePath& file_path,
                        bool is_recursive,
                        const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  remove_operation_->Remove(file_path, is_recursive, callback);
}

void FileSystem::CreateDirectory(
    const base::FilePath& directory_path,
    bool is_exclusive,
    bool is_recursive,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  change_list_loader_->LoadIfNeeded(
      DirectoryFetchInfo(),
      base::Bind(&FileSystem::CreateDirectoryAfterLoad,
                 weak_ptr_factory_.GetWeakPtr(),
                 directory_path, is_exclusive, is_recursive, callback));
}

void FileSystem::CreateDirectoryAfterLoad(
    const base::FilePath& directory_path,
    bool is_exclusive,
    bool is_recursive,
    const FileOperationCallback& callback,
    FileError load_error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (load_error != FILE_ERROR_OK) {
    callback.Run(load_error);
    return;
  }

  create_directory_operation_->CreateDirectory(
      directory_path, is_exclusive, is_recursive, callback);
}

void FileSystem::CreateFile(const base::FilePath& file_path,
                            bool is_exclusive,
                            const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  create_file_operation_->CreateFile(file_path, is_exclusive, callback);
}

void FileSystem::TouchFile(const base::FilePath& file_path,
                           const base::Time& last_access_time,
                           const base::Time& last_modified_time,
                           const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!last_access_time.is_null());
  DCHECK(!last_modified_time.is_null());
  DCHECK(!callback.is_null());
  touch_operation_->TouchFile(
      file_path, last_access_time, last_modified_time, callback);
}

void FileSystem::Pin(const base::FilePath& file_path,
                     const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  GetResourceEntryByPath(file_path,
                         base::Bind(&FileSystem::PinAfterGetResourceEntryByPath,
                                    weak_ptr_factory_.GetWeakPtr(),
                                    callback));
}

void FileSystem::PinAfterGetResourceEntryByPath(
    const FileOperationCallback& callback,
    FileError error,
    scoped_ptr<ResourceEntry> entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  // TODO(hashimoto): Support pinning directories. crbug.com/127831
  if (entry && entry->file_info().is_directory())
    error = FILE_ERROR_NOT_A_FILE;

  if (error != FILE_ERROR_OK) {
    callback.Run(error);
    return;
  }
  DCHECK(entry);

  cache_->PinOnUIThread(entry->resource_id(),
                        base::Bind(&FileSystem::FinishPin,
                                   weak_ptr_factory_.GetWeakPtr(),
                                   callback,
                                   entry->resource_id()));
}

void FileSystem::FinishPin(const FileOperationCallback& callback,
                           const std::string& resource_id,
                           FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (error == FILE_ERROR_OK)
    sync_client_->AddFetchTask(resource_id);
  callback.Run(error);
}

void FileSystem::Unpin(const base::FilePath& file_path,
                       const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  GetResourceEntryByPath(
      file_path,
      base::Bind(&FileSystem::UnpinAfterGetResourceEntryByPath,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback));
}

void FileSystem::UnpinAfterGetResourceEntryByPath(
    const FileOperationCallback& callback,
    FileError error,
    scoped_ptr<ResourceEntry> entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  // TODO(hashimoto): Support pinning directories. crbug.com/127831
  if (entry && entry->file_info().is_directory())
    error = FILE_ERROR_NOT_A_FILE;

  if (error != FILE_ERROR_OK) {
    callback.Run(error);
    return;
  }
  DCHECK(entry);

  cache_->UnpinOnUIThread(entry->resource_id(),
                          base::Bind(&FileSystem::FinishUnpin,
                                     weak_ptr_factory_.GetWeakPtr(),
                                     callback,
                                     entry->resource_id()));
}

void FileSystem::FinishUnpin(const FileOperationCallback& callback,
                             const std::string& resource_id,
                             FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (error == FILE_ERROR_OK)
    sync_client_->RemoveFetchTask(resource_id);
  callback.Run(error);
}

void FileSystem::GetFileByPath(const base::FilePath& file_path,
                               const GetFileCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  download_operation_->EnsureFileDownloadedByPath(
      file_path,
      ClientContext(USER_INITIATED),
      GetFileContentInitializedCallback(),
      google_apis::GetContentCallback(),
      callback);
}

void FileSystem::GetFileByResourceId(
    const std::string& resource_id,
    const ClientContext& context,
    const GetFileCallback& get_file_callback,
    const google_apis::GetContentCallback& get_content_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!resource_id.empty());
  DCHECK(!get_file_callback.is_null());

  download_operation_->EnsureFileDownloadedByResourceId(
      resource_id,
      context,
      GetFileContentInitializedCallback(),
      get_content_callback,
      get_file_callback);
}

void FileSystem::GetFileContentByPath(
    const base::FilePath& file_path,
    const GetFileContentInitializedCallback& initialized_callback,
    const google_apis::GetContentCallback& get_content_callback,
    const FileOperationCallback& completion_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!initialized_callback.is_null());
  DCHECK(!get_content_callback.is_null());
  DCHECK(!completion_callback.is_null());

  download_operation_->EnsureFileDownloadedByPath(
      file_path,
      ClientContext(USER_INITIATED),
      initialized_callback,
      get_content_callback,
      base::Bind(&GetFileCallbackToFileOperationCallbackAdapter,
                 completion_callback));
}

void FileSystem::GetResourceEntryByPath(
    const base::FilePath& file_path,
    const GetResourceEntryCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  // ResourceMetadata may know about the entry even if the resource
  // metadata is not yet fully loaded. For instance, ResourceMetadata()
  // always knows about the root directory. For "fast fetch"
  // (crbug.com/178348) to work, it's needed to delay the resource metadata
  // loading until the first call to ReadDirectoryByPath().
  resource_metadata_->GetResourceEntryByPathOnUIThread(
      file_path,
      base::Bind(&FileSystem::GetResourceEntryByPathAfterGetEntry1,
                 weak_ptr_factory_.GetWeakPtr(),
                 file_path,
                 callback));
}

void FileSystem::GetResourceEntryByPathAfterGetEntry1(
    const base::FilePath& file_path,
    const GetResourceEntryCallback& callback,
    FileError error,
    scoped_ptr<ResourceEntry> entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (error == FILE_ERROR_OK) {
    CheckLocalModificationAndRun(entry.Pass(), callback);
    return;
  }

  // If the information about the path is not in the local ResourceMetadata,
  // try fetching information of the directory and retry.
  LoadDirectoryIfNeeded(
      file_path.DirName(),
      base::Bind(&FileSystem::GetResourceEntryByPathAfterLoad,
                 weak_ptr_factory_.GetWeakPtr(),
                 file_path,
                 callback));
}

void FileSystem::GetResourceEntryByPathAfterLoad(
    const base::FilePath& file_path,
    const GetResourceEntryCallback& callback,
    FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (error != FILE_ERROR_OK) {
    callback.Run(error, scoped_ptr<ResourceEntry>());
    return;
  }

  resource_metadata_->GetResourceEntryByPathOnUIThread(
      file_path,
      base::Bind(&FileSystem::GetResourceEntryByPathAfterGetEntry2,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback));
}

void FileSystem::GetResourceEntryByPathAfterGetEntry2(
    const GetResourceEntryCallback& callback,
    FileError error,
    scoped_ptr<ResourceEntry> entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (error != FILE_ERROR_OK) {
    callback.Run(error, scoped_ptr<ResourceEntry>());
    return;
  }
  DCHECK(entry.get());

  CheckLocalModificationAndRun(entry.Pass(), callback);
}

void FileSystem::ReadDirectoryByPath(
    const base::FilePath& directory_path,
    const ReadDirectoryWithSettingCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  LoadDirectoryIfNeeded(
      directory_path,
      base::Bind(&FileSystem::ReadDirectoryByPathAfterLoad,
                 weak_ptr_factory_.GetWeakPtr(),
                 directory_path,
                 callback));
}

void FileSystem::LoadDirectoryIfNeeded(const base::FilePath& directory_path,
                                       const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  // As described in GetResourceEntryByPath(), ResourceMetadata may know
  // about the entry even if the file system is not yet fully loaded, hence we
  // should just ask ResourceMetadata first.
  resource_metadata_->GetResourceEntryByPathOnUIThread(
      directory_path,
      base::Bind(&FileSystem::LoadDirectoryIfNeededAfterGetEntry,
                 weak_ptr_factory_.GetWeakPtr(),
                 directory_path,
                 callback));
}

void FileSystem::LoadDirectoryIfNeededAfterGetEntry(
    const base::FilePath& directory_path,
    const FileOperationCallback& callback,
    FileError error,
    scoped_ptr<ResourceEntry> entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (error != FILE_ERROR_OK ||
      entry->resource_id() == util::kDriveOtherDirSpecialResourceId) {
    // If we don't know about the directory, or it is the "drive/other"
    // directory that has to gather all orphan entries, start loading full
    // resource list.
    change_list_loader_->LoadIfNeeded(DirectoryFetchInfo(), callback);
    return;
  }

  if (!entry->file_info().is_directory()) {
    callback.Run(FILE_ERROR_NOT_A_DIRECTORY);
    return;
  }

  // Pass the directory fetch info so we can fetch the contents of the
  // directory before loading change lists.
  DirectoryFetchInfo directory_fetch_info(
      entry->resource_id(),
      entry->directory_specific_info().changestamp());
  change_list_loader_->LoadIfNeeded(directory_fetch_info, callback);
}

void FileSystem::ReadDirectoryByPathAfterLoad(
    const base::FilePath& directory_path,
    const ReadDirectoryWithSettingCallback& callback,
    FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  DLOG_IF(INFO, error != FILE_ERROR_OK) << "LoadIfNeeded failed. "
                                        << FileErrorToString(error);

  resource_metadata_->ReadDirectoryByPathOnUIThread(
      directory_path,
      base::Bind(&FileSystem::ReadDirectoryByPathAfterRead,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback));
}

void FileSystem::ReadDirectoryByPathAfterRead(
    const ReadDirectoryWithSettingCallback& callback,
    FileError error,
    scoped_ptr<ResourceEntryVector> entries) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (error != FILE_ERROR_OK) {
    callback.Run(error,
                 hide_hosted_docs_,
                 scoped_ptr<ResourceEntryVector>());
    return;
  }
  DCHECK(entries.get());  // This is valid for empty directories too.

  callback.Run(FILE_ERROR_OK, hide_hosted_docs_, entries.Pass());
}

void FileSystem::RefreshDirectory(
    const base::FilePath& directory_path,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  // Make sure the destination directory exists.
  resource_metadata_->GetResourceEntryByPathOnUIThread(
      directory_path,
      base::Bind(&FileSystem::RefreshDirectoryAfterGetResourceEntry,
                 weak_ptr_factory_.GetWeakPtr(),
                 directory_path,
                 callback));
}

void FileSystem::RefreshDirectoryAfterGetResourceEntry(
    const base::FilePath& directory_path,
    const FileOperationCallback& callback,
    FileError error,
    scoped_ptr<ResourceEntry> entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (error != FILE_ERROR_OK) {
    callback.Run(error);
    return;
  }
  if (!entry->file_info().is_directory()) {
    callback.Run(FILE_ERROR_NOT_A_DIRECTORY);
    return;
  }
  if (util::IsSpecialResourceId(entry->resource_id())) {
    // Do not load special directories. Just return.
    callback.Run(FILE_ERROR_OK);
    return;
  }

  change_list_loader_->LoadDirectoryFromServer(
      entry->resource_id(),
      callback);
}

void FileSystem::UpdateFileByResourceId(
    const std::string& resource_id,
    const ClientContext& context,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  update_operation_->UpdateFileByResourceId(resource_id, context, callback);
}

void FileSystem::GetAvailableSpace(
    const GetAvailableSpaceCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  scheduler_->GetAboutResource(
      base::Bind(&FileSystem::OnGetAboutResource,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback));
}

void FileSystem::OnGetAboutResource(
    const GetAvailableSpaceCallback& callback,
    google_apis::GDataErrorCode status,
    scoped_ptr<google_apis::AboutResource> about_resource) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  FileError error = util::GDataToFileError(status);
  if (error != FILE_ERROR_OK) {
    callback.Run(error, -1, -1);
    return;
  }
  DCHECK(about_resource);

  callback.Run(FILE_ERROR_OK,
               about_resource->quota_bytes_total(),
               about_resource->quota_bytes_used());
}

void FileSystem::Search(const std::string& search_query,
                        const GURL& next_url,
                        const SearchCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  search_operation_->Search(search_query, next_url, callback);
}

void FileSystem::SearchMetadata(const std::string& query,
                                int options,
                                int at_most_num_matches,
                                const SearchMetadataCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (hide_hosted_docs_)
    options |= SEARCH_METADATA_EXCLUDE_HOSTED_DOCUMENTS;

  drive::internal::SearchMetadata(blocking_task_runner_,
                                  resource_metadata_,
                                  cache_,
                                  query,
                                  options,
                                  at_most_num_matches,
                                  callback);
}

void FileSystem::OnDirectoryChangedByOperation(
    const base::FilePath& directory_path) {
  OnDirectoryChanged(directory_path);
}

void FileSystem::OnCacheFileUploadNeededByOperation(
    const std::string& resource_id) {
  sync_client_->AddUploadTask(resource_id);
}

void FileSystem::OnDirectoryChanged(const base::FilePath& directory_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  FOR_EACH_OBSERVER(FileSystemObserver, observers_,
                    OnDirectoryChanged(directory_path));
}

void FileSystem::OnLoadFromServerComplete() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  sync_client_->StartCheckingExistingPinnedFiles();
}

void FileSystem::OnInitialLoadComplete() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  blocking_task_runner_->PostTask(FROM_HERE,
                                  base::Bind(&internal::RemoveStaleCacheFiles,
                                             cache_,
                                             resource_metadata_));
  sync_client_->StartProcessingBacklog();
}

void FileSystem::GetMetadata(
    const GetFilesystemMetadataCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  FileSystemMetadata metadata;
  metadata.refreshing = change_list_loader_->IsRefreshing();

  // Metadata related to delta update.
  metadata.last_update_check_time = last_update_check_time_;
  metadata.last_update_check_error = last_update_check_error_;

  resource_metadata_->GetLargestChangestampOnUIThread(
      base::Bind(&OnGetLargestChangestamp, metadata, callback));
}

void FileSystem::MarkCacheFileAsMounted(
    const base::FilePath& drive_file_path,
    const OpenFileCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  GetResourceEntryByPath(
      drive_file_path,
      base::Bind(&FileSystem::MarkCacheFileAsMountedAfterGetResourceEntry,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

void FileSystem::MarkCacheFileAsMountedAfterGetResourceEntry(
    const OpenFileCallback& callback,
    FileError error,
    scoped_ptr<ResourceEntry> entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (error != FILE_ERROR_OK) {
    callback.Run(error, base::FilePath());
    return;
  }

  DCHECK(entry);
  cache_->MarkAsMountedOnUIThread(entry->resource_id(), callback);
}

void FileSystem::MarkCacheFileAsUnmounted(
    const base::FilePath& cache_file_path,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (!cache_->IsUnderFileCacheDirectory(cache_file_path)) {
    callback.Run(FILE_ERROR_FAILED);
    return;
  }
  cache_->MarkAsUnmountedOnUIThread(cache_file_path, callback);
}

void FileSystem::GetCacheEntryByResourceId(
    const std::string& resource_id,
    const std::string& md5,
    const GetCacheEntryCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!resource_id.empty());
  DCHECK(!callback.is_null());

  cache_->GetCacheEntryOnUIThread(resource_id, md5, callback);
}

void FileSystem::OnDisableDriveHostedFilesChanged() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  PrefService* pref_service = profile_->GetPrefs();
  SetHideHostedDocuments(
      pref_service->GetBoolean(prefs::kDisableDriveHostedFiles));
}

void FileSystem::SetHideHostedDocuments(bool hide) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (hide == hide_hosted_docs_)
    return;

  hide_hosted_docs_ = hide;

  // Kick off directory refresh when this setting changes.
  FOR_EACH_OBSERVER(FileSystemObserver, observers_,
                    OnDirectoryChanged(util::GetDriveGrandRootPath()));
}

//============= FileSystem: internal helper functions =====================

void FileSystem::InitializePreferenceObserver() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  pref_registrar_.reset(new PrefChangeRegistrar());
  pref_registrar_->Init(profile_->GetPrefs());
  pref_registrar_->Add(
      prefs::kDisableDriveHostedFiles,
      base::Bind(&FileSystem::OnDisableDriveHostedFilesChanged,
                 base::Unretained(this)));
}

void FileSystem::OpenFile(const base::FilePath& file_path,
                          const OpenFileCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  // If the file is already opened, it cannot be opened again before closed.
  // This is for avoiding simultaneous modification to the file, and moreover
  // to avoid an inconsistent cache state (suppose an operation sequence like
  // Open->Open->modify->Close->modify->Close; the second modify may not be
  // synchronized to the server since it is already Closed on the cache).
  if (open_files_.find(file_path) != open_files_.end()) {
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE,
        base::Bind(callback, FILE_ERROR_IN_USE, base::FilePath()));
    return;
  }
  open_files_.insert(file_path);

  download_operation_->EnsureFileDownloadedByPath(
      file_path,
      ClientContext(USER_INITIATED),
      GetFileContentInitializedCallback(),
      google_apis::GetContentCallback(),
      base::Bind(&FileSystem::OpenFileAfterFileDownloaded,
                 weak_ptr_factory_.GetWeakPtr(),
                 file_path,
                 base::Bind(&FileSystem::OnOpenFileFinished,
                            weak_ptr_factory_.GetWeakPtr(),
                            file_path,
                            callback)));
}

void FileSystem::OpenFileAfterFileDownloaded(
    const base::FilePath& file_path,
    const OpenFileCallback& callback,
    FileError error,
    const base::FilePath& local_file_path,
    scoped_ptr<ResourceEntry> entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (error == FILE_ERROR_OK) {
    DCHECK(entry);
    DCHECK(entry->has_file_specific_info());
    if (entry->file_specific_info().is_hosted_document())
      // No support for opening a hosted document.
      error = FILE_ERROR_INVALID_OPERATION;
  }

  if (error != FILE_ERROR_OK) {
    callback.Run(error, base::FilePath());
    return;
  }

  cache_->MarkDirtyOnUIThread(entry->resource_id(),
                              entry->file_specific_info().md5(),
                              base::Bind(&FileSystem::OpenFileAfterMarkDirty,
                                         weak_ptr_factory_.GetWeakPtr(),
                                         entry->resource_id(),
                                         entry->file_specific_info().md5(),
                                         callback));
}

void FileSystem::OpenFileAfterMarkDirty(
    const std::string& resource_id,
    const std::string& md5,
    const OpenFileCallback& callback,
    FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (error != FILE_ERROR_OK) {
    callback.Run(error, base::FilePath());
    return;
  }

  cache_->GetFileOnUIThread(resource_id, md5, callback);
}

void FileSystem::OnOpenFileFinished(
    const base::FilePath& file_path,
    const OpenFileCallback& callback,
    FileError result,
    const base::FilePath& cache_file_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  // All the invocation of |callback| from operations initiated from OpenFile
  // must go through here. Removes the |file_path| from the remembered set when
  // the file was not successfully opened.
  if (result != FILE_ERROR_OK)
    open_files_.erase(file_path);

  callback.Run(result, cache_file_path);
}

void FileSystem::CloseFile(const base::FilePath& file_path,
                           const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (open_files_.find(file_path) == open_files_.end()) {
    // The file is not being opened.
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE,
        base::Bind(callback, FILE_ERROR_NOT_FOUND));
    return;
  }

  // Step 1 of CloseFile: Get resource_id and md5 for |file_path|.
  resource_metadata_->GetResourceEntryByPathOnUIThread(
      file_path,
      base::Bind(&FileSystem::CloseFileAfterGetResourceEntry,
                 weak_ptr_factory_.GetWeakPtr(),
                 file_path,
                 callback));
}

void FileSystem::CloseFileAfterGetResourceEntry(
    const base::FilePath& file_path,
    const FileOperationCallback& callback,
    FileError error,
    scoped_ptr<ResourceEntry> entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (entry.get() && !entry->has_file_specific_info())
    error = FILE_ERROR_NOT_FOUND;

  // Step 2 of CloseFile: Trigger upload.
  // TODO(benchan,kinaba): Call ClearDirtyInCache if the file has not been
  // modified. Come up with a way to detect the intactness effectively, or
  // provide a method for user to declare it when calling CloseFile().
  if (error == FILE_ERROR_OK)
    sync_client_->AddUploadTask(entry->resource_id());

  // Step 3 of CloseFile.
  // All the invocation of |callback| from operations initiated from CloseFile
  // must go through here. Removes the |file_path| from the remembered set so
  // that subsequent operations can open the file again.
  open_files_.erase(file_path);

  // Then invokes the user-supplied callback function.
  callback.Run(error);
}

void FileSystem::CheckLocalModificationAndRun(
    scoped_ptr<ResourceEntry> entry,
    const GetResourceEntryCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(entry.get());
  DCHECK(!callback.is_null());

  // For entries that will never be cached, use the original resource entry
  // as is.
  if (!entry->has_file_specific_info() ||
      entry->file_specific_info().is_hosted_document()) {
    callback.Run(FILE_ERROR_OK, entry.Pass());
    return;
  }

  // Checks if the file is cached and modified locally.
  const std::string resource_id = entry->resource_id();
  const std::string md5 = entry->file_specific_info().md5();
  cache_->GetCacheEntryOnUIThread(
      resource_id,
      md5,
      base::Bind(
          &FileSystem::CheckLocalModificationAndRunAfterGetCacheEntry,
          weak_ptr_factory_.GetWeakPtr(),
          base::Passed(&entry),
          callback));
}

void FileSystem::CheckLocalModificationAndRunAfterGetCacheEntry(
    scoped_ptr<ResourceEntry> entry,
    const GetResourceEntryCallback& callback,
    bool success,
    const FileCacheEntry& cache_entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  // When no dirty cache is found, use the original resource entry as is.
  if (!success || !cache_entry.is_dirty()) {
    callback.Run(FILE_ERROR_OK, entry.Pass());
    return;
  }

  // Gets the cache file path.
  const std::string& resource_id = entry->resource_id();
  const std::string& md5 = entry->file_specific_info().md5();
  cache_->GetFileOnUIThread(
      resource_id,
      md5,
      base::Bind(
          &FileSystem::CheckLocalModificationAndRunAfterGetCacheFile,
          weak_ptr_factory_.GetWeakPtr(),
          base::Passed(&entry),
          callback));
}

void FileSystem::CheckLocalModificationAndRunAfterGetCacheFile(
    scoped_ptr<ResourceEntry> entry,
    const GetResourceEntryCallback& callback,
    FileError error,
    const base::FilePath& local_cache_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  // When no dirty cache is found, use the original resource entry as is.
  if (error != FILE_ERROR_OK) {
    callback.Run(FILE_ERROR_OK, entry.Pass());
    return;
  }

  // If the cache is dirty, obtain the file info from the cache file itself.
  base::PlatformFileInfo* file_info = new base::PlatformFileInfo;
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(),
      FROM_HERE,
      base::Bind(&file_util::GetFileInfo,
                 local_cache_path,
                 base::Unretained(file_info)),
      base::Bind(&FileSystem::CheckLocalModificationAndRunAfterGetFileInfo,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Passed(&entry),
                 callback,
                 base::Owned(file_info)));
}

void FileSystem::CheckLocalModificationAndRunAfterGetFileInfo(
    scoped_ptr<ResourceEntry> entry,
    const GetResourceEntryCallback& callback,
    base::PlatformFileInfo* file_info,
    bool get_file_info_result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (!get_file_info_result) {
    callback.Run(FILE_ERROR_NOT_FOUND, scoped_ptr<ResourceEntry>());
    return;
  }

  PlatformFileInfoProto entry_file_info;
  util::ConvertPlatformFileInfoToResourceEntry(*file_info, &entry_file_info);
  *entry->mutable_file_info() = entry_file_info;
  callback.Run(FILE_ERROR_OK, entry.Pass());
}

}  // namespace drive
