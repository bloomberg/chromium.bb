// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/file_system.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/chromeos/drive/change_list_loader.h"
#include "chrome/browser/chromeos/drive/directory_loader.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/file_cache.h"
#include "chrome/browser/chromeos/drive/file_change.h"
#include "chrome/browser/chromeos/drive/file_system/copy_operation.h"
#include "chrome/browser/chromeos/drive/file_system/create_directory_operation.h"
#include "chrome/browser/chromeos/drive/file_system/create_file_operation.h"
#include "chrome/browser/chromeos/drive/file_system/download_operation.h"
#include "chrome/browser/chromeos/drive/file_system/get_file_for_saving_operation.h"
#include "chrome/browser/chromeos/drive/file_system/move_operation.h"
#include "chrome/browser/chromeos/drive/file_system/open_file_operation.h"
#include "chrome/browser/chromeos/drive/file_system/remove_operation.h"
#include "chrome/browser/chromeos/drive/file_system/search_operation.h"
#include "chrome/browser/chromeos/drive/file_system/touch_operation.h"
#include "chrome/browser/chromeos/drive/file_system/truncate_operation.h"
#include "chrome/browser/chromeos/drive/file_system_observer.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/drive/job_scheduler.h"
#include "chrome/browser/chromeos/drive/remove_stale_cache_files.h"
#include "chrome/browser/chromeos/drive/resource_entry_conversion.h"
#include "chrome/browser/chromeos/drive/search_metadata.h"
#include "chrome/browser/chromeos/drive/sync_client.h"
#include "chrome/browser/drive/drive_service_interface.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/drive/drive_api_parser.h"

using content::BrowserThread;

namespace drive {
namespace {

// Gets a ResourceEntry from the metadata, and overwrites its file info when the
// cached file is dirty.
FileError GetLocallyStoredResourceEntry(
    internal::ResourceMetadata* resource_metadata,
    internal::FileCache* cache,
    const base::FilePath& file_path,
    ResourceEntry* entry) {
  std::string local_id;
  FileError error = resource_metadata->GetIdByPath(file_path, &local_id);
  if (error != FILE_ERROR_OK)
    return error;

  error = resource_metadata->GetResourceEntryById(local_id, entry);
  if (error != FILE_ERROR_OK)
    return error;

  // For entries that will never be cached, use the original resource entry
  // as is.
  if (!entry->has_file_specific_info() ||
      entry->file_specific_info().is_hosted_document())
    return FILE_ERROR_OK;

  // When cache is not found, use the original resource entry as is.
  if (!entry->file_specific_info().has_cache_state())
    return FILE_ERROR_OK;

  // When cache is non-dirty and obsolete (old hash), use the original entry.
  if (!entry->file_specific_info().cache_state().is_dirty() &&
      entry->file_specific_info().md5() !=
      entry->file_specific_info().cache_state().md5())
    return FILE_ERROR_OK;

  // If there's a valid cache, obtain the file info from the cache file itself.
  base::FilePath local_cache_path;
  error = cache->GetFile(local_id, &local_cache_path);
  if (error != FILE_ERROR_OK)
    return error;

  base::File::Info file_info;
  if (!base::GetFileInfo(local_cache_path, &file_info))
    return FILE_ERROR_NOT_FOUND;

  entry->mutable_file_info()->set_size(file_info.size);
  return FILE_ERROR_OK;
}

// Runs the callback with parameters.
void RunGetResourceEntryCallback(const GetResourceEntryCallback& callback,
                                 scoped_ptr<ResourceEntry> entry,
                                 FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (error != FILE_ERROR_OK)
    entry.reset();
  callback.Run(error, entry.Pass());
}

// Used to implement Pin().
FileError PinInternal(internal::ResourceMetadata* resource_metadata,
                      internal::FileCache* cache,
                      const base::FilePath& file_path,
                      std::string* local_id) {
  FileError error = resource_metadata->GetIdByPath(file_path, local_id);
  if (error != FILE_ERROR_OK)
    return error;

  ResourceEntry entry;
  error = resource_metadata->GetResourceEntryById(*local_id, &entry);
  if (error != FILE_ERROR_OK)
    return error;

  // TODO(hashimoto): Support pinning directories. crbug.com/127831
  if (entry.file_info().is_directory())
    return FILE_ERROR_NOT_A_FILE;

  return cache->Pin(*local_id);
}

// Used to implement Unpin().
FileError UnpinInternal(internal::ResourceMetadata* resource_metadata,
                        internal::FileCache* cache,
                        const base::FilePath& file_path,
                        std::string* local_id) {
  FileError error = resource_metadata->GetIdByPath(file_path, local_id);
  if (error != FILE_ERROR_OK)
    return error;

  return cache->Unpin(*local_id);
}

// Used to implement MarkCacheFileAsMounted().
FileError MarkCacheFileAsMountedInternal(
    internal::ResourceMetadata* resource_metadata,
    internal::FileCache* cache,
    const base::FilePath& drive_file_path,
    base::FilePath* cache_file_path) {
  std::string local_id;
  FileError error = resource_metadata->GetIdByPath(drive_file_path, &local_id);
  if (error != FILE_ERROR_OK)
    return error;

  return cache->MarkAsMounted(local_id, cache_file_path);
}

// Runs the callback with arguments.
void RunMarkMountedCallback(const MarkMountedCallback& callback,
                            base::FilePath* cache_file_path,
                            FileError error) {
  DCHECK(!callback.is_null());
  callback.Run(error, *cache_file_path);
}

// Callback for ResourceMetadata::GetLargestChangestamp.
// |callback| must not be null.
void OnGetLargestChangestamp(
    FileSystemMetadata metadata,  // Will be modified.
    const GetFilesystemMetadataCallback& callback,
    const int64* largest_changestamp,
    FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  metadata.largest_changestamp = *largest_changestamp;
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

// Clears |resource_metadata| and |cache|.
FileError ResetOnBlockingPool(internal::ResourceMetadata* resource_metadata,
                              internal::FileCache* cache) {
  FileError error = resource_metadata->Reset();
  if (error != FILE_ERROR_OK)
    return error;
 return cache->ClearAll() ? FILE_ERROR_OK : FILE_ERROR_FAILED;
}

// Part of GetPathFromResourceId().
// Obtains |file_path| from |resource_id|. The function should be run on the
// blocking pool.
FileError GetPathFromResourceIdOnBlockingPool(
    internal::ResourceMetadata* resource_metadata,
    const std::string& resource_id,
    base::FilePath* file_path) {
  std::string local_id;
  const FileError error =
      resource_metadata->GetIdByResourceId(resource_id, &local_id);
  if (error != FILE_ERROR_OK)
    return error;
  return resource_metadata->GetFilePath(local_id, file_path);
}

// Part of GetPathFromResourceId().
// Called when GetPathFromResourceIdInBlockingPool is complete.
void GetPathFromResourceIdAfterGetPath(base::FilePath* file_path,
                                       const GetFilePathCallback& callback,
                                       FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  callback.Run(error, *file_path);
}

// Excludes hosted documents from the given entries.
// Used to implement ReadDirectory().
void FilterHostedDocuments(const ReadDirectoryEntriesCallback& callback,
                           scoped_ptr<ResourceEntryVector> entries) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (entries) {
    // TODO(kinaba): Stop handling hide_hosted_docs here. crbug.com/256520.
    scoped_ptr<ResourceEntryVector> filtered(new ResourceEntryVector);
    for (size_t i = 0; i < entries->size(); ++i) {
      if (entries->at(i).file_specific_info().is_hosted_document()) {
        continue;
      }
      filtered->push_back(entries->at(i));
    }
    entries.swap(filtered);
  }
  callback.Run(entries.Pass());
}

// Adapter for using FileOperationCallback as google_apis::EntryActionCallback.
void RunFileOperationCallbackAsEntryActionCallback(
    const FileOperationCallback& callback,
    google_apis::GDataErrorCode error) {
  callback.Run(GDataToFileError(error));
}

}  // namespace

struct FileSystem::CreateDirectoryParams {
  base::FilePath directory_path;
  bool is_exclusive;
  bool is_recursive;
  FileOperationCallback callback;
};

FileSystem::FileSystem(
    PrefService* pref_service,
    EventLogger* logger,
    internal::FileCache* cache,
    DriveServiceInterface* drive_service,
    JobScheduler* scheduler,
    internal::ResourceMetadata* resource_metadata,
    base::SequencedTaskRunner* blocking_task_runner,
    const base::FilePath& temporary_file_directory)
    : pref_service_(pref_service),
      logger_(logger),
      cache_(cache),
      drive_service_(drive_service),
      scheduler_(scheduler),
      resource_metadata_(resource_metadata),
      last_update_check_error_(FILE_ERROR_OK),
      blocking_task_runner_(blocking_task_runner),
      temporary_file_directory_(temporary_file_directory),
      weak_ptr_factory_(this) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  ResetComponents();
}

FileSystem::~FileSystem() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  directory_loader_->RemoveObserver(this);
  change_list_loader_->RemoveObserver(this);
}

void FileSystem::Reset(const FileOperationCallback& callback) {
  // Discard the current loader and operation objects and renew them. This is to
  // avoid that changes initiated before the metadata reset is applied after the
  // reset, which may cause an inconsistent state.
  // TODO(kinaba): callbacks held in the subcomponents are discarded. We might
  // want to have a way to abort and flush callbacks in in-flight operations.
  ResetComponents();

  base::PostTaskAndReplyWithResult(
      blocking_task_runner_,
      FROM_HERE,
      base::Bind(&ResetOnBlockingPool, resource_metadata_, cache_),
      callback);
}

void FileSystem::ResetComponents() {
  file_system::OperationDelegate* delegate = this;

  about_resource_loader_.reset(new internal::AboutResourceLoader(scheduler_));
  loader_controller_.reset(new internal::LoaderController);
  change_list_loader_.reset(new internal::ChangeListLoader(
      logger_,
      blocking_task_runner_.get(),
      resource_metadata_,
      scheduler_,
      about_resource_loader_.get(),
      loader_controller_.get()));
  change_list_loader_->AddObserver(this);
  directory_loader_.reset(new internal::DirectoryLoader(
      logger_,
      blocking_task_runner_.get(),
      resource_metadata_,
      scheduler_,
      about_resource_loader_.get(),
      loader_controller_.get()));
  directory_loader_->AddObserver(this);

  sync_client_.reset(new internal::SyncClient(blocking_task_runner_.get(),
                                              delegate,
                                              scheduler_,
                                              resource_metadata_,
                                              cache_,
                                              loader_controller_.get(),
                                              temporary_file_directory_));

  copy_operation_.reset(
      new file_system::CopyOperation(
          blocking_task_runner_.get(),
          delegate,
          scheduler_,
          resource_metadata_,
          cache_,
          drive_service_->GetResourceIdCanonicalizer()));
  create_directory_operation_.reset(new file_system::CreateDirectoryOperation(
      blocking_task_runner_.get(), delegate, resource_metadata_));
  create_file_operation_.reset(
      new file_system::CreateFileOperation(blocking_task_runner_.get(),
                                           delegate,
                                           resource_metadata_));
  move_operation_.reset(
      new file_system::MoveOperation(blocking_task_runner_.get(),
                                     delegate,
                                     resource_metadata_));
  open_file_operation_.reset(
      new file_system::OpenFileOperation(blocking_task_runner_.get(),
                                         delegate,
                                         scheduler_,
                                         resource_metadata_,
                                         cache_,
                                         temporary_file_directory_));
  remove_operation_.reset(
      new file_system::RemoveOperation(blocking_task_runner_.get(),
                                       delegate,
                                       resource_metadata_,
                                       cache_));
  touch_operation_.reset(new file_system::TouchOperation(
      blocking_task_runner_.get(), delegate, resource_metadata_));
  truncate_operation_.reset(
      new file_system::TruncateOperation(blocking_task_runner_.get(),
                                         delegate,
                                         scheduler_,
                                         resource_metadata_,
                                         cache_,
                                         temporary_file_directory_));
  download_operation_.reset(
      new file_system::DownloadOperation(blocking_task_runner_.get(),
                                         delegate,
                                         scheduler_,
                                         resource_metadata_,
                                         cache_,
                                         temporary_file_directory_));
  search_operation_.reset(new file_system::SearchOperation(
      blocking_task_runner_.get(), scheduler_, resource_metadata_,
      loader_controller_.get()));
  get_file_for_saving_operation_.reset(
      new file_system::GetFileForSavingOperation(logger_,
                                                 blocking_task_runner_.get(),
                                                 delegate,
                                                 scheduler_,
                                                 resource_metadata_,
                                                 cache_,
                                                 temporary_file_directory_));
}

void FileSystem::CheckForUpdates() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DVLOG(1) << "CheckForUpdates";

  change_list_loader_->CheckForUpdates(
      base::Bind(&FileSystem::OnUpdateChecked, weak_ptr_factory_.GetWeakPtr()));
}

void FileSystem::OnUpdateChecked(FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DVLOG(1) << "CheckForUpdates finished: " << FileErrorToString(error);
  last_update_check_time_ = base::Time::Now();
  last_update_check_error_ = error;
}

void FileSystem::AddObserver(FileSystemObserver* observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  observers_.AddObserver(observer);
}

void FileSystem::RemoveObserver(FileSystemObserver* observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  observers_.RemoveObserver(observer);
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
                      bool preserve_last_modified,
                      const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  copy_operation_->Copy(
      src_file_path, dest_file_path, preserve_last_modified, callback);
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

  CreateDirectoryParams params;
  params.directory_path = directory_path;
  params.is_exclusive = is_exclusive;
  params.is_recursive = is_recursive;
  params.callback = callback;

  // Ensure its parent directory is loaded to the local metadata.
  ReadDirectory(directory_path.DirName(),
                ReadDirectoryEntriesCallback(),
                base::Bind(&FileSystem::CreateDirectoryAfterRead,
                           weak_ptr_factory_.GetWeakPtr(), params));
}

void FileSystem::CreateDirectoryAfterRead(const CreateDirectoryParams& params,
                                          FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!params.callback.is_null());

  DVLOG_IF(1, error != FILE_ERROR_OK) << "ReadDirectory failed. "
                                      << FileErrorToString(error);

  create_directory_operation_->CreateDirectory(
      params.directory_path, params.is_exclusive, params.is_recursive,
      params.callback);
}

void FileSystem::CreateFile(const base::FilePath& file_path,
                            bool is_exclusive,
                            const std::string& mime_type,
                            const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  create_file_operation_->CreateFile(
      file_path, is_exclusive, mime_type, callback);
}

void FileSystem::TouchFile(const base::FilePath& file_path,
                           const base::Time& last_access_time,
                           const base::Time& last_modified_time,
                           const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  touch_operation_->TouchFile(
      file_path, last_access_time, last_modified_time, callback);
}

void FileSystem::TruncateFile(const base::FilePath& file_path,
                              int64 length,
                              const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  truncate_operation_->Truncate(file_path, length, callback);
}

void FileSystem::Pin(const base::FilePath& file_path,
                     const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  std::string* local_id = new std::string;
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_,
      FROM_HERE,
      base::Bind(&PinInternal, resource_metadata_, cache_, file_path, local_id),
      base::Bind(&FileSystem::FinishPin,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback,
                 base::Owned(local_id)));
}

void FileSystem::FinishPin(const FileOperationCallback& callback,
                           const std::string* local_id,
                           FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (error == FILE_ERROR_OK)
    sync_client_->AddFetchTask(*local_id);
  callback.Run(error);
}

void FileSystem::Unpin(const base::FilePath& file_path,
                       const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  std::string* local_id = new std::string;
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_,
      FROM_HERE,
      base::Bind(&UnpinInternal,
                 resource_metadata_,
                 cache_,
                 file_path,
                 local_id),
      base::Bind(&FileSystem::FinishUnpin,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback,
                 base::Owned(local_id)));
}

void FileSystem::FinishUnpin(const FileOperationCallback& callback,
                             const std::string* local_id,
                             FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (error == FILE_ERROR_OK)
    sync_client_->RemoveFetchTask(*local_id);
  callback.Run(error);
}

void FileSystem::GetFile(const base::FilePath& file_path,
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

void FileSystem::GetFileForSaving(const base::FilePath& file_path,
                                  const GetFileCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  get_file_for_saving_operation_->GetFileForSaving(file_path, callback);
}

base::Closure FileSystem::GetFileContent(
    const base::FilePath& file_path,
    const GetFileContentInitializedCallback& initialized_callback,
    const google_apis::GetContentCallback& get_content_callback,
    const FileOperationCallback& completion_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!initialized_callback.is_null());
  DCHECK(!get_content_callback.is_null());
  DCHECK(!completion_callback.is_null());

  return download_operation_->EnsureFileDownloadedByPath(
      file_path,
      ClientContext(USER_INITIATED),
      initialized_callback,
      get_content_callback,
      base::Bind(&GetFileCallbackToFileOperationCallbackAdapter,
                 completion_callback));
}

void FileSystem::GetResourceEntry(
    const base::FilePath& file_path,
    const GetResourceEntryCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  ReadDirectory(file_path.DirName(),
                ReadDirectoryEntriesCallback(),
                base::Bind(&FileSystem::GetResourceEntryAfterRead,
                           weak_ptr_factory_.GetWeakPtr(),
                           file_path,
                           callback));
}

void FileSystem::GetResourceEntryAfterRead(
    const base::FilePath& file_path,
    const GetResourceEntryCallback& callback,
    FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  DVLOG_IF(1, error != FILE_ERROR_OK) << "ReadDirectory failed. "
                                      << FileErrorToString(error);

  scoped_ptr<ResourceEntry> entry(new ResourceEntry);
  ResourceEntry* entry_ptr = entry.get();
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_,
      FROM_HERE,
      base::Bind(&GetLocallyStoredResourceEntry,
                 resource_metadata_,
                 cache_,
                 file_path,
                 entry_ptr),
      base::Bind(&RunGetResourceEntryCallback, callback, base::Passed(&entry)));
}

void FileSystem::ReadDirectory(
    const base::FilePath& directory_path,
    const ReadDirectoryEntriesCallback& entries_callback_in,
    const FileOperationCallback& completion_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!completion_callback.is_null());

  const bool hide_hosted_docs =
      pref_service_->GetBoolean(prefs::kDisableDriveHostedFiles);
  ReadDirectoryEntriesCallback entries_callback = entries_callback_in;
  if (!entries_callback.is_null() && hide_hosted_docs)
    entries_callback = base::Bind(&FilterHostedDocuments, entries_callback);

  directory_loader_->ReadDirectory(
      directory_path, entries_callback, completion_callback);

  // Also start loading all of the user's contents.
  change_list_loader_->LoadIfNeeded(
      base::Bind(&util::EmptyFileOperationCallback));
}

void FileSystem::GetAvailableSpace(
    const GetAvailableSpaceCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  about_resource_loader_->GetAboutResource(
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

  FileError error = GDataToFileError(status);
  if (error != FILE_ERROR_OK) {
    callback.Run(error, -1, -1);
    return;
  }
  DCHECK(about_resource);

  callback.Run(FILE_ERROR_OK,
               about_resource->quota_bytes_total(),
               about_resource->quota_bytes_used());
}

void FileSystem::GetShareUrl(const base::FilePath& file_path,
                             const GURL& embed_origin,
                             const GetShareUrlCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  // Resolve the resource id.
  ResourceEntry* entry = new ResourceEntry;
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(),
      FROM_HERE,
      base::Bind(&internal::ResourceMetadata::GetResourceEntryByPath,
                 base::Unretained(resource_metadata_),
                 file_path,
                 entry),
      base::Bind(&FileSystem::GetShareUrlAfterGetResourceEntry,
                 weak_ptr_factory_.GetWeakPtr(),
                 file_path,
                 embed_origin,
                 callback,
                 base::Owned(entry)));
}

void FileSystem::GetShareUrlAfterGetResourceEntry(
    const base::FilePath& file_path,
    const GURL& embed_origin,
    const GetShareUrlCallback& callback,
    ResourceEntry* entry,
    FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (error != FILE_ERROR_OK) {
    callback.Run(error, GURL());
    return;
  }
  if (entry->resource_id().empty()) {
    // This entry does not exist on the server. Just return.
    callback.Run(FILE_ERROR_FAILED, GURL());
    return;
  }

  scheduler_->GetShareUrl(
      entry->resource_id(),
      embed_origin,
      ClientContext(USER_INITIATED),
      base::Bind(&FileSystem::OnGetResourceEntryForGetShareUrl,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback));
}

void FileSystem::OnGetResourceEntryForGetShareUrl(
    const GetShareUrlCallback& callback,
    google_apis::GDataErrorCode status,
    const GURL& share_url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  FileError error = GDataToFileError(status);
  if (error != FILE_ERROR_OK) {
    callback.Run(error, GURL());
    return;
  }

  if (share_url.is_empty()) {
    callback.Run(FILE_ERROR_FAILED, GURL());
    return;
  }

  callback.Run(FILE_ERROR_OK, share_url);
}

void FileSystem::Search(const std::string& search_query,
                        const GURL& next_link,
                        const SearchCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  search_operation_->Search(search_query, next_link, callback);
}

void FileSystem::SearchMetadata(const std::string& query,
                                int options,
                                int at_most_num_matches,
                                const SearchMetadataCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // TODO(satorux): Stop handling hide_hosted_docs here. crbug.com/256520.
  if (pref_service_->GetBoolean(prefs::kDisableDriveHostedFiles))
    options |= SEARCH_METADATA_EXCLUDE_HOSTED_DOCUMENTS;

  drive::internal::SearchMetadata(blocking_task_runner_,
                                  resource_metadata_,
                                  query,
                                  options,
                                  at_most_num_matches,
                                  callback);
}

void FileSystem::OnFileChangedByOperation(const FileChange& changed_files) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  FOR_EACH_OBSERVER(
      FileSystemObserver, observers_, OnFileChanged(changed_files));
}

void FileSystem::OnEntryUpdatedByOperation(const std::string& local_id) {
  sync_client_->AddUpdateTask(ClientContext(USER_INITIATED), local_id);
}

void FileSystem::OnDriveSyncError(file_system::DriveSyncErrorType type,
                                  const std::string& local_id) {
  base::FilePath* file_path = new base::FilePath;
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_,
      FROM_HERE,
      base::Bind(&internal::ResourceMetadata::GetFilePath,
                 base::Unretained(resource_metadata_),
                 local_id,
                 file_path),
      base::Bind(&FileSystem::OnDriveSyncErrorAfterGetFilePath,
                 weak_ptr_factory_.GetWeakPtr(),
                 type,
                 base::Owned(file_path)));
}

void FileSystem::OnDriveSyncErrorAfterGetFilePath(
    file_system::DriveSyncErrorType type,
    const base::FilePath* file_path,
    FileError error) {
  if (error != FILE_ERROR_OK)
    return;
  FOR_EACH_OBSERVER(FileSystemObserver,
                    observers_,
                    OnDriveSyncError(type, *file_path));
}

bool FileSystem::WaitForSyncComplete(const std::string& local_id,
                                     const FileOperationCallback& callback) {
  return sync_client_->WaitForUpdateTaskToComplete(local_id, callback);
}

void FileSystem::OnDirectoryReloaded(const base::FilePath& directory_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  FOR_EACH_OBSERVER(
      FileSystemObserver, observers_, OnDirectoryChanged(directory_path));
}

void FileSystem::OnFileChanged(const FileChange& changed_files) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  FOR_EACH_OBSERVER(
      FileSystemObserver, observers_, OnFileChanged(changed_files));
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

  int64* largest_changestamp = new int64(0);
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_,
      FROM_HERE,
      base::Bind(&internal::ResourceMetadata::GetLargestChangestamp,
                 base::Unretained(resource_metadata_), largest_changestamp),
      base::Bind(&OnGetLargestChangestamp, metadata, callback,
                 base::Owned(largest_changestamp)));
}

void FileSystem::MarkCacheFileAsMounted(
    const base::FilePath& drive_file_path,
    const MarkMountedCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  base::FilePath* cache_file_path = new base::FilePath;
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_,
      FROM_HERE,
      base::Bind(&MarkCacheFileAsMountedInternal,
                 resource_metadata_,
                 cache_,
                 drive_file_path,
                 cache_file_path),
      base::Bind(&RunMarkMountedCallback,
                 callback,
                 base::Owned(cache_file_path)));
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

  base::PostTaskAndReplyWithResult(
      blocking_task_runner_,
      FROM_HERE,
      base::Bind(&internal::FileCache::MarkAsUnmounted,
                 base::Unretained(cache_),
                 cache_file_path),
      callback);
}

void FileSystem::AddPermission(const base::FilePath& drive_file_path,
                               const std::string& email,
                               google_apis::drive::PermissionRole role,
                               const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  // Resolve the resource id.
  ResourceEntry* const entry = new ResourceEntry;
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(),
      FROM_HERE,
      base::Bind(&internal::ResourceMetadata::GetResourceEntryByPath,
                 base::Unretained(resource_metadata_),
                 drive_file_path,
                 entry),
      base::Bind(&FileSystem::AddPermissionAfterGetResourceEntry,
                 weak_ptr_factory_.GetWeakPtr(),
                 email,
                 role,
                 callback,
                 base::Owned(entry)));
}

void FileSystem::AddPermissionAfterGetResourceEntry(
    const std::string& email,
    google_apis::drive::PermissionRole role,
    const FileOperationCallback& callback,
    ResourceEntry* entry,
    FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (error != FILE_ERROR_OK) {
    callback.Run(error);
    return;
  }

  scheduler_->AddPermission(
      entry->resource_id(),
      email,
      role,
      base::Bind(&RunFileOperationCallbackAsEntryActionCallback, callback));
}

void FileSystem::OpenFile(const base::FilePath& file_path,
                          OpenMode open_mode,
                          const std::string& mime_type,
                          const OpenFileCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  open_file_operation_->OpenFile(file_path, open_mode, mime_type, callback);
}

void FileSystem::GetPathFromResourceId(const std::string& resource_id,
                                       const GetFilePathCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  base::FilePath* const file_path = new base::FilePath();
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_,
      FROM_HERE,
      base::Bind(&GetPathFromResourceIdOnBlockingPool,
                 resource_metadata_,
                 resource_id,
                 file_path),
      base::Bind(&GetPathFromResourceIdAfterGetPath,
                 base::Owned(file_path),
                 callback));
}
}  // namespace drive
