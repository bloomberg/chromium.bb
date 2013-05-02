// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/file_system.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/message_loop_proxy.h"
#include "base/metrics/histogram.h"
#include "base/platform_file.h"
#include "base/prefs/pref_change_registrar.h"
#include "base/prefs/pref_service.h"
#include "base/stringprintf.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/chromeos/drive/change_list_loader.h"
#include "chrome/browser/chromeos/drive/change_list_processor.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/file_cache.h"
#include "chrome/browser/chromeos/drive/file_system_observer.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/drive/job_scheduler.h"
#include "chrome/browser/chromeos/drive/resource_entry_conversion.h"
#include "chrome/browser/chromeos/drive/search_metadata.h"
#include "chrome/browser/google_apis/drive_api_parser.h"
#include "chrome/browser/google_apis/drive_api_util.h"
#include "chrome/browser/google_apis/drive_service_interface.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"

using content::BrowserThread;

namespace drive {
namespace {

const char kMimeTypeJson[] = "application/json";

//================================ Helper functions ============================

// Creates a temporary JSON file representing a document with |edit_url|
// and |resource_id| under |document_dir| on blocking pool.
FileError CreateDocumentJsonFileOnBlockingPool(
    const base::FilePath& document_dir,
    const GURL& edit_url,
    const std::string& resource_id,
    base::FilePath* temp_file_path) {
  DCHECK(temp_file_path);

  FileError error = FILE_ERROR_FAILED;

  if (file_util::CreateTemporaryFileInDir(document_dir, temp_file_path)) {
    std::string document_content = base::StringPrintf(
        "{\"url\": \"%s\", \"resource_id\": \"%s\"}",
        edit_url.spec().c_str(), resource_id.c_str());
    int document_size = static_cast<int>(document_content.size());
    if (file_util::WriteFile(*temp_file_path, document_content.data(),
                             document_size) == document_size) {
      error = FILE_ERROR_OK;
    }
  }

  if (error != FILE_ERROR_OK)
    temp_file_path->clear();
  return error;
}

// Helper function for binding |path| to GetEntryInfoWithFilePathCallback and
// create GetEntryInfoCallback.
void RunGetEntryInfoWithFilePathCallback(
    const GetEntryInfoWithFilePathCallback& callback,
    const base::FilePath& path,
    FileError error,
    scoped_ptr<ResourceEntry> entry) {
  DCHECK(!callback.is_null());
  callback.Run(error, path, entry.Pass());
}

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
    const std::string& unused_mime_type,
    DriveFileType unused_file_type) {
  callback.Run(error);
}

// Wraps |callback| and always passes FILE_ERROR_OK when invoked.
// This is used for adopting |callback| to wait for a non-fatal operation.
void IgnoreError(const FileOperationCallback& callback, FileError error) {
  callback.Run(FILE_ERROR_OK);
}

// Creates a file with unique name in |dir| and stores the path to |temp_file|.
// Additionally, sets the permission of the file to allow read access from
// others and group member users (i.e, "-rw-r--r--").
// We need this wrapper because Drive cache files may be read from other
// processes (e.g., cros_disks for mounting zip files).
//
// Must be called on the blocking pool.
bool CreateTemporaryReadableFileInDir(const base::FilePath& dir,
                                      base::FilePath* temp_file) {
  if (!file_util::CreateTemporaryFileInDir(dir, temp_file))
    return false;
  return file_util::SetPosixFilePermissions(
      *temp_file,
      file_util::FILE_PERMISSION_READ_BY_USER |
      file_util::FILE_PERMISSION_WRITE_BY_USER |
      file_util::FILE_PERMISSION_READ_BY_GROUP |
      file_util::FILE_PERMISSION_READ_BY_OTHERS);
}

}  // namespace

// FileSystem::GetFileCompleteForOpenParams struct implementation.
struct FileSystem::GetFileCompleteForOpenParams {
  GetFileCompleteForOpenParams(const OpenFileCallback& callback,
                               const std::string& resource_id,
                               const std::string& md5);
  OpenFileCallback callback;
  std::string resource_id;
  std::string md5;
};

FileSystem::GetFileCompleteForOpenParams::GetFileCompleteForOpenParams(
    const OpenFileCallback& callback,
    const std::string& resource_id,
    const std::string& md5)
    : callback(callback),
      resource_id(resource_id),
      md5(md5) {
}

// FileSystem::GetResolvedFileParams struct implementation.
struct FileSystem::GetResolvedFileParams {
  GetResolvedFileParams(
      const base::FilePath& drive_file_path,
      const DriveClientContext& context,
      scoped_ptr<ResourceEntry> entry,
      const GetFileContentInitializedCallback& initialized_callback,
      const GetFileCallback& get_file_callback,
      const google_apis::GetContentCallback& get_content_callback)
      : drive_file_path(drive_file_path),
        context(context),
        entry(entry.Pass()),
        initialized_callback(initialized_callback),
        get_file_callback(get_file_callback),
        get_content_callback(get_content_callback) {
    DCHECK(!get_file_callback.is_null());
    DCHECK(this->entry);
  }

  void OnError(FileError error) {
    get_file_callback.Run(
        error, base::FilePath(), std::string(), REGULAR_FILE);
  }

  void OnCacheFileFound(const base::FilePath& local_file_path) {
    if (initialized_callback.is_null()) {
      return;
    }

    scoped_ptr<ResourceEntry> new_entry(new ResourceEntry(*entry));
    initialized_callback.Run(FILE_ERROR_OK,
                             new_entry.Pass(),
                             local_file_path,
                             base::Closure());
  }

  void OnStartDownloading(const base::Closure& cancel_download_closure) {
    if (initialized_callback.is_null()) {
      return;
    }

    scoped_ptr<ResourceEntry> new_entry(new ResourceEntry(*entry));
    initialized_callback.Run(FILE_ERROR_OK,
                             new_entry.Pass(),
                             base::FilePath(),
                             cancel_download_closure);
  }

  void OnComplete(const base::FilePath& local_file_path) {
    if (entry->file_specific_info().is_hosted_document()) {
      get_file_callback.Run(
          FILE_ERROR_OK, local_file_path, kMimeTypeJson, HOSTED_DOCUMENT);
    } else {
      get_file_callback.Run(
          FILE_ERROR_OK, local_file_path,
          entry->file_specific_info().content_mime_type(), REGULAR_FILE);
    }
  }

  const base::FilePath drive_file_path;
  const DriveClientContext context;
  scoped_ptr<ResourceEntry> entry;
  const GetFileContentInitializedCallback initialized_callback;
  const GetFileCallback get_file_callback;
  const google_apis::GetContentCallback get_content_callback;
};

// FileSystem::AddUploadedFileParams implementation.
struct FileSystem::AddUploadedFileParams {
  AddUploadedFileParams(const base::FilePath& file_content_path,
                        const FileOperationCallback& callback,
                        const std::string& resource_id,
                        const std::string& md5)
      : file_content_path(file_content_path),
        callback(callback),
        resource_id(resource_id),
        md5(md5) {
  }

  base::FilePath file_content_path;
  FileOperationCallback callback;
  std::string resource_id;
  std::string md5;
};


// FileSystem class implementation.

FileSystem::FileSystem(
    Profile* profile,
    FileCache* cache,
    google_apis::DriveServiceInterface* drive_service,
    JobScheduler* scheduler,
    DriveWebAppsRegistry* webapps_registry,
    internal::ResourceMetadata* resource_metadata,
    base::SequencedTaskRunner* blocking_task_runner)
    : profile_(profile),
      cache_(cache),
      drive_service_(drive_service),
      scheduler_(scheduler),
      webapps_registry_(webapps_registry),
      resource_metadata_(resource_metadata),
      last_update_check_error_(FILE_ERROR_OK),
      hide_hosted_docs_(false),
      blocking_task_runner_(blocking_task_runner),
      weak_ptr_factory_(this) {
  // Should be created from the file browser extension API on UI thread.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FileSystem::Reload() {
  resource_metadata_->Reset(base::Bind(&FileSystem::ReloadAfterReset,
                                       weak_ptr_factory_.GetWeakPtr()));
}

void FileSystem::Initialize() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  SetupChangeListLoader();

  // Allocate the drive operation handlers.
  drive_operations_.Init(scheduler_,
                         this,  // FileSystemInterface
                         cache_,
                         resource_metadata_,
                         blocking_task_runner_,
                         this);  // OperationObserver

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
  change_list_loader_.reset(new ChangeListLoader(resource_metadata_,
                                                 scheduler_,
                                                 webapps_registry_));
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
  // This should be called from UI thread, from DriveSystemService shutdown.
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

void FileSystem::GetEntryInfoByResourceId(
    const std::string& resource_id,
    const GetEntryInfoWithFilePathCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!resource_id.empty());
  DCHECK(!callback.is_null());

  resource_metadata_->GetEntryInfoByResourceId(
      resource_id,
      base::Bind(&FileSystem::GetEntryInfoByResourceIdAfterGetEntry,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback));
}

void FileSystem::GetEntryInfoByResourceIdAfterGetEntry(
    const GetEntryInfoWithFilePathCallback& callback,
    FileError error,
    const base::FilePath& file_path,
    scoped_ptr<ResourceEntry> entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (error != FILE_ERROR_OK) {
    callback.Run(error, base::FilePath(), scoped_ptr<ResourceEntry>());
    return;
  }
  DCHECK(entry.get());

  CheckLocalModificationAndRun(
      entry.Pass(),
      base::Bind(&RunGetEntryInfoWithFilePathCallback,
                 callback,
                 file_path));
}

void FileSystem::TransferFileFromRemoteToLocal(
    const base::FilePath& remote_src_file_path,
    const base::FilePath& local_dest_file_path,
    const FileOperationCallback& callback) {

  drive_operations_.TransferFileFromRemoteToLocal(remote_src_file_path,
                                                  local_dest_file_path,
                                                  callback);
}

void FileSystem::TransferFileFromLocalToRemote(
    const base::FilePath& local_src_file_path,
    const base::FilePath& remote_dest_file_path,
    const FileOperationCallback& callback) {

  drive_operations_.TransferFileFromLocalToRemote(local_src_file_path,
                                                  remote_dest_file_path,
                                                  callback);
}

void FileSystem::Copy(const base::FilePath& src_file_path,
                      const base::FilePath& dest_file_path,
                      const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  drive_operations_.Copy(src_file_path, dest_file_path, callback);
}

void FileSystem::Move(const base::FilePath& src_file_path,
                      const base::FilePath& dest_file_path,
                      const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  drive_operations_.Move(src_file_path, dest_file_path, callback);
}

void FileSystem::Remove(const base::FilePath& file_path,
                        bool is_recursive,
                        const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  drive_operations_.Remove(file_path, is_recursive, callback);
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

  drive_operations_.CreateDirectory(
      directory_path, is_exclusive, is_recursive, callback);
}

void FileSystem::CreateFile(const base::FilePath& file_path,
                            bool is_exclusive,
                            const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  drive_operations_.CreateFile(file_path, is_exclusive, callback);
}

void FileSystem::Pin(const base::FilePath& file_path,
                     const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  GetEntryInfoByPath(file_path,
                     base::Bind(&FileSystem::PinAfterGetEntryInfoByPath,
                                weak_ptr_factory_.GetWeakPtr(),
                                callback));
}

void FileSystem::PinAfterGetEntryInfoByPath(
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

  cache_->Pin(entry->resource_id(), entry->file_specific_info().file_md5(),
              callback);
}

void FileSystem::Unpin(const base::FilePath& file_path,
                       const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  GetEntryInfoByPath(file_path,
                     base::Bind(&FileSystem::UnpinAfterGetEntryInfoByPath,
                                weak_ptr_factory_.GetWeakPtr(),
                                callback));
}

void FileSystem::UnpinAfterGetEntryInfoByPath(
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

  cache_->Unpin(entry->resource_id(), entry->file_specific_info().file_md5(),
                callback);
}

void FileSystem::GetFileByPath(const base::FilePath& file_path,
                               const GetFileCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  resource_metadata_->GetEntryInfoByPath(
      file_path,
      base::Bind(&FileSystem::OnGetEntryInfoCompleteForGetFileByPath,
                 weak_ptr_factory_.GetWeakPtr(),
                 file_path,
                 callback));
}

void FileSystem::OnGetEntryInfoCompleteForGetFileByPath(
    const base::FilePath& file_path,
    const GetFileCallback& callback,
    FileError error,
    scoped_ptr<ResourceEntry> entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (error != FILE_ERROR_OK) {
    callback.Run(error, base::FilePath(), std::string(), REGULAR_FILE);
    return;
  }
  DCHECK(entry);

  GetResolvedFileByPath(
      make_scoped_ptr(new GetResolvedFileParams(
          file_path,
          DriveClientContext(USER_INITIATED),
          entry.Pass(),
          GetFileContentInitializedCallback(),
          callback,
          google_apis::GetContentCallback())));
}

void FileSystem::GetFileByResourceId(
    const std::string& resource_id,
    const DriveClientContext& context,
    const GetFileCallback& get_file_callback,
    const google_apis::GetContentCallback& get_content_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!resource_id.empty());
  DCHECK(!get_file_callback.is_null());

  resource_metadata_->GetEntryInfoByResourceId(
      resource_id,
      base::Bind(&FileSystem::GetFileByResourceIdAfterGetEntry,
                 weak_ptr_factory_.GetWeakPtr(),
                 context,
                 get_file_callback,
                 get_content_callback));
}

void FileSystem::GetFileByResourceIdAfterGetEntry(
    const DriveClientContext& context,
    const GetFileCallback& get_file_callback,
    const google_apis::GetContentCallback& get_content_callback,
    FileError error,
    const base::FilePath& file_path,
    scoped_ptr<ResourceEntry> entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!get_file_callback.is_null());

  if (error != FILE_ERROR_OK) {
    get_file_callback.Run(FILE_ERROR_NOT_FOUND,
                          base::FilePath(),
                          std::string(),
                          REGULAR_FILE);
    return;
  }

  GetResolvedFileByPath(
      make_scoped_ptr(new GetResolvedFileParams(
          file_path,
          context,
          entry.Pass(),
          GetFileContentInitializedCallback(),
          get_file_callback,
          get_content_callback)));
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

  resource_metadata_->GetEntryInfoByPath(
      file_path,
      base::Bind(&FileSystem::GetFileContentByPathAfterGetEntry,
                 weak_ptr_factory_.GetWeakPtr(),
                 file_path,
                 initialized_callback,
                 get_content_callback,
                 completion_callback));
}

void FileSystem::GetFileContentByPathAfterGetEntry(
    const base::FilePath& file_path,
    const GetFileContentInitializedCallback& initialized_callback,
    const google_apis::GetContentCallback& get_content_callback,
    const FileOperationCallback& completion_callback,
    FileError error,
    scoped_ptr<ResourceEntry> entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!initialized_callback.is_null());
  DCHECK(!get_content_callback.is_null());
  DCHECK(!completion_callback.is_null());

  if (error != FILE_ERROR_OK) {
    completion_callback.Run(error);
    return;
  }

  DCHECK(entry);
  GetResolvedFileByPath(
      make_scoped_ptr(new GetResolvedFileParams(
          file_path,
          DriveClientContext(USER_INITIATED),
          entry.Pass(),
          initialized_callback,
          base::Bind(&GetFileCallbackToFileOperationCallbackAdapter,
                     completion_callback),
          get_content_callback)));
}

void FileSystem::GetEntryInfoByPath(const base::FilePath& file_path,
                                    const GetEntryInfoCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  // ResourceMetadata may know about the entry even if the resource
  // metadata is not yet fully loaded. For instance, ResourceMetadata()
  // always knows about the root directory. For "fast fetch"
  // (crbug.com/178348) to work, it's needed to delay the resource metadata
  // loading until the first call to ReadDirectoryByPath().
  resource_metadata_->GetEntryInfoByPath(
      file_path,
      base::Bind(&FileSystem::GetEntryInfoByPathAfterGetEntry1,
                 weak_ptr_factory_.GetWeakPtr(),
                 file_path,
                 callback));
}

void FileSystem::GetEntryInfoByPathAfterGetEntry1(
    const base::FilePath& file_path,
    const GetEntryInfoCallback& callback,
    FileError error,
    scoped_ptr<ResourceEntry> entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (error == FILE_ERROR_OK) {
    CheckLocalModificationAndRun(entry.Pass(), callback);
    return;
  }

  // Start loading if needed. Note that directory_fetch_info is empty here,
  // as we don't need to fetch the contents of a directory when we just need
  // to get an entry of the directory.
  change_list_loader_->LoadIfNeeded(
      DirectoryFetchInfo(),
      base::Bind(&FileSystem::GetEntryInfoByPathAfterLoad,
                 weak_ptr_factory_.GetWeakPtr(),
                 file_path,
                 callback));
}

void FileSystem::GetEntryInfoByPathAfterLoad(
    const base::FilePath& file_path,
    const GetEntryInfoCallback& callback,
    FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (error != FILE_ERROR_OK) {
    callback.Run(error, scoped_ptr<ResourceEntry>());
    return;
  }

  resource_metadata_->GetEntryInfoByPath(
      file_path,
      base::Bind(&FileSystem::GetEntryInfoByPathAfterGetEntry2,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback));
}

void FileSystem::GetEntryInfoByPathAfterGetEntry2(
    const GetEntryInfoCallback& callback,
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

  // As described in GetEntryInfoByPath(), ResourceMetadata may know
  // about the entry even if the file system is not yet fully loaded, hence we
  // should just ask ResourceMetadata first.
  resource_metadata_->GetEntryInfoByPath(
      directory_path,
      base::Bind(&FileSystem::ReadDirectoryByPathAfterGetEntry,
                 weak_ptr_factory_.GetWeakPtr(),
                 directory_path,
                 callback));
}

void FileSystem::ReadDirectoryByPathAfterGetEntry(
    const base::FilePath& directory_path,
    const ReadDirectoryWithSettingCallback& callback,
    FileError error,
    scoped_ptr<ResourceEntry> entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (error != FILE_ERROR_OK) {
    // If we don't know about the directory, start loading.
    change_list_loader_->LoadIfNeeded(
        DirectoryFetchInfo(),
        base::Bind(&FileSystem::ReadDirectoryByPathAfterLoad,
                   weak_ptr_factory_.GetWeakPtr(),
                   directory_path,
                   callback));
    return;
  }

  if (!entry->file_info().is_directory()) {
    callback.Run(FILE_ERROR_NOT_A_DIRECTORY,
                 hide_hosted_docs_,
                 scoped_ptr<ResourceEntryVector>());
    return;
  }

  // Pass the directory fetch info so we can fetch the contents of the
  // directory before loading change lists.
  DirectoryFetchInfo directory_fetch_info(
      entry->resource_id(),
      entry->directory_specific_info().changestamp());
  change_list_loader_->LoadIfNeeded(
      directory_fetch_info,
      base::Bind(&FileSystem::ReadDirectoryByPathAfterLoad,
                 weak_ptr_factory_.GetWeakPtr(),
                 directory_path,
                 callback));
}

void FileSystem::ReadDirectoryByPathAfterLoad(
    const base::FilePath& directory_path,
    const ReadDirectoryWithSettingCallback& callback,
    FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (error != FILE_ERROR_OK) {
    callback.Run(error,
                 hide_hosted_docs_,
                 scoped_ptr<ResourceEntryVector>());
    return;
  }

  resource_metadata_->ReadDirectoryByPath(
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

void FileSystem::GetResolvedFileByPath(
    scoped_ptr<GetResolvedFileParams> params) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(params);

  if (params->entry->file_info().is_directory()) {
    params->OnError(FILE_ERROR_NOT_A_FILE);
    return;
  }

  // The file's entry should have its file specific info.
  DCHECK(params->entry->has_file_specific_info());

  // For a hosted document, we create a special JSON file to represent the
  // document instead of fetching the document content in one of the exported
  // formats. The JSON file contains the edit URL and resource ID of the
  // document.
  if (params->entry->file_specific_info().is_hosted_document()) {
    base::FilePath* temp_file_path = new base::FilePath;
    ResourceEntry* entry_ptr = params->entry.get();
    base::PostTaskAndReplyWithResult(
        blocking_task_runner_,
        FROM_HERE,
        base::Bind(&CreateDocumentJsonFileOnBlockingPool,
                   cache_->GetCacheDirectoryPath(
                       FileCache::CACHE_TYPE_TMP_DOCUMENTS),
                   GURL(entry_ptr->file_specific_info().alternate_url()),
                   entry_ptr->resource_id(),
                   temp_file_path),
        base::Bind(
            &FileSystem::GetResolvedFileByPathAfterCreateDocumentJsonFile,
            weak_ptr_factory_.GetWeakPtr(),
            base::Passed(&params),
            base::Owned(temp_file_path)));
    return;
  }

  // Returns absolute path of the file if it were cached or to be cached.
  ResourceEntry* entry_ptr = params->entry.get();
  cache_->GetFile(
      entry_ptr->resource_id(),
      entry_ptr->file_specific_info().file_md5(),
      base::Bind(
          &FileSystem::GetResolvedFileByPathAfterGetFileFromCache,
          weak_ptr_factory_.GetWeakPtr(),
          base::Passed(&params)));
}

void FileSystem::GetResolvedFileByPathAfterCreateDocumentJsonFile(
    scoped_ptr<GetResolvedFileParams> params,
    const base::FilePath* file_path,
    FileError error) {
  DCHECK(params);
  DCHECK(file_path);

  if (error != FILE_ERROR_OK) {
    params->OnError(error);
    return;
  }

  params->OnCacheFileFound(*file_path);
  params->OnComplete(*file_path);
}

void FileSystem::GetResolvedFileByPathAfterGetFileFromCache(
    scoped_ptr<GetResolvedFileParams> params,
    FileError error,
    const base::FilePath& cache_file_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(params);

  // Have we found the file in cache? If so, return it back to the caller.
  if (error == FILE_ERROR_OK) {
    params->OnCacheFileFound(cache_file_path);
    params->OnComplete(cache_file_path);
    return;
  }

  // If cache file is not found, try to download the file from the server
  // instead. This logic is rather complicated but here's how this works:
  //
  // Retrieve fresh file metadata from server. We will extract file size and
  // content url from there (we want to make sure used content url is not
  // stale).
  //
  // Check if we have enough space, based on the expected file size.
  // - if we don't have enough space, try to free up the disk space
  // - if we still don't have enough space, return "no space" error
  // - if we have enough space, start downloading the file from the server
  GetResolvedFileParams* params_ptr = params.get();
  scheduler_->GetResourceEntry(
      params_ptr->entry->resource_id(),
      params_ptr->context,
      base::Bind(&FileSystem::GetResolvedFileByPathAfterGetResourceEntry,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Passed(&params)));
}

void FileSystem::GetResolvedFileByPathAfterGetResourceEntry(
    scoped_ptr<GetResolvedFileParams> params,
    google_apis::GDataErrorCode status,
    scoped_ptr<google_apis::ResourceEntry> entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(params);

  const FileError error = util::GDataToFileError(status);
  if (error != FILE_ERROR_OK) {
    params->OnError(error);
    return;
  }

  // The download URL is:
  // 1) src attribute of content element, on GData WAPI.
  // 2) the value of the key 'downloadUrl', on Drive API v2.
  // In both cases, we can use ResourceEntry::download_url().
  const GURL& download_url = entry->download_url();

  // The content URL can be empty for non-downloadable files (such as files
  // shared from others with "prevent downloading by viewers" flag set.)
  if (download_url.is_empty()) {
    params->OnError(FILE_ERROR_ACCESS_DENIED);
    return;
  }

  DCHECK_EQ(params->entry->resource_id(), entry->resource_id());
  resource_metadata_->RefreshEntry(
      ConvertToResourceEntry(*entry),
      base::Bind(&FileSystem::GetResolvedFileByPathAfterRefreshEntry,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Passed(&params),
                 download_url));
}

void FileSystem::GetResolvedFileByPathAfterRefreshEntry(
    scoped_ptr<GetResolvedFileParams> params,
    const GURL& download_url,
    FileError error,
    const base::FilePath& drive_file_path,
    scoped_ptr<ResourceEntry> entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(params);

  if (error != FILE_ERROR_OK) {
    params->OnError(error);
    return;
  }

  int64 file_size = entry->file_info().size();
  params->entry = entry.Pass();  // Update the entry in |params|.
  cache_->FreeDiskSpaceIfNeededFor(
      file_size,
      base::Bind(&FileSystem::GetResolvedFileByPathAfterFreeDiskSpace,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Passed(&params),
                 download_url));
}

void FileSystem::GetResolvedFileByPathAfterFreeDiskSpace(
    scoped_ptr<GetResolvedFileParams> params,
    const GURL& download_url,
    bool has_enough_space) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(params);

  if (!has_enough_space) {
    // If no enough space, return FILE_ERROR_NO_SPACE.
    params->OnError(FILE_ERROR_NO_SPACE);
    return;
  }

  // We have enough disk space. Create download destination file.
  const base::FilePath temp_download_directory =
      cache_->GetCacheDirectoryPath(FileCache::CACHE_TYPE_TMP_DOWNLOADS);
  base::FilePath* file_path = new base::FilePath;
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_,
      FROM_HERE,
      base::Bind(&CreateTemporaryReadableFileInDir,
                 temp_download_directory,
                 file_path),
      base::Bind(&FileSystem::GetResolveFileByPathAfterCreateTemporaryFile,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Passed(&params),
                 download_url,
                 base::Owned(file_path)));
}

void FileSystem::GetResolveFileByPathAfterCreateTemporaryFile(
    scoped_ptr<GetResolvedFileParams> params,
    const GURL& download_url,
    base::FilePath* temp_file,
    bool success) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(params);

  if (!success) {
    params->OnError(FILE_ERROR_FAILED);
    return;
  }

  GetResolvedFileParams* params_ptr = params.get();
  JobID id = scheduler_->DownloadFile(
      params_ptr->drive_file_path,
      *temp_file,
      download_url,
      params_ptr->context,
      base::Bind(&FileSystem::GetResolvedFileByPathAfterDownloadFile,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Passed(&params)),
      params_ptr->get_content_callback);
  params_ptr->OnStartDownloading(
      base::Bind(&FileSystem::CancelJobInScheduler,
                 weak_ptr_factory_.GetWeakPtr(),
                 id));
}

void FileSystem::GetResolvedFileByPathAfterDownloadFile(
    scoped_ptr<GetResolvedFileParams> params,
    google_apis::GDataErrorCode status,
    const base::FilePath& downloaded_file_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(params);

  // If user cancels download of a pinned-but-not-fetched file, mark file as
  // unpinned so that we do not sync the file again.
  if (status == google_apis::GDATA_CANCELLED) {
    cache_->GetCacheEntry(
        params->entry->resource_id(),
        params->entry->file_specific_info().file_md5(),
        base::Bind(
            &FileSystem::GetResolvedFileByPathAfterGetCacheEntryForCancel,
            weak_ptr_factory_.GetWeakPtr(),
            params->entry->resource_id(),
            params->entry->file_specific_info().file_md5()));
  }

  FileError error = util::GDataToFileError(status);
  if (error != FILE_ERROR_OK) {
    params->OnError(error);
    return;
  }

  ResourceEntry* entry = params->entry.get();
  cache_->Store(entry->resource_id(),
                entry->file_specific_info().file_md5(),
                downloaded_file_path,
                FileCache::FILE_OPERATION_MOVE,
                base::Bind(&FileSystem::GetResolvedFileByPathAfterStore,
                           weak_ptr_factory_.GetWeakPtr(),
                           base::Passed(&params),
                           downloaded_file_path));
}

void FileSystem::GetResolvedFileByPathAfterGetCacheEntryForCancel(
    const std::string& resource_id,
    const std::string& md5,
    bool success,
    const FileCacheEntry& cache_entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // TODO(hshi): http://crbug.com/127138 notify when file properties change.
  // This allows file manager to clear the "Available offline" checkbox.
  if (success && cache_entry.is_pinned()) {
    cache_->Unpin(resource_id,
                  md5,
                  base::Bind(&util::EmptyFileOperationCallback));
  }
}

void FileSystem::GetResolvedFileByPathAfterStore(
    scoped_ptr<GetResolvedFileParams> params,
    const base::FilePath& downloaded_file_path,
    FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(params);

  if (error != FILE_ERROR_OK) {
    blocking_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(base::IgnoreResult(&file_util::Delete),
                   downloaded_file_path,
                   false /* recursive*/));
    params->OnError(error);
    return;
  }
  // Storing to cache changes the "offline available" status, hence notify.
  OnDirectoryChanged(params->drive_file_path.DirName());

  ResourceEntry* entry = params->entry.get();
  cache_->GetFile(
      entry->resource_id(),
      entry->file_specific_info().file_md5(),
      base::Bind(&FileSystem::GetResolvedFileByPathAfterGetFile,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Passed(&params)));
}

void FileSystem::GetResolvedFileByPathAfterGetFile(
    scoped_ptr<GetResolvedFileParams> params,
    FileError error,
    const base::FilePath& cache_file) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(params);

  if (error != FILE_ERROR_OK) {
    params->OnError(error);
    return;
  }
  params->OnComplete(cache_file);
}

void FileSystem::RefreshDirectory(
    const base::FilePath& directory_path,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  // Make sure the destination directory exists.
  resource_metadata_->GetEntryInfoByPath(
      directory_path,
      base::Bind(&FileSystem::RefreshDirectoryAfterGetEntryInfo,
                 weak_ptr_factory_.GetWeakPtr(),
                 directory_path,
                 callback));
}

void FileSystem::RefreshDirectoryAfterGetEntryInfo(
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
    const DriveClientContext& context,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  drive_operations_.UpdateFileByResourceId(resource_id, context, callback);
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

void FileSystem::OnSearch(const SearchCallback& search_callback,
                          ScopedVector<ChangeList> change_lists,
                          FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!search_callback.is_null());

  if (error != FILE_ERROR_OK) {
    search_callback.Run(error,
                        GURL(),
                        scoped_ptr<std::vector<SearchResultInfo> >());
    return;
  }

  // The search results will be returned using virtual directory.
  // The directory is not really part of the file system, so it has no parent or
  // root.
  std::vector<SearchResultInfo>* results(new std::vector<SearchResultInfo>());
  scoped_ptr<std::vector<SearchResultInfo> > result_vec(results);

  DCHECK_EQ(1u, change_lists.size());
  const ChangeList* change_list = change_lists[0];

  // TODO(tbarzic): Limit total number of returned results for the query.
  const GURL& next_feed = change_list->next_url();

  const base::Closure callback = base::Bind(
      search_callback, FILE_ERROR_OK, next_feed, base::Passed(&result_vec));

  const std::vector<ResourceEntry>& entries = change_list->entries();
  if (entries.empty()) {
    callback.Run();
    return;
  }

  DVLOG(1) << "OnSearch number of entries=" << entries.size();
  // Go through all entries generated by the feed and add them to the search
  // result directory.
  for (size_t i = 0; i < entries.size(); ++i) {
    // Run the callback if this is the last iteration of the loop.
    const bool should_run_callback = (i + 1 == entries.size());
    const ResourceEntry& entry = entries[i];

    const GetEntryInfoWithFilePathCallback entry_info_callback =
        base::Bind(&FileSystem::AddToSearchResults,
                   weak_ptr_factory_.GetWeakPtr(),
                   results,
                   should_run_callback,
                   callback);

    resource_metadata_->RefreshEntry(entry, entry_info_callback);
  }
}

void FileSystem::AddToSearchResults(
    std::vector<SearchResultInfo>* results,
    bool should_run_callback,
    const base::Closure& callback,
    FileError error,
    const base::FilePath& drive_file_path,
    scoped_ptr<ResourceEntry> entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // If a result is not present in our local file system snapshot, call
  // CheckForUpdates to refresh the snapshot with a delta feed. This may happen
  // if the entry has recently been added to the drive (and we still haven't
  // received its delta feed).
  if (error == FILE_ERROR_OK) {
    DCHECK(entry.get());
    results->push_back(SearchResultInfo(drive_file_path, *entry.get()));
    DVLOG(1) << "AddToSearchResults " << drive_file_path.value();
  } else if (error == FILE_ERROR_NOT_FOUND) {
    CheckForUpdates();
  } else {
    NOTREACHED();
  }

  if (should_run_callback)
    callback.Run();
}

void FileSystem::Search(const std::string& search_query,
                        const GURL& next_feed,
                        const SearchCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  change_list_loader_->SearchFromServer(
      search_query,
      next_feed,
      base::Bind(&FileSystem::OnSearch,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback));
}

void FileSystem::SearchMetadata(const std::string& query,
                                int options,
                                int at_most_num_matches,
                                const SearchMetadataCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (hide_hosted_docs_)
    options |= SEARCH_METADATA_EXCLUDE_HOSTED_DOCUMENTS;

  drive::SearchMetadata(resource_metadata_,
                        query,
                        options,
                        at_most_num_matches,
                        callback);
}

void FileSystem::OnDirectoryChangedByOperation(
    const base::FilePath& directory_path) {
  OnDirectoryChanged(directory_path);
}

void FileSystem::OnDirectoryChanged(const base::FilePath& directory_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  FOR_EACH_OBSERVER(FileSystemObserver, observers_,
                    OnDirectoryChanged(directory_path));
}

void FileSystem::OnFeedFromServerLoaded() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  FOR_EACH_OBSERVER(FileSystemObserver, observers_,
                    OnFeedFromServerLoaded());
}

void FileSystem::OnInitialFeedLoaded() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  FOR_EACH_OBSERVER(FileSystemObserver,
                    observers_,
                    OnInitialLoadFinished());
}

void FileSystem::AddUploadedFile(
    scoped_ptr<google_apis::ResourceEntry> entry,
    const base::FilePath& file_content_path,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(entry.get());
  DCHECK(!entry->resource_id().empty());
  DCHECK(!entry->file_md5().empty());
  DCHECK(!callback.is_null());

  AddUploadedFileParams params(file_content_path,
                               callback,
                               entry->resource_id(),
                               entry->file_md5());

  resource_metadata_->AddEntry(
      ConvertToResourceEntry(*entry),
      base::Bind(&FileSystem::AddUploadedFileToCache,
                 weak_ptr_factory_.GetWeakPtr(), params));
}

void FileSystem::AddUploadedFileToCache(
    const AddUploadedFileParams& params,
    FileError error,
    const base::FilePath& file_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!params.resource_id.empty());
  DCHECK(!params.md5.empty());
  DCHECK(!params.callback.is_null());

  // Depending on timing, a metadata may have inserted via delta feed already.
  // So, FILE_ERROR_EXISTS is not an error.
  if (error != FILE_ERROR_OK && error != FILE_ERROR_EXISTS) {
    params.callback.Run(error);
    return;
  }

  OnDirectoryChanged(file_path.DirName());

  // At this point, upload to the server is fully succeeded. Failure to store to
  // cache is not a fatal error, so we wrap the callback with IgnoreError, and
  // always return success to the caller.
  cache_->Store(params.resource_id,
                params.md5,
                params.file_content_path,
                FileCache::FILE_OPERATION_COPY,
                base::Bind(&IgnoreError, params.callback));
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

  resource_metadata_->GetLargestChangestamp(
      base::Bind(&OnGetLargestChangestamp, metadata, callback));
}

void FileSystem::MarkCacheFileAsMounted(
    const base::FilePath& drive_file_path,
    const OpenFileCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  GetEntryInfoByPath(
      drive_file_path,
      base::Bind(&FileSystem::MarkCacheFileAsMountedAfterGetEntryInfo,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

void FileSystem::MarkCacheFileAsMountedAfterGetEntryInfo(
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
  cache_->MarkAsMounted(entry->resource_id(),
                        entry->file_specific_info().file_md5(),
                        callback);
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
  cache_->MarkAsUnmounted(cache_file_path, callback);
}

void FileSystem::GetCacheEntryByResourceId(
    const std::string& resource_id,
    const std::string& md5,
    const GetCacheEntryCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!resource_id.empty());
  DCHECK(!callback.is_null());

  cache_->GetCacheEntry(resource_id, md5, callback);
}

void FileSystem::IterateCache(
    const CacheIterateCallback& iteration_callback,
    const base::Closure& completion_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!iteration_callback.is_null());
  DCHECK(!completion_callback.is_null());

  cache_->Iterate(iteration_callback, completion_callback);
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

  resource_metadata_->GetEntryInfoByPath(
      file_path,
      base::Bind(&FileSystem::OnGetEntryInfoCompleteForOpenFile,
                 weak_ptr_factory_.GetWeakPtr(),
                 file_path,
                 base::Bind(&FileSystem::OnOpenFileFinished,
                            weak_ptr_factory_.GetWeakPtr(),
                            file_path,
                            callback)));
}

void FileSystem::OnGetEntryInfoCompleteForOpenFile(
    const base::FilePath& file_path,
    const OpenFileCallback& callback,
    FileError error,
    scoped_ptr<ResourceEntry> entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  DCHECK(entry.get() || error != FILE_ERROR_OK);

  if (entry.get() && !entry->has_file_specific_info())
    error = FILE_ERROR_NOT_FOUND;

  if (error == FILE_ERROR_OK) {
    if (entry->file_specific_info().file_md5().empty() ||
        entry->file_specific_info().is_hosted_document()) {
      // No support for opening a directory or hosted document.
      error = FILE_ERROR_INVALID_OPERATION;
    }
  }

  if (error != FILE_ERROR_OK) {
    callback.Run(error, base::FilePath());
    return;
  }

  DCHECK(!entry->resource_id().empty());
  // Extract a pointer before we call Pass() so we can use it below.
  ResourceEntry* entry_ptr = entry.get();
  GetResolvedFileByPath(
      make_scoped_ptr(new GetResolvedFileParams(
          file_path,
          DriveClientContext(USER_INITIATED),
          entry.Pass(),
          GetFileContentInitializedCallback(),
          base::Bind(&FileSystem::OnGetFileCompleteForOpenFile,
                     weak_ptr_factory_.GetWeakPtr(),
                     GetFileCompleteForOpenParams(
                         callback,
                         entry_ptr->resource_id(),
                         entry_ptr->file_specific_info().file_md5())),
          google_apis::GetContentCallback())));
}

void FileSystem::OnGetFileCompleteForOpenFile(
    const GetFileCompleteForOpenParams& params,
    FileError error,
    const base::FilePath& file_path,
    const std::string& mime_type,
    DriveFileType file_type) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!params.callback.is_null());

  if (error != FILE_ERROR_OK) {
    params.callback.Run(error, base::FilePath());
    return;
  }

  // OpenFile ensures that the file is a regular file.
  DCHECK_EQ(REGULAR_FILE, file_type);

  cache_->MarkDirty(
      params.resource_id,
      params.md5,
      base::Bind(&FileSystem::OnMarkDirtyInCacheCompleteForOpenFile,
                 weak_ptr_factory_.GetWeakPtr(),
                 params));
}

void FileSystem::OnMarkDirtyInCacheCompleteForOpenFile(
    const GetFileCompleteForOpenParams& params,
    FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!params.callback.is_null());

  if (error != FILE_ERROR_OK) {
    params.callback.Run(error, base::FilePath());
    return;
  }

  cache_->GetFile(params.resource_id, params.md5, params.callback);
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
  resource_metadata_->GetEntryInfoByPath(
      file_path,
      base::Bind(&FileSystem::CloseFileAfterGetEntryInfo,
                 weak_ptr_factory_.GetWeakPtr(),
                 file_path,
                 base::Bind(&FileSystem::CloseFileFinalize,
                            weak_ptr_factory_.GetWeakPtr(),
                            file_path,
                            callback)));
}

void FileSystem::CloseFileAfterGetEntryInfo(
    const base::FilePath& file_path,
    const FileOperationCallback& callback,
    FileError error,
    scoped_ptr<ResourceEntry> entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (entry.get() && !entry->has_file_specific_info())
    error = FILE_ERROR_NOT_FOUND;

  if (error != FILE_ERROR_OK) {
    callback.Run(error);
    return;
  }

  // Step 2 of CloseFile: Commit the modification in cache. This will trigger
  // background upload.
  // TODO(benchan,kinaba): Call ClearDirtyInCache instead of CommitDirtyInCache
  // if the file has not been modified. Come up with a way to detect the
  // intactness effectively, or provide a method for user to declare it when
  // calling CloseFile().
  cache_->CommitDirty(entry->resource_id(),
                      entry->file_specific_info().file_md5(),
                      callback);
}

void FileSystem::CloseFileFinalize(const base::FilePath& file_path,
                                   const FileOperationCallback& callback,
                                   FileError result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  // Step 3 of CloseFile.
  // All the invocation of |callback| from operations initiated from CloseFile
  // must go through here. Removes the |file_path| from the remembered set so
  // that subsequent operations can open the file again.
  open_files_.erase(file_path);

  // Then invokes the user-supplied callback function.
  callback.Run(result);
}

void FileSystem::CheckLocalModificationAndRun(
    scoped_ptr<ResourceEntry> entry,
    const GetEntryInfoCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(entry.get());
  DCHECK(!callback.is_null());

  // For entries that will never be cached, use the original entry info as is.
  if (!entry->has_file_specific_info() ||
      entry->file_specific_info().is_hosted_document()) {
    callback.Run(FILE_ERROR_OK, entry.Pass());
    return;
  }

  // Checks if the file is cached and modified locally.
  const std::string resource_id = entry->resource_id();
  const std::string md5 = entry->file_specific_info().file_md5();
  cache_->GetCacheEntry(
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
    const GetEntryInfoCallback& callback,
    bool success,
    const FileCacheEntry& cache_entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  // When no dirty cache is found, use the original entry info as is.
  if (!success || !cache_entry.is_dirty()) {
    callback.Run(FILE_ERROR_OK, entry.Pass());
    return;
  }

  // Gets the cache file path.
  const std::string& resource_id = entry->resource_id();
  const std::string& md5 = entry->file_specific_info().file_md5();
  cache_->GetFile(
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
    const GetEntryInfoCallback& callback,
    FileError error,
    const base::FilePath& local_cache_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  // When no dirty cache is found, use the original entry info as is.
  if (error != FILE_ERROR_OK) {
    callback.Run(FILE_ERROR_OK, entry.Pass());
    return;
  }

  // If the cache is dirty, obtain the file info from the cache file itself.
  base::PlatformFileInfo* file_info = new base::PlatformFileInfo;
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_,
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
    const GetEntryInfoCallback& callback,
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

void FileSystem::CancelJobInScheduler(JobID id) {
  scheduler_->CancelJob(id);
}

}  // namespace drive
