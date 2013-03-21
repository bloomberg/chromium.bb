// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/drive_file_system.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/message_loop_proxy.h"
#include "base/metrics/histogram.h"
#include "base/platform_file.h"
#include "base/prefs/pref_change_registrar.h"
#include "base/prefs/pref_service.h"
#include "base/stringprintf.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/values.h"
#include "chrome/browser/chromeos/drive/change_list_loader.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/drive_cache.h"
#include "chrome/browser/chromeos/drive/drive_file_system_observer.h"
#include "chrome/browser/chromeos/drive/drive_file_system_util.h"
#include "chrome/browser/chromeos/drive/drive_scheduler.h"
#include "chrome/browser/chromeos/drive/resource_entry_conversion.h"
#include "chrome/browser/google_apis/drive_api_parser.h"
#include "chrome/browser/google_apis/drive_api_util.h"
#include "chrome/browser/google_apis/drive_service_interface.h"
#include "chrome/browser/google_apis/drive_uploader.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"

using content::BrowserThread;

namespace drive {
namespace {

const char kMimeTypeJson[] = "application/json";
const char kEmptyFilePath[] = "/dev/null";

// Drive update polling interval for polling only mode (in seconds).
const int kFastPollingIntervalInSec = 60;

// Drive update polling interval when update notification is available (in
// seconds). Ideally we don't need this, but we do polling in case update
// notification doesn't work. http://crbug.com/157080
const int kSlowPollingIntervalInSec = 300;

//================================ Helper functions ============================

// Runs GetFileCallback with pointers dereferenced.
// Used for PostTaskAndReply().
void RunGetFileCallbackHelper(const GetFileCallback& callback,
                              base::FilePath* file_path,
                              const std::string& mime_type,
                              DriveFileType file_type,
                              DriveFileError error) {
  DCHECK(!callback.is_null());
  DCHECK(file_path);
  callback.Run(error, *file_path, mime_type, file_type);
}

// The class to wait for the drive service to be ready to start operation.
class OperationReadinessObserver : public google_apis::DriveServiceObserver {
 public:
  OperationReadinessObserver(google_apis::DriveServiceInterface* drive_service,
                             const base::Closure& callback)
      : drive_service_(drive_service),
        callback_(callback) {
    DCHECK(!callback_.is_null());
    drive_service_->AddObserver(this);
  }

  // DriveServiceObserver override.
  virtual void OnReadyToPerformOperations() OVERRIDE {
    base::MessageLoopProxy::current()->PostTask(FROM_HERE, callback_);
    drive_service_->RemoveObserver(this);
    base::MessageLoopProxy::current()->DeleteSoon(FROM_HERE, this);
  }

 private:
  google_apis::DriveServiceInterface* drive_service_;
  base::Closure callback_;

  DISALLOW_COPY_AND_ASSIGN(OperationReadinessObserver);
};

// Creates a temporary JSON file representing a document with |edit_url|
// and |resource_id| under |document_dir| on blocking pool.
DriveFileError CreateDocumentJsonFileOnBlockingPool(
    const base::FilePath& document_dir,
    const GURL& edit_url,
    const std::string& resource_id,
    base::FilePath* temp_file_path) {
  DCHECK(temp_file_path);

  DriveFileError error = DRIVE_FILE_ERROR_FAILED;

  if (file_util::CreateTemporaryFileInDir(document_dir, temp_file_path)) {
    std::string document_content = base::StringPrintf(
        "{\"url\": \"%s\", \"resource_id\": \"%s\"}",
        edit_url.spec().c_str(), resource_id.c_str());
    int document_size = static_cast<int>(document_content.size());
    if (file_util::WriteFile(*temp_file_path, document_content.data(),
                             document_size) == document_size) {
      error = DRIVE_FILE_OK;
    }
  }

  if (error != DRIVE_FILE_OK)
    temp_file_path->clear();
  return error;
}

// Helper function for binding |path| to GetEntryInfoWithFilePathCallback and
// create GetEntryInfoCallback.
void RunGetEntryInfoWithFilePathCallback(
    const GetEntryInfoWithFilePathCallback& callback,
    const base::FilePath& path,
    DriveFileError error,
    scoped_ptr<DriveEntryProto> entry_proto) {
  DCHECK(!callback.is_null());
  callback.Run(error, path, entry_proto.Pass());
}

// Callback for DriveResourceMetadata::GetLargestChangestamp.
// |callback| must not be null.
void OnGetLargestChangestamp(
    DriveFileSystemMetadata metadata,  // Will be modified.
    const GetFilesystemMetadataCallback& callback,
    int64 largest_changestamp) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  metadata.largest_changestamp = largest_changestamp;
  callback.Run(metadata);
}

}  // namespace

// DriveFileSystem::GetFileCompleteForOpenParams struct implementation.
struct DriveFileSystem::GetFileCompleteForOpenParams {
  GetFileCompleteForOpenParams(const OpenFileCallback& callback,
                               const std::string& resource_id,
                               const std::string& md5);
  OpenFileCallback callback;
  std::string resource_id;
  std::string md5;
};

DriveFileSystem::GetFileCompleteForOpenParams::GetFileCompleteForOpenParams(
    const OpenFileCallback& callback,
    const std::string& resource_id,
    const std::string& md5)
    : callback(callback),
      resource_id(resource_id),
      md5(md5) {
}

// DriveFileSystem::GetFileFromCacheParams struct implementation.
struct DriveFileSystem::GetFileFromCacheParams {
  GetFileFromCacheParams(
      const base::FilePath& virtual_file_path,
      const base::FilePath& local_tmp_path,
      const std::string& resource_id,
      const std::string& md5,
      const std::string& mime_type,
      const DriveClientContext& context,
      const GetFileCallback& get_file_callback,
      const google_apis::GetContentCallback& get_content_callback)
      : virtual_file_path(virtual_file_path),
        local_tmp_path(local_tmp_path),
        resource_id(resource_id),
        md5(md5),
        mime_type(mime_type),
        context(context),
        get_file_callback(get_file_callback),
        get_content_callback(get_content_callback) {
  }

  base::FilePath virtual_file_path;
  base::FilePath local_tmp_path;
  base::FilePath cache_file_path;
  std::string resource_id;
  std::string md5;
  std::string mime_type;
  DriveClientContext context;
  GetFileCallback get_file_callback;
  google_apis::GetContentCallback get_content_callback;
};

// DriveFileSystem::AddUploadedFileParams implementation.
struct DriveFileSystem::AddUploadedFileParams {
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


// DriveFileSystem class implementation.

DriveFileSystem::DriveFileSystem(
    Profile* profile,
    DriveCache* cache,
    google_apis::DriveServiceInterface* drive_service,
    google_apis::DriveUploaderInterface* uploader,
    DriveWebAppsRegistry* webapps_registry,
    DriveResourceMetadata* resource_metadata,
    base::SequencedTaskRunner* blocking_task_runner)
    : profile_(profile),
      cache_(cache),
      uploader_(uploader),
      drive_service_(drive_service),
      webapps_registry_(webapps_registry),
      resource_metadata_(resource_metadata),
      update_timer_(true /* retain_user_task */, true /* is_repeating */),
      last_update_check_error_(DRIVE_FILE_OK),
      hide_hosted_docs_(false),
      blocking_task_runner_(blocking_task_runner),
      scheduler_(new DriveScheduler(profile, drive_service, uploader)),
      polling_interval_sec_(kFastPollingIntervalInSec),
      push_notification_enabled_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
  // Should be created from the file browser extension API on UI thread.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void DriveFileSystem::Reload() {
  resource_metadata_->Reset(base::Bind(&DriveFileSystem::ReloadAfterReset,
                                       weak_ptr_factory_.GetWeakPtr()));
}

void DriveFileSystem::Initialize() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  SetupChangeListLoader();

  // Allocate the drive operation handlers.
  drive_operations_.Init(scheduler_.get(),
                         this,  // DriveFileSystemInterface
                         cache_,
                         resource_metadata_,
                         uploader_,
                         blocking_task_runner_,
                         this);  // OperationObserver

  PrefService* pref_service = profile_->GetPrefs();
  hide_hosted_docs_ = pref_service->GetBoolean(prefs::kDisableDriveHostedFiles);

  scheduler_->Initialize();

  InitializePreferenceObserver();
}

void DriveFileSystem::ReloadAfterReset() {
  SetupChangeListLoader();

  change_list_loader_->LoadFromServerIfNeeded(
      DirectoryFetchInfo(),
      base::Bind(&DriveFileSystem::OnUpdateChecked,
                 weak_ptr_factory_.GetWeakPtr()));
}

void DriveFileSystem::SetupChangeListLoader() {
  change_list_loader_.reset(new ChangeListLoader(resource_metadata_,
                                                 scheduler_.get(),
                                                 webapps_registry_));
  change_list_loader_->AddObserver(this);
}

void DriveFileSystem::CheckForUpdates() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DVLOG(1) << "CheckForUpdates";

  if (change_list_loader_->loaded() && !change_list_loader_->refreshing()) {
    change_list_loader_->LoadFromServerIfNeeded(
        DirectoryFetchInfo(),
        base::Bind(&DriveFileSystem::OnUpdateChecked,
                   weak_ptr_factory_.GetWeakPtr()));
  }
}

void DriveFileSystem::OnUpdateChecked(DriveFileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DVLOG(1) << "CheckForUpdates finished: " << error;
  last_update_check_time_ = base::Time::Now();
  last_update_check_error_ = error;
}

DriveFileSystem::~DriveFileSystem() {
  // This should be called from UI thread, from DriveSystemService shutdown.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  change_list_loader_->RemoveObserver(this);

  // Cancel all the in-flight operations.
  // This asynchronously cancels the URL fetch operations.
  drive_service_->CancelAll();
}

void DriveFileSystem::AddObserver(DriveFileSystemObserver* observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  observers_.AddObserver(observer);
}

void DriveFileSystem::RemoveObserver(DriveFileSystemObserver* observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  observers_.RemoveObserver(observer);
}

void DriveFileSystem::StartInitialFeedFetch() {
  if (drive_service_->CanStartOperation()) {
    LoadIfNeeded(DirectoryFetchInfo(),
                 base::Bind(&util::EmptyFileOperationCallback));
  } else {
    // Wait for the service to get ready. The observer deletes itself after
    // OnReadyToPerformOperations() gets called.
    new OperationReadinessObserver(
        drive_service_,
        base::Bind(&DriveFileSystem::LoadIfNeeded,
                   weak_ptr_factory_.GetWeakPtr(),
                   DirectoryFetchInfo(),
                   base::Bind(&util::EmptyFileOperationCallback)));
  }
}

void DriveFileSystem::StartPolling() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DCHECK(!update_timer_.IsRunning());
  update_timer_.Start(FROM_HERE,
                      base::TimeDelta::FromSeconds(polling_interval_sec_),
                      base::Bind(&DriveFileSystem::CheckForUpdates,
                                 weak_ptr_factory_.GetWeakPtr()));
}

void DriveFileSystem::StopPolling() {
  // If unmount request comes from filesystem side, this method may be called
  // twice. First is just after unmounting on filesystem, second is after
  // unmounting on filemanager on JS. In other words, if this is called from
  // DriveSystemService::RemoveDriveMountPoint(), this will be called again from
  // FileBrowserEventRouter::HandleRemoteUpdateRequestOnUIThread().
  // We choose to stopping updates asynchronous without waiting for filemanager,
  // rather than waiting for completion of unmounting on filemanager.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (update_timer_.IsRunning())
    update_timer_.Stop();
}

void DriveFileSystem::SetPushNotificationEnabled(bool enabled) {
  push_notification_enabled_ = enabled;
  polling_interval_sec_ = enabled ? kSlowPollingIntervalInSec :
                          kFastPollingIntervalInSec;
}

void DriveFileSystem::GetEntryInfoByResourceId(
    const std::string& resource_id,
    const GetEntryInfoWithFilePathCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!resource_id.empty());
  DCHECK(!callback.is_null());

  resource_metadata_->GetEntryInfoByResourceId(
      resource_id,
      base::Bind(&DriveFileSystem::GetEntryInfoByResourceIdAfterGetEntry,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback));
}

void DriveFileSystem::GetEntryInfoByResourceIdAfterGetEntry(
    const GetEntryInfoWithFilePathCallback& callback,
    DriveFileError error,
    const base::FilePath& file_path,
    scoped_ptr<DriveEntryProto> entry_proto) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (error != DRIVE_FILE_OK) {
    callback.Run(error, base::FilePath(), scoped_ptr<DriveEntryProto>());
    return;
  }
  DCHECK(entry_proto.get());

  CheckLocalModificationAndRun(
      entry_proto.Pass(),
      base::Bind(&RunGetEntryInfoWithFilePathCallback,
                 callback,
                 file_path));
}

void DriveFileSystem::LoadIfNeeded(
    const DirectoryFetchInfo& directory_fetch_info,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  change_list_loader_->LoadIfNeeded(directory_fetch_info, callback);
}

void DriveFileSystem::TransferFileFromRemoteToLocal(
    const base::FilePath& remote_src_file_path,
    const base::FilePath& local_dest_file_path,
    const FileOperationCallback& callback) {

  drive_operations_.TransferFileFromRemoteToLocal(remote_src_file_path,
                                                  local_dest_file_path,
                                                  callback);
}

void DriveFileSystem::TransferFileFromLocalToRemote(
    const base::FilePath& local_src_file_path,
    const base::FilePath& remote_dest_file_path,
    const FileOperationCallback& callback) {

  drive_operations_.TransferFileFromLocalToRemote(local_src_file_path,
                                                  remote_dest_file_path,
                                                  callback);
}

void DriveFileSystem::Copy(const base::FilePath& src_file_path,
                           const base::FilePath& dest_file_path,
                           const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  drive_operations_.Copy(src_file_path, dest_file_path, callback);
}

void DriveFileSystem::Move(const base::FilePath& src_file_path,
                           const base::FilePath& dest_file_path,
                           const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  drive_operations_.Move(src_file_path, dest_file_path, callback);
}

void DriveFileSystem::Remove(const base::FilePath& file_path,
    bool is_recursive,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  drive_operations_.Remove(file_path, is_recursive, callback);
}

void DriveFileSystem::CreateDirectory(
    const base::FilePath& directory_path,
    bool is_exclusive,
    bool is_recursive,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  drive_operations_.CreateDirectory(
      directory_path, is_exclusive, is_recursive, callback);
}

void DriveFileSystem::CreateFile(const base::FilePath& file_path,
                                 bool is_exclusive,
                                 const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  // First, checks the existence of a file at |file_path|.
  resource_metadata_->GetEntryInfoByPath(
      file_path,
      base::Bind(&DriveFileSystem::OnGetEntryInfoForCreateFile,
                 weak_ptr_factory_.GetWeakPtr(),
                 file_path,
                 is_exclusive,
                 callback));
}

void DriveFileSystem::OnGetEntryInfoForCreateFile(
    const base::FilePath& file_path,
    bool is_exclusive,
    const FileOperationCallback& callback,
    DriveFileError result,
    scoped_ptr<DriveEntryProto> entry_proto) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  // The |file_path| is invalid. It is an error.
  if (result != DRIVE_FILE_ERROR_NOT_FOUND &&
      result != DRIVE_FILE_OK) {
    callback.Run(result);
    return;
  }

  // An entry already exists at |file_path|.
  if (result == DRIVE_FILE_OK) {
    DCHECK(entry_proto.get());
    // If an exclusive mode is requested, or the entry is not a regular file,
    // it is an error.
    if (is_exclusive ||
        entry_proto->file_info().is_directory() ||
        entry_proto->file_specific_info().is_hosted_document()) {
      callback.Run(DRIVE_FILE_ERROR_EXISTS);
      return;
    }

    // Otherwise nothing more to do. Succeeded.
    callback.Run(DRIVE_FILE_OK);
    return;
  }

  // No entry found at |file_path|. Let's create a brand new file.
  // For now, it is implemented by uploading an empty file (/dev/null).
  // TODO(kinaba): http://crbug.com/135143. Implement in a nicer way.
  drive_operations_.TransferRegularFile(base::FilePath(kEmptyFilePath),
                                        file_path,
                                        callback);
}

void DriveFileSystem::GetFileByPath(const base::FilePath& file_path,
                                    const GetFileCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  resource_metadata_->GetEntryInfoByPath(
      file_path,
      base::Bind(&DriveFileSystem::OnGetEntryInfoCompleteForGetFileByPath,
                 weak_ptr_factory_.GetWeakPtr(),
                 file_path,
                 callback));
}

void DriveFileSystem::OnGetEntryInfoCompleteForGetFileByPath(
    const base::FilePath& file_path,
    const GetFileCallback& callback,
    DriveFileError error,
    scoped_ptr<DriveEntryProto> entry_proto) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (error != DRIVE_FILE_OK) {
    callback.Run(error, base::FilePath(), std::string(), REGULAR_FILE);
    return;
  }
  DCHECK(entry_proto.get());

  GetResolvedFileByPath(file_path,
                        DriveClientContext(USER_INITIATED),
                        callback,
                        google_apis::GetContentCallback(),
                        entry_proto.Pass());
}

void DriveFileSystem::GetResolvedFileByPath(
    const base::FilePath& file_path,
    const DriveClientContext& context,
    const GetFileCallback& get_file_callback,
    const google_apis::GetContentCallback& get_content_callback,
    scoped_ptr<DriveEntryProto> entry_proto) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!get_file_callback.is_null());
  DCHECK(entry_proto.get());

  if (!entry_proto->has_file_specific_info()) {
    get_file_callback.Run(DRIVE_FILE_ERROR_NOT_FOUND,
                          base::FilePath(),
                          std::string(),
                          REGULAR_FILE);
    return;
  }

  // For a hosted document, we create a special JSON file to represent the
  // document instead of fetching the document content in one of the exported
  // formats. The JSON file contains the edit URL and resource ID of the
  // document.
  if (entry_proto->file_specific_info().is_hosted_document()) {
    base::FilePath* temp_file_path = new base::FilePath;
    const std::string mime_type = kMimeTypeJson;
    const DriveFileType file_type = HOSTED_DOCUMENT;
    base::PostTaskAndReplyWithResult(
        blocking_task_runner_,
        FROM_HERE,
        base::Bind(&CreateDocumentJsonFileOnBlockingPool,
                   cache_->GetCacheDirectoryPath(
                       DriveCache::CACHE_TYPE_TMP_DOCUMENTS),
                   GURL(entry_proto->file_specific_info().alternate_url()),
                   entry_proto->resource_id(),
                   temp_file_path),
        base::Bind(&RunGetFileCallbackHelper,
                   get_file_callback,
                   base::Owned(temp_file_path),
                   mime_type,
                   file_type));
    return;
  }

  // Returns absolute path of the file if it were cached or to be cached.
  base::FilePath local_tmp_path = cache_->GetCacheFilePath(
      entry_proto->resource_id(),
      entry_proto->file_specific_info().file_md5(),
      DriveCache::CACHE_TYPE_TMP,
      DriveCache::CACHED_FILE_FROM_SERVER);
  cache_->GetFile(entry_proto->resource_id(),
                  entry_proto->file_specific_info().file_md5(),
                  base::Bind(
                      &DriveFileSystem::OnGetFileFromCache,
                      weak_ptr_factory_.GetWeakPtr(),
                      GetFileFromCacheParams(
                          file_path,
                          local_tmp_path,
                          entry_proto->resource_id(),
                          entry_proto->file_specific_info().file_md5(),
                          entry_proto->file_specific_info().content_mime_type(),
                          context,
                          get_file_callback,
                          get_content_callback)));
}

void DriveFileSystem::GetFileByResourceId(
    const std::string& resource_id,
    const DriveClientContext& context,
    const GetFileCallback& get_file_callback,
    const google_apis::GetContentCallback& get_content_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!resource_id.empty());
  DCHECK(!get_file_callback.is_null());

  resource_metadata_->GetEntryInfoByResourceId(
      resource_id,
      base::Bind(&DriveFileSystem::GetFileByResourceIdAfterGetEntry,
                 weak_ptr_factory_.GetWeakPtr(),
                 context,
                 get_file_callback,
                 get_content_callback));
}

void DriveFileSystem::GetFileByResourceIdAfterGetEntry(
    const DriveClientContext& context,
    const GetFileCallback& get_file_callback,
    const google_apis::GetContentCallback& get_content_callback,
    DriveFileError error,
    const base::FilePath& file_path,
    scoped_ptr<DriveEntryProto> entry_proto) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!get_file_callback.is_null());

  if (error != DRIVE_FILE_OK) {
    get_file_callback.Run(DRIVE_FILE_ERROR_NOT_FOUND,
                          base::FilePath(),
                          std::string(),
                          REGULAR_FILE);
    return;
  }

  GetResolvedFileByPath(file_path,
                        context,
                        get_file_callback,
                        get_content_callback,
                        entry_proto.Pass());
}

void DriveFileSystem::CancelGetFile(const base::FilePath& drive_file_path) {
  // Currently the task is managed on DriveServiceInterface, so we directly
  // call its method here.
  // Note: the task management will be moved to DriveScheduler, an the we
  // can cancel the job via the |scheduler_|.
  drive_service_->CancelForFilePath(drive_file_path);
}

void DriveFileSystem::OnGetFileFromCache(
    const GetFileFromCacheParams& in_params,
    DriveFileError error,
    const base::FilePath& cache_file_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!in_params.get_file_callback.is_null());

  // Have we found the file in cache? If so, return it back to the caller.
  if (error == DRIVE_FILE_OK) {
    in_params.get_file_callback.Run(error,
                                    cache_file_path,
                                    in_params.mime_type,
                                    REGULAR_FILE);
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
  GetFileFromCacheParams params(in_params);
  params.cache_file_path = cache_file_path;
  scheduler_->GetResourceEntry(
      params.resource_id,
      params.context,
      base::Bind(&DriveFileSystem::OnGetResourceEntry,
                 weak_ptr_factory_.GetWeakPtr(),
                 params));
}

void DriveFileSystem::OnGetResourceEntry(
    const GetFileFromCacheParams& params,
    google_apis::GDataErrorCode status,
    scoped_ptr<google_apis::ResourceEntry> entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!params.get_file_callback.is_null());

  const DriveFileError error = util::GDataToDriveFileError(status);
  if (error != DRIVE_FILE_OK) {
    params.get_file_callback.Run(error,
                                 params.cache_file_path,
                                 params.mime_type,
                                 REGULAR_FILE);
    return;
  }

  // The download URL is:
  // 1) src attribute of content element, on GData WAPI.
  // 2) the value of the key 'downloadUrl', on Drive API v2.
  // In both cases, we can use ResourceEntry::download_url().
  GURL download_url = entry->download_url();
  int64 file_size = entry->file_size();

  // The content URL can be empty for non-downloadable files (such as files
  // shared from others with "prevent downloading by viewers" flag set.)
  if (download_url.is_empty()) {
    params.get_file_callback.Run(DRIVE_FILE_ERROR_ACCESS_DENIED,
                                 params.cache_file_path,
                                 params.mime_type,
                                 REGULAR_FILE);
    return;
  }

  DCHECK_EQ(params.resource_id, entry->resource_id());
  resource_metadata_->RefreshEntry(
      ConvertResourceEntryToDriveEntryProto(*entry),
      base::Bind(&DriveFileSystem::CheckForSpaceBeforeDownload,
                 weak_ptr_factory_.GetWeakPtr(),
                 params,
                 file_size,
                 download_url));
}

void DriveFileSystem::CheckForSpaceBeforeDownload(
    const GetFileFromCacheParams& params,
    int64 file_size,
    const GURL& download_url,
    DriveFileError error,
    const base::FilePath& /* drive_file_path */,
    scoped_ptr<DriveEntryProto> /* entry_proto */) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!params.get_file_callback.is_null());

  if (error != DRIVE_FILE_OK) {
    params.get_file_callback.Run(error,
                                 params.cache_file_path,
                                 params.mime_type,
                                 REGULAR_FILE);
    return;
  }

  cache_->FreeDiskSpaceIfNeededFor(
      file_size,
      base::Bind(&DriveFileSystem::StartDownloadFileIfEnoughSpace,
                 weak_ptr_factory_.GetWeakPtr(),
                 params,
                 download_url,
                 params.context,
                 params.cache_file_path));
}

void DriveFileSystem::StartDownloadFileIfEnoughSpace(
    const GetFileFromCacheParams& params,
    const GURL& download_url,
    const DriveClientContext& context,
    const base::FilePath& cache_file_path,
    bool has_enough_space) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!params.get_file_callback.is_null());

  if (!has_enough_space) {
    // If no enough space, return PLATFORM_FILE_ERROR_NO_SPACE.
    params.get_file_callback.Run(DRIVE_FILE_ERROR_NO_SPACE,
                                 cache_file_path,
                                 params.mime_type,
                                 REGULAR_FILE);
    return;
  }

  // We have enough disk space. Start downloading the file.
  scheduler_->DownloadFile(
      params.virtual_file_path,
      params.local_tmp_path,
      download_url,
      context,
      base::Bind(&DriveFileSystem::OnFileDownloaded,
                 weak_ptr_factory_.GetWeakPtr(),
                 params),
      params.get_content_callback);
}

void DriveFileSystem::GetEntryInfoByPath(const base::FilePath& file_path,
                                         const GetEntryInfoCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  // DriveResourceMetadata may know about the entry even if the resource
  // metadata is not yet fully loaded. For instance, DriveResourceMetadata()
  // always knows about the root directory. For "fast fetch"
  // (crbug.com/178348) to work, it's needd to delay the resource metadata
  // loading until the first call to ReadDirectoryByPath().
  resource_metadata_->GetEntryInfoByPath(
      file_path,
      base::Bind(&DriveFileSystem::GetEntryInfoByPathAfterGetEntry1,
                 weak_ptr_factory_.GetWeakPtr(),
                 file_path,
                 callback));
}

void DriveFileSystem::GetEntryInfoByPathAfterGetEntry1(
    const base::FilePath& file_path,
    const GetEntryInfoCallback& callback,
    DriveFileError error,
    scoped_ptr<DriveEntryProto> entry_proto) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (error == DRIVE_FILE_OK) {
    CheckLocalModificationAndRun(entry_proto.Pass(), callback);
    return;
  }

  // Start loading if needed. Note that directory_fetch_info is empty here,
  // as we don't need to fetch the contents of a directory when we just need
  // to get an entry of the directory.
  LoadIfNeeded(DirectoryFetchInfo(),
               base::Bind(&DriveFileSystem::GetEntryInfoByPathAfterLoad,
                          weak_ptr_factory_.GetWeakPtr(),
                          file_path,
                          callback));
}

void DriveFileSystem::GetEntryInfoByPathAfterLoad(
    const base::FilePath& file_path,
    const GetEntryInfoCallback& callback,
    DriveFileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (error != DRIVE_FILE_OK) {
    callback.Run(error, scoped_ptr<DriveEntryProto>());
    return;
  }

  resource_metadata_->GetEntryInfoByPath(
      file_path,
      base::Bind(&DriveFileSystem::GetEntryInfoByPathAfterGetEntry2,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback));
}

void DriveFileSystem::GetEntryInfoByPathAfterGetEntry2(
    const GetEntryInfoCallback& callback,
    DriveFileError error,
    scoped_ptr<DriveEntryProto> entry_proto) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (error != DRIVE_FILE_OK) {
    callback.Run(error, scoped_ptr<DriveEntryProto>());
    return;
  }
  DCHECK(entry_proto.get());

  CheckLocalModificationAndRun(entry_proto.Pass(), callback);
}

void DriveFileSystem::ReadDirectoryByPath(
    const base::FilePath& directory_path,
    const ReadDirectoryWithSettingCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  // As described in GetEntryInfoByPath(), DriveResourceMetadata may know
  // about the entry even if the file system is not yet fully loaded, hece we
  // should just ask DriveResourceMetadata first.
  resource_metadata_->GetEntryInfoByPath(
      directory_path,
      base::Bind(&DriveFileSystem::ReadDirectoryByPathAfterGetEntry,
                 weak_ptr_factory_.GetWeakPtr(),
                 directory_path,
                 callback));
}

void DriveFileSystem::ReadDirectoryByPathAfterGetEntry(
    const base::FilePath& directory_path,
    const ReadDirectoryWithSettingCallback& callback,
    DriveFileError error,
    scoped_ptr<DriveEntryProto> entry_proto) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (error != DRIVE_FILE_OK) {
    // If we don't know about the directory, start loading.
    LoadIfNeeded(DirectoryFetchInfo(),
                 base::Bind(&DriveFileSystem::ReadDirectoryByPathAfterLoad,
                            weak_ptr_factory_.GetWeakPtr(),
                            directory_path,
                            callback));
    return;
  }

  if (!entry_proto->file_info().is_directory()) {
    callback.Run(DRIVE_FILE_ERROR_NOT_A_DIRECTORY,
                 hide_hosted_docs_,
                 scoped_ptr<DriveEntryProtoVector>());
    return;
  }

  // Pass the directory fetch info so we can fetch the contents of the
  // directory before loading change lists.
  DirectoryFetchInfo directory_fetch_info(
      entry_proto->resource_id(),
      entry_proto->directory_specific_info().changestamp());
  LoadIfNeeded(directory_fetch_info,
               base::Bind(&DriveFileSystem::ReadDirectoryByPathAfterLoad,
                          weak_ptr_factory_.GetWeakPtr(),
                          directory_path,
                          callback));
}

void DriveFileSystem::ReadDirectoryByPathAfterLoad(
    const base::FilePath& directory_path,
    const ReadDirectoryWithSettingCallback& callback,
    DriveFileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (error != DRIVE_FILE_OK) {
    callback.Run(error,
                 hide_hosted_docs_,
                 scoped_ptr<DriveEntryProtoVector>());
    return;
  }

  resource_metadata_->ReadDirectoryByPath(
      directory_path,
      base::Bind(&DriveFileSystem::ReadDirectoryByPathAfterRead,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback));
}

void DriveFileSystem::ReadDirectoryByPathAfterRead(
    const ReadDirectoryWithSettingCallback& callback,
    DriveFileError error,
    scoped_ptr<DriveEntryProtoVector> entries) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (error != DRIVE_FILE_OK) {
    callback.Run(error,
                 hide_hosted_docs_,
                 scoped_ptr<DriveEntryProtoVector>());
    return;
  }
  DCHECK(entries.get());  // This is valid for empty directories too.

  callback.Run(DRIVE_FILE_OK, hide_hosted_docs_, entries.Pass());
}

void DriveFileSystem::RefreshDirectory(
    const base::FilePath& directory_path,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  // Make sure the destination directory exists.
  resource_metadata_->GetEntryInfoByPath(
      directory_path,
      base::Bind(&DriveFileSystem::RefreshDirectoryAfterGetEntryInfo,
                 weak_ptr_factory_.GetWeakPtr(),
                 directory_path,
                 callback));
}

void DriveFileSystem::RefreshDirectoryAfterGetEntryInfo(
    const base::FilePath& directory_path,
    const FileOperationCallback& callback,
    DriveFileError error,
    scoped_ptr<DriveEntryProto> entry_proto) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (error != DRIVE_FILE_OK) {
    callback.Run(error);
    return;
  }
  if (!entry_proto->file_info().is_directory()) {
    callback.Run(DRIVE_FILE_ERROR_NOT_A_DIRECTORY);
    return;
  }

  change_list_loader_->LoadDirectoryFromServer(
      entry_proto->resource_id(),
      callback);
}

void DriveFileSystem::UpdateFileByResourceId(
    const std::string& resource_id,
    const DriveClientContext& context,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  drive_operations_.UpdateFileByResourceId(resource_id, context, callback);
}

void DriveFileSystem::GetAvailableSpace(
    const GetAvailableSpaceCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  scheduler_->GetAboutResource(
      base::Bind(&DriveFileSystem::OnGetAboutResource,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback));
}

void DriveFileSystem::OnGetAboutResource(
    const GetAvailableSpaceCallback& callback,
    google_apis::GDataErrorCode status,
    scoped_ptr<google_apis::AboutResource> about_resource) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  DriveFileError error = util::GDataToDriveFileError(status);
  if (error != DRIVE_FILE_OK) {
    callback.Run(error, -1, -1);
    return;
  }
  DCHECK(about_resource);

  callback.Run(DRIVE_FILE_OK,
               about_resource->quota_bytes_total(),
               about_resource->quota_bytes_used());
}

void DriveFileSystem::OnSearch(
    bool shared_with_me,
    const SearchCallback& search_callback,
    const ScopedVector<google_apis::ResourceList>& feed_list,
    DriveFileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!search_callback.is_null());

  if (error != DRIVE_FILE_OK) {
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

  DCHECK_EQ(1u, feed_list.size());
  const google_apis::ResourceList* feed = feed_list[0];

  // TODO(tbarzic): Limit total number of returned results for the query.
  GURL next_feed;
  feed->GetNextFeedURL(&next_feed);

  const base::Closure callback = base::Bind(
      search_callback, DRIVE_FILE_OK, next_feed, base::Passed(&result_vec));

  const ScopedVector<google_apis::ResourceEntry>& entries = feed->entries();
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
    DriveEntryProto entry_proto =
        ConvertResourceEntryToDriveEntryProto(*entries[i]);

    const GetEntryInfoWithFilePathCallback entry_info_callback =
        base::Bind(&DriveFileSystem::AddToSearchResults,
                   weak_ptr_factory_.GetWeakPtr(),
                   results,
                   should_run_callback,
                   callback);

    // Some entries (e.g. shared_with_me files) are not tracked in
    // DriveResourceMetadata, so that we skip RefreshEntry() and call the
    // callback directly.
    if (!shared_with_me) {
      resource_metadata_->RefreshEntry(entry_proto, entry_info_callback);
    } else {
      entry_info_callback.Run(
          DRIVE_FILE_OK,
          base::FilePath::FromUTF8Unsafe(entry_proto.base_name()),
          scoped_ptr<DriveEntryProto>(new DriveEntryProto(entry_proto)));
    }
  }
}

void DriveFileSystem::AddToSearchResults(
    std::vector<SearchResultInfo>* results,
    bool should_run_callback,
    const base::Closure& callback,
    DriveFileError error,
    const base::FilePath& drive_file_path,
    scoped_ptr<DriveEntryProto> entry_proto) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // If a result is not present in our local file system snapshot, call
  // CheckForUpdates to refresh the snapshot with a delta feed. This may happen
  // if the entry has recently been added to the drive (and we still haven't
  // received its delta feed).
  if (error == DRIVE_FILE_OK) {
    DCHECK(entry_proto.get());
    results->push_back(SearchResultInfo(drive_file_path, *entry_proto.get()));
    DVLOG(1) << "AddToSearchResults " << drive_file_path.value();
  } else if (error == DRIVE_FILE_ERROR_NOT_FOUND) {
    CheckForUpdates();
  } else {
    NOTREACHED();
  }

  if (should_run_callback)
    callback.Run();
}

void DriveFileSystem::Search(const std::string& search_query,
                             bool shared_with_me,
                             const GURL& next_feed,
                             const SearchCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  change_list_loader_->SearchFromServer(
      search_query,
      shared_with_me,
      next_feed,
      base::Bind(&DriveFileSystem::OnSearch,
                 weak_ptr_factory_.GetWeakPtr(),
                 shared_with_me,
                 callback));
}

void DriveFileSystem::OnDirectoryChangedByOperation(
    const base::FilePath& directory_path) {
  OnDirectoryChanged(directory_path);
}

void DriveFileSystem::OnDirectoryChanged(const base::FilePath& directory_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  FOR_EACH_OBSERVER(DriveFileSystemObserver, observers_,
                    OnDirectoryChanged(directory_path));
}

void DriveFileSystem::OnResourceListFetched(int num_accumulated_entries) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  FOR_EACH_OBSERVER(DriveFileSystemObserver, observers_,
                    OnResourceListFetched(num_accumulated_entries));
}

void DriveFileSystem::OnFeedFromServerLoaded() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  FOR_EACH_OBSERVER(DriveFileSystemObserver, observers_,
                    OnFeedFromServerLoaded());
}

void DriveFileSystem::OnInitialFeedLoaded() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  FOR_EACH_OBSERVER(DriveFileSystemObserver,
                    observers_,
                    OnInitialLoadFinished());
}

void DriveFileSystem::OnFileDownloaded(
    const GetFileFromCacheParams& params,
    google_apis::GDataErrorCode status,
    const base::FilePath& downloaded_file_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!params.get_file_callback.is_null());

  // If user cancels download of a pinned-but-not-fetched file, mark file as
  // unpinned so that we do not sync the file again.
  if (status == google_apis::GDATA_CANCELLED) {
    cache_->GetCacheEntry(params.resource_id,
                          params.md5,
                          base::Bind(&DriveFileSystem::UnpinIfPinned,
                                     weak_ptr_factory_.GetWeakPtr(),
                                     params.resource_id,
                                     params.md5));
  }

  // At this point, the disk can be full or nearly full for several reasons:
  // - The expected file size was incorrect and the file was larger
  // - There was an in-flight download operation and it used up space
  // - The disk became full for some user actions we cannot control
  //   (ex. the user might have downloaded a large file from a regular web site)
  //
  // If we don't have enough space, we return PLATFORM_FILE_ERROR_NO_SPACE,
  // and try to free up space, even if the file was downloaded successfully.
  cache_->FreeDiskSpaceIfNeededFor(
      0,
      base::Bind(&DriveFileSystem::OnFileDownloadedAndSpaceChecked,
                 weak_ptr_factory_.GetWeakPtr(),
                 params,
                 status,
                 downloaded_file_path));
}

void DriveFileSystem::UnpinIfPinned(
    const std::string& resource_id,
    const std::string& md5,
    bool success,
    const DriveCacheEntry& cache_entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // TODO(hshi): http://crbug.com/127138 notify when file properties change.
  // This allows file manager to clear the "Available offline" checkbox.
  if (success && cache_entry.is_pinned()) {
    cache_->Unpin(resource_id,
                  md5,
                  base::Bind(&util::EmptyFileOperationCallback));
  }
}

void DriveFileSystem::OnFileDownloadedAndSpaceChecked(
    const GetFileFromCacheParams& params,
    google_apis::GDataErrorCode status,
    const base::FilePath& downloaded_file_path,
    bool has_enough_space) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!params.get_file_callback.is_null());

  DriveFileError error = util::GDataToDriveFileError(status);

  // Make sure that downloaded file is properly stored in cache. We don't have
  // to wait for this operation to finish since the user can already use the
  // downloaded file.
  if (error == DRIVE_FILE_OK) {
    if (has_enough_space) {
      cache_->Store(params.resource_id,
                    params.md5,
                    downloaded_file_path,
                    DriveCache::FILE_OPERATION_MOVE,
                    base::Bind(&util::EmptyFileOperationCallback));
      // Storing to cache changes the "offline available" status, hence notify.
      OnDirectoryChanged(params.virtual_file_path.DirName());
    } else {
      // If we don't have enough space, remove the downloaded file, and
      // report "no space" error.
      blocking_task_runner_->PostTask(
          FROM_HERE,
          base::Bind(base::IgnoreResult(&file_util::Delete),
                     downloaded_file_path,
                     false /* recursive*/));
      error = DRIVE_FILE_ERROR_NO_SPACE;
    }
  }

  params.get_file_callback.Run(error,
                               downloaded_file_path,
                               params.mime_type,
                               REGULAR_FILE);
}

void DriveFileSystem::NotifyFileSystemMounted() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DVLOG(1) << "File System is mounted";
  // Notify the observers that the file system is mounted.
  FOR_EACH_OBSERVER(DriveFileSystemObserver, observers_,
                    OnFileSystemMounted());
}

void DriveFileSystem::NotifyFileSystemToBeUnmounted() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DVLOG(1) << "File System is to be unmounted";
  // Notify the observers that the file system is being unmounted.
  FOR_EACH_OBSERVER(DriveFileSystemObserver, observers_,
                    OnFileSystemBeingUnmounted());
}

void DriveFileSystem::AddUploadedFile(
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
      ConvertResourceEntryToDriveEntryProto(*entry),
      base::Bind(&DriveFileSystem::AddUploadedFileToCache,
                 weak_ptr_factory_.GetWeakPtr(), params));
}

void DriveFileSystem::AddUploadedFileToCache(
    const AddUploadedFileParams& params,
    DriveFileError error,
    const base::FilePath& file_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!params.resource_id.empty());
  DCHECK(!params.md5.empty());
  DCHECK(!params.callback.is_null());

  if (error != DRIVE_FILE_OK) {
    params.callback.Run(error);
    return;
  }

  OnDirectoryChanged(file_path.DirName());

  cache_->Store(params.resource_id,
                params.md5,
                params.file_content_path,
                DriveCache::FILE_OPERATION_COPY,
                params.callback);
}

void DriveFileSystem::GetMetadata(
    const GetFilesystemMetadataCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  DriveFileSystemMetadata metadata;
  metadata.loaded = change_list_loader_->loaded();
  metadata.refreshing = change_list_loader_->refreshing();

  // Metadata related to delta update.
  metadata.push_notification_enabled = push_notification_enabled_;
  metadata.polling_interval_sec = polling_interval_sec_;
  metadata.last_update_check_time = last_update_check_time_;
  metadata.last_update_check_error = last_update_check_error_;

  resource_metadata_->GetLargestChangestamp(
      base::Bind(&OnGetLargestChangestamp, metadata, callback));
}

void DriveFileSystem::OnDisableDriveHostedFilesChanged() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  PrefService* pref_service = profile_->GetPrefs();
  SetHideHostedDocuments(
      pref_service->GetBoolean(prefs::kDisableDriveHostedFiles));
}

void DriveFileSystem::SetHideHostedDocuments(bool hide) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (hide == hide_hosted_docs_)
    return;

  hide_hosted_docs_ = hide;

  // Kick off directory refresh when this setting changes.
  FOR_EACH_OBSERVER(DriveFileSystemObserver, observers_,
                    OnDirectoryChanged(util::GetDriveMyDriveRootPath()));
}

//============= DriveFileSystem: internal helper functions =====================

void DriveFileSystem::InitializePreferenceObserver() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  pref_registrar_.reset(new PrefChangeRegistrar());
  pref_registrar_->Init(profile_->GetPrefs());
  pref_registrar_->Add(
      prefs::kDisableDriveHostedFiles,
      base::Bind(&DriveFileSystem::OnDisableDriveHostedFilesChanged,
                 base::Unretained(this)));
}

void DriveFileSystem::OpenFile(const base::FilePath& file_path,
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
        base::Bind(callback, DRIVE_FILE_ERROR_IN_USE, base::FilePath()));
    return;
  }
  open_files_.insert(file_path);

  resource_metadata_->GetEntryInfoByPath(
      file_path,
      base::Bind(&DriveFileSystem::OnGetEntryInfoCompleteForOpenFile,
                 weak_ptr_factory_.GetWeakPtr(),
                 file_path,
                 base::Bind(&DriveFileSystem::OnOpenFileFinished,
                            weak_ptr_factory_.GetWeakPtr(),
                            file_path,
                            callback)));
}

void DriveFileSystem::OnGetEntryInfoCompleteForOpenFile(
    const base::FilePath& file_path,
    const OpenFileCallback& callback,
    DriveFileError error,
    scoped_ptr<DriveEntryProto> entry_proto) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  DCHECK(entry_proto.get() || error != DRIVE_FILE_OK);

  if (entry_proto.get() && !entry_proto->has_file_specific_info())
    error = DRIVE_FILE_ERROR_NOT_FOUND;

  if (error == DRIVE_FILE_OK) {
    if (entry_proto->file_specific_info().file_md5().empty() ||
        entry_proto->file_specific_info().is_hosted_document()) {
      // No support for opening a directory or hosted document.
      error = DRIVE_FILE_ERROR_INVALID_OPERATION;
    }
  }

  if (error != DRIVE_FILE_OK) {
    callback.Run(error, base::FilePath());
    return;
  }

  DCHECK(!entry_proto->resource_id().empty());
  // Extract a pointer before we call Pass() so we can use it below.
  DriveEntryProto* entry_proto_ptr = entry_proto.get();
  GetResolvedFileByPath(
      file_path,
      DriveClientContext(USER_INITIATED),
      base::Bind(&DriveFileSystem::OnGetFileCompleteForOpenFile,
                 weak_ptr_factory_.GetWeakPtr(),
                 GetFileCompleteForOpenParams(
                     callback,
                     entry_proto_ptr->resource_id(),
                     entry_proto_ptr->file_specific_info().file_md5())),
      google_apis::GetContentCallback(),
      entry_proto.Pass());
}

void DriveFileSystem::OnGetFileCompleteForOpenFile(
    const GetFileCompleteForOpenParams& params,
    DriveFileError error,
    const base::FilePath& file_path,
    const std::string& mime_type,
    DriveFileType file_type) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!params.callback.is_null());

  if (error != DRIVE_FILE_OK) {
    params.callback.Run(error, base::FilePath());
    return;
  }

  // OpenFile ensures that the file is a regular file.
  DCHECK_EQ(REGULAR_FILE, file_type);

  cache_->MarkDirty(
      params.resource_id,
      params.md5,
      base::Bind(&DriveFileSystem::OnMarkDirtyInCacheCompleteForOpenFile,
                 weak_ptr_factory_.GetWeakPtr(),
                 params));
}

void DriveFileSystem::OnMarkDirtyInCacheCompleteForOpenFile(
    const GetFileCompleteForOpenParams& params,
    DriveFileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!params.callback.is_null());

  if (error != DRIVE_FILE_OK) {
    params.callback.Run(error, base::FilePath());
    return;
  }

  cache_->GetFile(params.resource_id, params.md5, params.callback);
}

void DriveFileSystem::OnOpenFileFinished(
    const base::FilePath& file_path,
    const OpenFileCallback& callback,
    DriveFileError result,
    const base::FilePath& cache_file_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  // All the invocation of |callback| from operations initiated from OpenFile
  // must go through here. Removes the |file_path| from the remembered set when
  // the file was not successfully opened.
  if (result != DRIVE_FILE_OK)
    open_files_.erase(file_path);

  callback.Run(result, cache_file_path);
}

void DriveFileSystem::CloseFile(const base::FilePath& file_path,
                                const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (open_files_.find(file_path) == open_files_.end()) {
    // The file is not being opened.
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE,
        base::Bind(callback, DRIVE_FILE_ERROR_NOT_FOUND));
    return;
  }

  // Step 1 of CloseFile: Get resource_id and md5 for |file_path|.
  resource_metadata_->GetEntryInfoByPath(
      file_path,
      base::Bind(&DriveFileSystem::CloseFileAfterGetEntryInfo,
                 weak_ptr_factory_.GetWeakPtr(),
                 file_path,
                 base::Bind(&DriveFileSystem::CloseFileFinalize,
                            weak_ptr_factory_.GetWeakPtr(),
                            file_path,
                            callback)));
}

void DriveFileSystem::CloseFileAfterGetEntryInfo(
    const base::FilePath& file_path,
    const FileOperationCallback& callback,
    DriveFileError error,
    scoped_ptr<DriveEntryProto> entry_proto) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (entry_proto.get() && !entry_proto->has_file_specific_info())
    error = DRIVE_FILE_ERROR_NOT_FOUND;

  if (error != DRIVE_FILE_OK) {
    callback.Run(error);
    return;
  }

  // Step 2 of CloseFile: Commit the modification in cache. This will trigger
  // background upload.
  // TODO(benchan,kinaba): Call ClearDirtyInCache instead of CommitDirtyInCache
  // if the file has not been modified. Come up with a way to detect the
  // intactness effectively, or provide a method for user to declare it when
  // calling CloseFile().
  cache_->CommitDirty(entry_proto->resource_id(),
                      entry_proto->file_specific_info().file_md5(),
                      callback);
}

void DriveFileSystem::CloseFileFinalize(const base::FilePath& file_path,
                                        const FileOperationCallback& callback,
                                        DriveFileError result) {
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

void DriveFileSystem::CheckLocalModificationAndRun(
    scoped_ptr<DriveEntryProto> entry_proto,
    const GetEntryInfoCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(entry_proto.get());
  DCHECK(!callback.is_null());

  // For entries that will never be cached, use the original entry info as is.
  if (!entry_proto->has_file_specific_info() ||
      entry_proto->file_specific_info().is_hosted_document()) {
    callback.Run(DRIVE_FILE_OK, entry_proto.Pass());
    return;
  }

  // Checks if the file is cached and modified locally.
  const std::string resource_id = entry_proto->resource_id();
  const std::string md5 = entry_proto->file_specific_info().file_md5();
  cache_->GetCacheEntry(
      resource_id,
      md5,
      base::Bind(
          &DriveFileSystem::CheckLocalModificationAndRunAfterGetCacheEntry,
          weak_ptr_factory_.GetWeakPtr(),
          base::Passed(&entry_proto),
          callback));
}

void DriveFileSystem::CheckLocalModificationAndRunAfterGetCacheEntry(
    scoped_ptr<DriveEntryProto> entry_proto,
    const GetEntryInfoCallback& callback,
    bool success,
    const DriveCacheEntry& cache_entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  // When no dirty cache is found, use the original entry info as is.
  if (!success || !cache_entry.is_dirty()) {
    callback.Run(DRIVE_FILE_OK, entry_proto.Pass());
    return;
  }

  // Gets the cache file path.
  const std::string& resource_id = entry_proto->resource_id();
  const std::string& md5 = entry_proto->file_specific_info().file_md5();
  cache_->GetFile(
      resource_id,
      md5,
      base::Bind(
          &DriveFileSystem::CheckLocalModificationAndRunAfterGetCacheFile,
          weak_ptr_factory_.GetWeakPtr(),
          base::Passed(&entry_proto),
          callback));
}

void DriveFileSystem::CheckLocalModificationAndRunAfterGetCacheFile(
    scoped_ptr<DriveEntryProto> entry_proto,
    const GetEntryInfoCallback& callback,
    DriveFileError error,
    const base::FilePath& local_cache_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  // When no dirty cache is found, use the original entry info as is.
  if (error != DRIVE_FILE_OK) {
    callback.Run(DRIVE_FILE_OK, entry_proto.Pass());
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
      base::Bind(&DriveFileSystem::CheckLocalModificationAndRunAfterGetFileInfo,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Passed(&entry_proto),
                 callback,
                 base::Owned(file_info)));
}

void DriveFileSystem::CheckLocalModificationAndRunAfterGetFileInfo(
    scoped_ptr<DriveEntryProto> entry_proto,
    const GetEntryInfoCallback& callback,
    base::PlatformFileInfo* file_info,
    bool get_file_info_result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (!get_file_info_result) {
    callback.Run(DRIVE_FILE_ERROR_NOT_FOUND, scoped_ptr<DriveEntryProto>());
    return;
  }

  PlatformFileInfoProto entry_file_info;
  util::ConvertPlatformFileInfoToProto(*file_info, &entry_file_info);
  *entry_proto->mutable_file_info() = entry_file_info;
  callback.Run(DRIVE_FILE_OK, entry_proto.Pass());
}

}  // namespace drive
