// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend_v1/api_util.h"

#include <algorithm>
#include <functional>
#include <sstream>
#include <string>

#include "base/file_util.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/values.h"
#include "chrome/browser/drive/drive_api_service.h"
#include "chrome/browser/drive/drive_api_util.h"
#include "chrome/browser/drive/drive_uploader.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_backend_constants.h"
#include "chrome/browser/sync_file_system/drive_backend_v1/drive_file_sync_util.h"
#include "chrome/browser/sync_file_system/logger.h"
#include "chrome/browser/sync_file_system/syncable_file_system_util.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "google_apis/drive/drive_api_parser.h"
#include "google_apis/drive/drive_api_url_generator.h"
#include "google_apis/drive/gdata_wapi_url_generator.h"

namespace sync_file_system {
namespace drive_backend {

namespace {

enum ParentType {
  PARENT_TYPE_ROOT_OR_EMPTY,
  PARENT_TYPE_DIRECTORY,
};

const char kFakeAccountId[] = "test_user@gmail.com";

void EmptyGDataErrorCodeCallback(google_apis::GDataErrorCode error) {}

bool HasParentLinkTo(const ScopedVector<google_apis::Link>& links,
                     const std::string& parent_resource_id,
                     ParentType parent_type) {
  bool has_parent = false;

  for (ScopedVector<google_apis::Link>::const_iterator itr = links.begin();
       itr != links.end(); ++itr) {
    if ((*itr)->type() == google_apis::Link::LINK_PARENT) {
      has_parent = true;
      if (drive::util::ExtractResourceIdFromUrl((*itr)->href()) ==
          parent_resource_id)
        return true;
    }
  }

  return parent_type == PARENT_TYPE_ROOT_OR_EMPTY && !has_parent;
}

struct TitleAndParentQuery
    : std::unary_function<const google_apis::ResourceEntry*, bool> {
  TitleAndParentQuery(const std::string& title,
                      const std::string& parent_resource_id,
                      ParentType parent_type)
      : title(title),
        parent_resource_id(parent_resource_id),
        parent_type(parent_type) {}

  bool operator()(const google_apis::ResourceEntry* entry) const {
    return entry->title() == title &&
           HasParentLinkTo(entry->links(), parent_resource_id, parent_type);
  }

  const std::string& title;
  const std::string& parent_resource_id;
  ParentType parent_type;
};

void FilterEntriesByTitleAndParent(
    ScopedVector<google_apis::ResourceEntry>* entries,
    const std::string& title,
    const std::string& parent_resource_id,
    ParentType parent_type) {
  typedef ScopedVector<google_apis::ResourceEntry>::iterator iterator;
  iterator itr = std::partition(entries->begin(),
                                entries->end(),
                                TitleAndParentQuery(title,
                                                    parent_resource_id,
                                                    parent_type));
  entries->erase(itr, entries->end());
}

google_apis::ResourceEntry* GetDocumentByTitleAndParent(
    const ScopedVector<google_apis::ResourceEntry>& entries,
    const std::string& title,
    const std::string& parent_resource_id,
    ParentType parent_type) {
  typedef ScopedVector<google_apis::ResourceEntry>::const_iterator iterator;
  iterator found =
      std::find_if(entries.begin(),
                   entries.end(),
                   TitleAndParentQuery(title, parent_resource_id, parent_type));
  if (found != entries.end())
    return *found;
  return NULL;
}

void EntryAdapterForEnsureTitleUniqueness(
    scoped_ptr<google_apis::ResourceEntry> entry,
    const APIUtil::EnsureUniquenessCallback& callback,
    APIUtil::EnsureUniquenessStatus status,
    google_apis::GDataErrorCode error) {
  callback.Run(error, status, entry.Pass());
}

void UploadResultAdapter(const APIUtil::ResourceEntryCallback& callback,
                         google_apis::GDataErrorCode error,
                         const GURL& upload_location,
                         scoped_ptr<google_apis::FileResource> entry) {
  callback.Run(error, entry ?
               drive::util::ConvertFileResourceToResourceEntry(*entry) :
               scoped_ptr<google_apis::ResourceEntry>());
}

std::string GetMimeTypeFromTitle(const std::string& title) {
  base::FilePath::StringType extension =
      base::FilePath::FromUTF8Unsafe(title).Extension();
  std::string mime_type;
  if (extension.empty() ||
      !net::GetWellKnownMimeTypeFromExtension(extension.substr(1), &mime_type))
    return kMimeTypeOctetStream;
  return mime_type;
}

bool CreateTemporaryFile(const base::FilePath& dir_path,
                         webkit_blob::ScopedFile* temp_file) {
  base::FilePath temp_file_path;
  const bool success = base::CreateDirectory(dir_path) &&
      base::CreateTemporaryFileInDir(dir_path, &temp_file_path);
  if (!success)
    return success;
  *temp_file =
      webkit_blob::ScopedFile(temp_file_path,
                              webkit_blob::ScopedFile::DELETE_ON_SCOPE_OUT,
                              base::ThreadTaskRunnerHandle::Get().get());
  return success;
}

}  // namespace

APIUtil::APIUtil(Profile* profile,
                 const base::FilePath& temp_dir_path)
    : oauth_service_(ProfileOAuth2TokenServiceFactory::GetForProfile(profile)),
      signin_manager_(SigninManagerFactory::GetForProfile(profile)),
      upload_next_key_(0),
      temp_dir_path_(temp_dir_path),
      has_initialized_token_(false) {
  base::SequencedWorkerPool* blocking_pool =
      content::BrowserThread::GetBlockingPool();
  scoped_refptr<base::SequencedTaskRunner> task_runner(
      blocking_pool->GetSequencedTaskRunner(blocking_pool->GetSequenceToken()));
  DCHECK(!IsDriveAPIDisabled());
  drive_service_.reset(new drive::DriveAPIService(
      oauth_service_,
      profile->GetRequestContext(),
      task_runner.get(),
      GURL(google_apis::DriveApiUrlGenerator::kBaseUrlForProduction),
      GURL(google_apis::DriveApiUrlGenerator::kBaseDownloadUrlForProduction),
      GURL(google_apis::GDataWapiUrlGenerator::kBaseUrlForProduction),
      std::string() /* custom_user_agent */));
  drive_service_->Initialize(signin_manager_->GetAuthenticatedAccountId());
  drive_service_->AddObserver(this);
  has_initialized_token_ = drive_service_->HasRefreshToken();

  net::NetworkChangeNotifier::AddConnectionTypeObserver(this);

  drive_uploader_.reset(new drive::DriveUploader(
      drive_service_.get(), content::BrowserThread::GetBlockingPool()));
}

scoped_ptr<APIUtil> APIUtil::CreateForTesting(
    const base::FilePath& temp_dir_path,
    scoped_ptr<drive::DriveServiceInterface> drive_service,
    scoped_ptr<drive::DriveUploaderInterface> drive_uploader) {
  return make_scoped_ptr(new APIUtil(
      temp_dir_path,
      drive_service.Pass(),
      drive_uploader.Pass(),
      kFakeAccountId));
}

APIUtil::APIUtil(const base::FilePath& temp_dir_path,
                 scoped_ptr<drive::DriveServiceInterface> drive_service,
                 scoped_ptr<drive::DriveUploaderInterface> drive_uploader,
                 const std::string& account_id)
    : upload_next_key_(0),
      temp_dir_path_(temp_dir_path) {
  drive_service_ = drive_service.Pass();
  drive_service_->Initialize(account_id);
  drive_service_->AddObserver(this);
  net::NetworkChangeNotifier::AddConnectionTypeObserver(this);

  drive_uploader_ = drive_uploader.Pass();
}

APIUtil::~APIUtil() {
  DCHECK(CalledOnValidThread());
  net::NetworkChangeNotifier::RemoveConnectionTypeObserver(this);
  drive_service_->RemoveObserver(this);
}

void APIUtil::AddObserver(APIUtilObserver* observer) {
  DCHECK(CalledOnValidThread());
  observers_.AddObserver(observer);
}

void APIUtil::RemoveObserver(APIUtilObserver* observer) {
  DCHECK(CalledOnValidThread());
  observers_.RemoveObserver(observer);
}

void APIUtil::GetDriveRootResourceId(const GDataErrorCallback& callback) {
  DCHECK(CalledOnValidThread());
  DCHECK(!IsDriveAPIDisabled());
  DVLOG(2) << "Getting resource id for Drive root";

  drive_service_->GetAboutResource(
      base::Bind(&APIUtil::DidGetDriveRootResourceId, AsWeakPtr(), callback));
}

void APIUtil::DidGetDriveRootResourceId(
    const GDataErrorCallback& callback,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::AboutResource> about_resource) {
  DCHECK(CalledOnValidThread());

  if (error != google_apis::HTTP_SUCCESS) {
    DVLOG(2) << "Error on getting resource id for Drive root: " << error;
    callback.Run(error);
    return;
  }

  DCHECK(about_resource);
  root_resource_id_ = about_resource->root_folder_id();
  DCHECK(!root_resource_id_.empty());
  DVLOG(2) << "Got resource id for Drive root: " << root_resource_id_;
  callback.Run(error);
}

void APIUtil::GetDriveDirectoryForSyncRoot(const ResourceIdCallback& callback) {
  DCHECK(CalledOnValidThread());

  if (GetRootResourceId().empty()) {
    GetDriveRootResourceId(
        base::Bind(&APIUtil::DidGetDriveRootResourceIdForGetSyncRoot,
                   AsWeakPtr(), callback));
    return;
  }

  DVLOG(2) << "Getting Drive directory for SyncRoot";
  std::string directory_name(GetSyncRootDirectoryName());
  SearchByTitle(directory_name,
                std::string(),
                base::Bind(&APIUtil::DidGetDirectory,
                           AsWeakPtr(),
                           std::string(),
                           directory_name,
                           callback));
}

void APIUtil::DidGetDriveRootResourceIdForGetSyncRoot(
    const ResourceIdCallback& callback,
    google_apis::GDataErrorCode error) {
  DCHECK(CalledOnValidThread());
  if (error != google_apis::HTTP_SUCCESS) {
    DVLOG(2) << "Error on getting Drive directory for SyncRoot: " << error;
    callback.Run(error, std::string());
    return;
  }
  GetDriveDirectoryForSyncRoot(callback);
}

void APIUtil::GetDriveDirectoryForOrigin(
    const std::string& sync_root_resource_id,
    const GURL& origin,
    const ResourceIdCallback& callback) {
  DCHECK(CalledOnValidThread());
  DVLOG(2) << "Getting Drive directory for Origin: " << origin;

  std::string directory_name(OriginToDirectoryTitle(origin));
  SearchByTitle(directory_name,
                sync_root_resource_id,
                base::Bind(&APIUtil::DidGetDirectory,
                           AsWeakPtr(),
                           sync_root_resource_id,
                           directory_name,
                           callback));
}

void APIUtil::DidGetDirectory(const std::string& parent_resource_id,
                              const std::string& directory_name,
                              const ResourceIdCallback& callback,
                              google_apis::GDataErrorCode error,
                              scoped_ptr<google_apis::ResourceList> feed) {
  DCHECK(CalledOnValidThread());
  DCHECK(base::IsStringASCII(directory_name));

  if (error != google_apis::HTTP_SUCCESS) {
    DVLOG(2) << "Error on getting Drive directory: " << error;
    callback.Run(error, std::string());
    return;
  }

  std::string resource_id;
  ParentType parent_type = PARENT_TYPE_DIRECTORY;
  if (parent_resource_id.empty()) {
    resource_id = GetRootResourceId();
    DCHECK(!resource_id.empty());
    parent_type = PARENT_TYPE_ROOT_OR_EMPTY;
  } else {
    resource_id = parent_resource_id;
  }
  std::string title(directory_name);
  google_apis::ResourceEntry* entry = GetDocumentByTitleAndParent(
      feed->entries(), title, resource_id, parent_type);
  if (!entry) {
    DVLOG(2) << "Directory not found. Creating: " << directory_name;
    drive_service_->AddNewDirectory(
        resource_id,
        directory_name,
        drive::DriveServiceInterface::AddNewDirectoryOptions(),
        base::Bind(&APIUtil::DidCreateDirectory,
                   AsWeakPtr(),
                   parent_resource_id,
                   title,
                   callback));
    return;
  }
  DVLOG(2) << "Found Drive directory.";

  // TODO(tzik): Handle error.
  DCHECK_EQ(google_apis::ResourceEntry::ENTRY_KIND_FOLDER, entry->kind());
  DCHECK_EQ(directory_name, entry->title());

  if (entry->title() == GetSyncRootDirectoryName())
    EnsureSyncRootIsNotInMyDrive(entry->resource_id());

  callback.Run(error, entry->resource_id());
}

void APIUtil::DidCreateDirectory(const std::string& parent_resource_id,
                                 const std::string& title,
                                 const ResourceIdCallback& callback,
                                 google_apis::GDataErrorCode error,
                                 scoped_ptr<google_apis::FileResource> entry) {
  DCHECK(CalledOnValidThread());

  if (error != google_apis::HTTP_SUCCESS &&
      error != google_apis::HTTP_CREATED) {
    DVLOG(2) << "Error on creating Drive directory: " << error;
    callback.Run(error, std::string());
    return;
  }
  DVLOG(2) << "Created Drive directory.";

  DCHECK(entry);
  // Check if any other client creates a directory with same title.
  EnsureTitleUniqueness(
      parent_resource_id,
      title,
      base::Bind(&APIUtil::DidEnsureUniquenessForCreateDirectory,
                 AsWeakPtr(),
                 callback));
}

void APIUtil::DidEnsureUniquenessForCreateDirectory(
    const ResourceIdCallback& callback,
    google_apis::GDataErrorCode error,
    EnsureUniquenessStatus status,
    scoped_ptr<google_apis::ResourceEntry> entry) {
  DCHECK(CalledOnValidThread());

  if (error != google_apis::HTTP_SUCCESS) {
    callback.Run(error, std::string());
    return;
  }

  if (status == NO_DUPLICATES_FOUND)
    error = google_apis::HTTP_CREATED;

  DCHECK(entry) << "No entry: " << error;

  if (!entry->is_folder()) {
    // TODO(kinuko): Fix this. http://crbug.com/237090
    util::Log(
        logging::LOG_ERROR,
        FROM_HERE,
        "A file is left for CreateDirectory due to file-folder conflict!");
    callback.Run(google_apis::HTTP_CONFLICT, std::string());
    return;
  }

  if (entry->title() == GetSyncRootDirectoryName())
    EnsureSyncRootIsNotInMyDrive(entry->resource_id());

  callback.Run(error, entry->resource_id());
}

void APIUtil::GetLargestChangeStamp(const ChangeStampCallback& callback) {
  DCHECK(CalledOnValidThread());
  DVLOG(2) << "Getting largest change id";

  drive_service_->GetAboutResource(
      base::Bind(&APIUtil::DidGetLargestChangeStamp, AsWeakPtr(), callback));
}

void APIUtil::GetResourceEntry(const std::string& resource_id,
                               const ResourceEntryCallback& callback) {
  DCHECK(CalledOnValidThread());
  DVLOG(2) << "Getting ResourceEntry for: " << resource_id;

  drive_service_->GetFileResource(
      resource_id,
      base::Bind(&APIUtil::DidGetFileResource, AsWeakPtr(), callback));
}

void APIUtil::DidGetLargestChangeStamp(
    const ChangeStampCallback& callback,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::AboutResource> about_resource) {
  DCHECK(CalledOnValidThread());

  int64 largest_change_id = 0;
  if (error == google_apis::HTTP_SUCCESS) {
    DCHECK(about_resource);
    largest_change_id = about_resource->largest_change_id();
    root_resource_id_ = about_resource->root_folder_id();
    DVLOG(2) << "Got largest change id: " << largest_change_id;
  } else {
    DVLOG(2) << "Error on getting largest change id: " << error;
  }

  callback.Run(error, largest_change_id);
}

void APIUtil::SearchByTitle(const std::string& title,
                            const std::string& directory_resource_id,
                            const ResourceListCallback& callback) {
  DCHECK(CalledOnValidThread());
  DCHECK(!title.empty());
  DVLOG(2) << "Searching resources in the directory [" << directory_resource_id
           << "] with title [" << title << "]";

  drive_service_->SearchByTitle(
      title,
      directory_resource_id,
      base::Bind(&APIUtil::DidGetFileList, AsWeakPtr(), callback));
}

void APIUtil::ListFiles(const std::string& directory_resource_id,
                        const ResourceListCallback& callback) {
  DCHECK(CalledOnValidThread());
  DVLOG(2) << "Listing resources in the directory [" << directory_resource_id
           << "]";

  drive_service_->GetFileListInDirectory(
      directory_resource_id,
      base::Bind(&APIUtil::DidGetFileList, AsWeakPtr(), callback));
}

void APIUtil::ListChanges(int64 start_changestamp,
                          const ResourceListCallback& callback) {
  DCHECK(CalledOnValidThread());
  DVLOG(2) << "Listing changes since: " << start_changestamp;

  drive_service_->GetChangeList(
      start_changestamp,
      base::Bind(&APIUtil::DidGetChangeList, AsWeakPtr(), callback));
}

void APIUtil::ContinueListing(const GURL& next_link,
                              const ResourceListCallback& callback) {
  DCHECK(CalledOnValidThread());
  DVLOG(2) << "Continue listing on feed: " << next_link.spec();

  drive_service_->GetRemainingFileList(
      next_link,
      base::Bind(&APIUtil::DidGetFileList, AsWeakPtr(), callback));
}

void APIUtil::DownloadFile(const std::string& resource_id,
                           const std::string& local_file_md5,
                           const DownloadFileCallback& callback) {
  DCHECK(CalledOnValidThread());
  DCHECK(!temp_dir_path_.empty());
  DVLOG(2) << "Downloading file [" << resource_id << "]";

  scoped_ptr<webkit_blob::ScopedFile> temp_file(new webkit_blob::ScopedFile);
  webkit_blob::ScopedFile* temp_file_ptr = temp_file.get();
  content::BrowserThread::PostTaskAndReplyWithResult(
      content::BrowserThread::FILE, FROM_HERE,
      base::Bind(&CreateTemporaryFile, temp_dir_path_, temp_file_ptr),
      base::Bind(&APIUtil::DidGetTemporaryFileForDownload,
                 AsWeakPtr(), resource_id, local_file_md5,
                 base::Passed(&temp_file), callback));
}

void APIUtil::UploadNewFile(const std::string& directory_resource_id,
                            const base::FilePath& local_file_path,
                            const std::string& title,
                            const UploadFileCallback& callback) {
  DCHECK(CalledOnValidThread());
  DVLOG(2) << "Uploading new file into the directory [" << directory_resource_id
           << "] with title [" << title << "]";

  std::string mime_type = GetMimeTypeFromTitle(title);
  UploadKey upload_key = RegisterUploadCallback(callback);
  ResourceEntryCallback did_upload_callback =
      base::Bind(&APIUtil::DidUploadNewFile,
                 AsWeakPtr(),
                 directory_resource_id,
                 title,
                 upload_key);
  drive_uploader_->UploadNewFile(
      directory_resource_id,
      local_file_path,
      title,
      mime_type,
      drive::DriveUploader::UploadNewFileOptions(),
      base::Bind(&UploadResultAdapter, did_upload_callback),
      google_apis::ProgressCallback());
}

void APIUtil::UploadExistingFile(const std::string& resource_id,
                                 const std::string& remote_file_md5,
                                 const base::FilePath& local_file_path,
                                 const UploadFileCallback& callback) {
  DCHECK(CalledOnValidThread());
  DVLOG(2) << "Uploading existing file [" << resource_id << "]";
  drive_service_->GetFileResource(
      resource_id,
      base::Bind(&APIUtil::DidGetFileResource,
                 AsWeakPtr(),
                 base::Bind(&APIUtil::UploadExistingFileInternal,
                            AsWeakPtr(),
                            remote_file_md5,
                            local_file_path,
                            callback)));
}

void APIUtil::CreateDirectory(const std::string& parent_resource_id,
                              const std::string& title,
                              const ResourceIdCallback& callback) {
  DCHECK(CalledOnValidThread());
  // TODO(kinuko): This will call EnsureTitleUniqueness and will delete
  // directories if there're duplicated directories. This must be ok
  // for current design but we'll need to merge directories when we support
  // 'real' directories.
  drive_service_->AddNewDirectory(
      parent_resource_id,
      title,
      drive::DriveServiceInterface::AddNewDirectoryOptions(),
      base::Bind(&APIUtil::DidCreateDirectory,
                 AsWeakPtr(),
                 parent_resource_id,
                 title,
                 callback));
}

void APIUtil::DeleteFile(const std::string& resource_id,
                         const std::string& remote_file_md5,
                         const GDataErrorCallback& callback) {
  DCHECK(CalledOnValidThread());
  DVLOG(2) << "Deleting file: " << resource_id;

  // Load actual remote_file_md5 to check for conflict before deletion.
  if (!remote_file_md5.empty()) {
    drive_service_->GetFileResource(
        resource_id,
        base::Bind(&APIUtil::DidGetFileResource,
                   AsWeakPtr(),
                   base::Bind(&APIUtil::DeleteFileInternal,
                              AsWeakPtr(),
                              remote_file_md5,
                              callback)));
    return;
  }

  // Expected remote_file_md5 is empty so do a force delete.
  drive_service_->TrashResource(
      resource_id,
      base::Bind(&APIUtil::DidDeleteFile, AsWeakPtr(), callback));
  return;
}

void APIUtil::EnsureSyncRootIsNotInMyDrive(
    const std::string& sync_root_resource_id) {
  DCHECK(CalledOnValidThread());

  if (GetRootResourceId().empty()) {
    GetDriveRootResourceId(
        base::Bind(&APIUtil::DidGetDriveRootResourceIdForEnsureSyncRoot,
                   AsWeakPtr(), sync_root_resource_id));
    return;
  }

  DVLOG(2) << "Ensuring the sync root directory is not in 'My Drive'.";
  drive_service_->RemoveResourceFromDirectory(
      GetRootResourceId(),
      sync_root_resource_id,
      base::Bind(&EmptyGDataErrorCodeCallback));
}

void APIUtil::DidGetDriveRootResourceIdForEnsureSyncRoot(
    const std::string& sync_root_resource_id,
    google_apis::GDataErrorCode error) {
  DCHECK(CalledOnValidThread());

  if (error != google_apis::HTTP_SUCCESS) {
    DVLOG(2) << "Error on ensuring the sync root directory is not in"
             << " 'My Drive': " << error;
    // Give up ensuring the sync root directory is not in 'My Drive'. This will
    // be retried at some point.
    return;
  }

  DCHECK(!GetRootResourceId().empty());
  EnsureSyncRootIsNotInMyDrive(sync_root_resource_id);
}

// static
std::string APIUtil::GetSyncRootDirectoryName() {
  return kSyncRootFolderTitle;
}

// static
std::string APIUtil::OriginToDirectoryTitle(const GURL& origin) {
  DCHECK(origin.SchemeIs(extensions::kExtensionScheme));
  return origin.host();
}

// static
GURL APIUtil::DirectoryTitleToOrigin(const std::string& title) {
  return extensions::Extension::GetBaseURLFromExtensionId(title);
}

void APIUtil::OnReadyToSendRequests() {
  DCHECK(CalledOnValidThread());
  if (!has_initialized_token_) {
    drive_service_->Initialize(signin_manager_->GetAuthenticatedAccountId());
    has_initialized_token_ = true;
  }
  FOR_EACH_OBSERVER(APIUtilObserver, observers_, OnAuthenticated());
}

void APIUtil::OnConnectionTypeChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  DCHECK(CalledOnValidThread());
  if (type != net::NetworkChangeNotifier::CONNECTION_NONE) {
    FOR_EACH_OBSERVER(APIUtilObserver, observers_, OnNetworkConnected());
    return;
  }
  // We're now disconnected, reset the drive_uploader_ to force stop
  // uploading, otherwise the uploader may get stuck.
  // TODO(kinuko): Check the uploader behavior if it's the expected behavior
  // (http://crbug.com/223818)
  CancelAllUploads(google_apis::GDATA_NO_CONNECTION);
}

void APIUtil::DidGetFileList(const ResourceListCallback& callback,
                             google_apis::GDataErrorCode error,
                             scoped_ptr<google_apis::FileList> file_list) {
  DCHECK(CalledOnValidThread());

  if (error != google_apis::HTTP_SUCCESS) {
    DVLOG(2) << "Error on listing files: " << error;
    callback.Run(error, scoped_ptr<google_apis::ResourceList>());
    return;
  }

  DVLOG(2) << "Got file list";
  DCHECK(file_list);
  callback.Run(error, drive::util::ConvertFileListToResourceList(*file_list));
}

void APIUtil::DidGetChangeList(
    const ResourceListCallback& callback,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::ChangeList> change_list) {
  DCHECK(CalledOnValidThread());

  if (error != google_apis::HTTP_SUCCESS) {
    DVLOG(2) << "Error on listing changes: " << error;
    callback.Run(error, scoped_ptr<google_apis::ResourceList>());
    return;
  }

  DVLOG(2) << "Got change list";
  DCHECK(change_list);
  callback.Run(error,
               drive::util::ConvertChangeListToResourceList(*change_list));
}

void APIUtil::DidGetFileResource(
    const ResourceEntryCallback& callback,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::FileResource> entry) {
  DCHECK(CalledOnValidThread());

  if (error != google_apis::HTTP_SUCCESS) {
    DVLOG(2) << "Error on getting resource entry:" << error;
    callback.Run(error, scoped_ptr<google_apis::ResourceEntry>());
    return;
  }
  DCHECK(entry);

  if (entry->labels().is_trashed()) {
    DVLOG(2) << "Got resource entry, the entry was trashed.";
    callback.Run(google_apis::HTTP_NOT_FOUND,
                 drive::util::ConvertFileResourceToResourceEntry(*entry));
    return;
  }

  DVLOG(2) << "Got resource entry";
  callback.Run(error, drive::util::ConvertFileResourceToResourceEntry(*entry));
}

void APIUtil::DidGetTemporaryFileForDownload(
    const std::string& resource_id,
    const std::string& local_file_md5,
    scoped_ptr<webkit_blob::ScopedFile> local_file,
    const DownloadFileCallback& callback,
    bool success) {
  if (!success) {
    DVLOG(2) << "Error in creating a temp file under "
             << temp_dir_path_.value();
    callback.Run(google_apis::GDATA_FILE_ERROR, std::string(), 0, base::Time(),
                 local_file->Pass());
    return;
  }
  drive_service_->GetFileResource(
      resource_id,
      base::Bind(&APIUtil::DidGetFileResource,
                 AsWeakPtr(),
                 base::Bind(&APIUtil::DownloadFileInternal,
                            AsWeakPtr(),
                            local_file_md5,
                            base::Passed(&local_file),
                            callback)));
}

void APIUtil::DownloadFileInternal(
    const std::string& local_file_md5,
    scoped_ptr<webkit_blob::ScopedFile> local_file,
    const DownloadFileCallback& callback,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::ResourceEntry> entry) {
  DCHECK(CalledOnValidThread());

  if (error != google_apis::HTTP_SUCCESS) {
    DVLOG(2) << "Error on getting resource entry for download";
    callback.Run(error, std::string(), 0, base::Time(), local_file->Pass());
    return;
  }
  DCHECK(entry);

  DVLOG(2) << "Got resource entry for download";
  // If local file and remote file are same, cancel the download.
  if (local_file_md5 == entry->file_md5()) {
    callback.Run(google_apis::HTTP_NOT_MODIFIED,
                 local_file_md5,
                 entry->file_size(),
                 entry->updated_time(),
                 local_file->Pass());
    return;
  }

  DVLOG(2) << "Downloading file: " << entry->resource_id();
  const std::string& resource_id = entry->resource_id();
  const base::FilePath& local_file_path = local_file->path();
  drive_service_->DownloadFile(local_file_path,
                               resource_id,
                               base::Bind(&APIUtil::DidDownloadFile,
                                          AsWeakPtr(),
                                          base::Passed(&entry),
                                          base::Passed(&local_file),
                                          callback),
                               google_apis::GetContentCallback(),
                               google_apis::ProgressCallback());
}

void APIUtil::DidDownloadFile(scoped_ptr<google_apis::ResourceEntry> entry,
                              scoped_ptr<webkit_blob::ScopedFile> local_file,
                              const DownloadFileCallback& callback,
                              google_apis::GDataErrorCode error,
                              const base::FilePath& downloaded_file_path) {
  DCHECK(CalledOnValidThread());
  if (error == google_apis::HTTP_SUCCESS)
    DVLOG(2) << "Download completed";
  else
    DVLOG(2) << "Error on downloading file: " << error;

  callback.Run(
      error, entry->file_md5(), entry->file_size(), entry->updated_time(),
      local_file->Pass());
}

void APIUtil::DidUploadNewFile(const std::string& parent_resource_id,
                               const std::string& title,
                               UploadKey upload_key,
                               google_apis::GDataErrorCode error,
                               scoped_ptr<google_apis::ResourceEntry> entry) {
  UploadFileCallback callback = GetAndUnregisterUploadCallback(upload_key);
  DCHECK(!callback.is_null());
  if (error != google_apis::HTTP_SUCCESS &&
      error != google_apis::HTTP_CREATED) {
    DVLOG(2) << "Error on uploading new file: " << error;
    callback.Run(error, std::string(), std::string());
    return;
  }

  DVLOG(2) << "Upload completed";
  EnsureTitleUniqueness(parent_resource_id,
                        title,
                        base::Bind(&APIUtil::DidEnsureUniquenessForCreateFile,
                                   AsWeakPtr(),
                                   entry->resource_id(),
                                   callback));
}

void APIUtil::DidEnsureUniquenessForCreateFile(
    const std::string& expected_resource_id,
    const UploadFileCallback& callback,
    google_apis::GDataErrorCode error,
    EnsureUniquenessStatus status,
    scoped_ptr<google_apis::ResourceEntry> entry) {
  if (error != google_apis::HTTP_SUCCESS) {
    DVLOG(2) << "Error on uploading new file: " << error;
    callback.Run(error, std::string(), std::string());
    return;
  }

  switch (status) {
    case NO_DUPLICATES_FOUND:
      // The file was uploaded successfully and no conflict was detected.
      DCHECK(entry);
      DVLOG(2) << "No conflict detected on uploading new file";
      callback.Run(
          google_apis::HTTP_CREATED, entry->resource_id(), entry->file_md5());
      return;

    case RESOLVED_DUPLICATES:
      // The file was uploaded successfully but a conflict was detected.
      // The duplicated file was deleted successfully.
      DCHECK(entry);
      if (entry->resource_id() != expected_resource_id) {
        // TODO(kinuko): We should check local vs remote md5 here.
        DVLOG(2) << "Conflict detected on uploading new file";
        callback.Run(google_apis::HTTP_CONFLICT,
                     entry->resource_id(),
                     entry->file_md5());
        return;
      }

      DVLOG(2) << "Conflict detected on uploading new file and resolved";
      callback.Run(
          google_apis::HTTP_CREATED, entry->resource_id(), entry->file_md5());
      return;

    default:
      NOTREACHED() << "Unknown status from EnsureTitleUniqueness:" << status
                   << " for " << expected_resource_id;
  }
}

void APIUtil::UploadExistingFileInternal(
    const std::string& remote_file_md5,
    const base::FilePath& local_file_path,
    const UploadFileCallback& callback,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::ResourceEntry> entry) {
  DCHECK(CalledOnValidThread());

  if (error != google_apis::HTTP_SUCCESS) {
    DVLOG(2) << "Error on uploading existing file: " << error;
    callback.Run(error, std::string(), std::string());
    return;
  }
  DCHECK(entry);

  // If remote file's hash value is different from the expected one, conflict
  // might have occurred.
  if (!remote_file_md5.empty() && remote_file_md5 != entry->file_md5()) {
    DVLOG(2) << "Conflict detected before uploading existing file";
    callback.Run(google_apis::HTTP_CONFLICT, std::string(), std::string());
    return;
  }

  drive::DriveUploader::UploadExistingFileOptions options;
  options.etag = entry->etag();
  std::string mime_type = GetMimeTypeFromTitle(entry->title());
  UploadKey upload_key = RegisterUploadCallback(callback);
  ResourceEntryCallback did_upload_callback =
      base::Bind(&APIUtil::DidUploadExistingFile, AsWeakPtr(), upload_key);
  drive_uploader_->UploadExistingFile(
      entry->resource_id(),
      local_file_path,
      mime_type,
      options,
      base::Bind(&UploadResultAdapter, did_upload_callback),
      google_apis::ProgressCallback());
}

bool APIUtil::IsAuthenticated() const {
  return drive_service_->HasRefreshToken();
}

void APIUtil::DidUploadExistingFile(
    UploadKey upload_key,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::ResourceEntry> entry) {
  DCHECK(CalledOnValidThread());
  UploadFileCallback callback = GetAndUnregisterUploadCallback(upload_key);
  DCHECK(!callback.is_null());
  if (error != google_apis::HTTP_SUCCESS) {
    DVLOG(2) << "Error on uploading existing file: " << error;
    callback.Run(error, std::string(), std::string());
    return;
  }

  DCHECK(entry);
  DVLOG(2) << "Upload completed";
  callback.Run(error, entry->resource_id(), entry->file_md5());
}

void APIUtil::DeleteFileInternal(const std::string& remote_file_md5,
                                 const GDataErrorCallback& callback,
                                 google_apis::GDataErrorCode error,
                                 scoped_ptr<google_apis::ResourceEntry> entry) {
  DCHECK(CalledOnValidThread());

  if (error != google_apis::HTTP_SUCCESS) {
    DVLOG(2) << "Error on getting resource entry for deleting file: " << error;
    callback.Run(error);
    return;
  }
  DCHECK(entry);

  // If remote file's hash value is different from the expected one, conflict
  // might have occurred.
  if (!remote_file_md5.empty() && remote_file_md5 != entry->file_md5()) {
    DVLOG(2) << "Conflict detected before deleting file";
    callback.Run(google_apis::HTTP_CONFLICT);
    return;
  }
  DVLOG(2) << "Got resource entry for deleting file";

  // Move the file to trash (don't delete it completely).
  drive_service_->TrashResource(
      entry->resource_id(),
      base::Bind(&APIUtil::DidDeleteFile, AsWeakPtr(), callback));
}

void APIUtil::DidDeleteFile(const GDataErrorCallback& callback,
                            google_apis::GDataErrorCode error) {
  DCHECK(CalledOnValidThread());
  if (error == google_apis::HTTP_SUCCESS)
    DVLOG(2) << "Deletion completed";
  else
    DVLOG(2) << "Error on deleting file: " << error;

  callback.Run(error);
}

void APIUtil::EnsureTitleUniqueness(const std::string& parent_resource_id,
                                    const std::string& expected_title,
                                    const EnsureUniquenessCallback& callback) {
  DCHECK(CalledOnValidThread());
  DVLOG(2) << "Checking if there's no conflict on entry creation";

  const google_apis::GetResourceListCallback& bound_callback =
      base::Bind(&APIUtil::DidListEntriesToEnsureUniqueness,
                 AsWeakPtr(),
                 parent_resource_id,
                 expected_title,
                 callback);

  SearchByTitle(expected_title, parent_resource_id, bound_callback);
}

void APIUtil::DidListEntriesToEnsureUniqueness(
    const std::string& parent_resource_id,
    const std::string& expected_title,
    const EnsureUniquenessCallback& callback,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::ResourceList> feed) {
  DCHECK(CalledOnValidThread());

  if (error != google_apis::HTTP_SUCCESS) {
    DVLOG(2) << "Error on listing resource for ensuring title uniqueness";
    callback.Run(
        error, NO_DUPLICATES_FOUND, scoped_ptr<google_apis::ResourceEntry>());
    return;
  }
  DVLOG(2) << "Got resource list for ensuring title uniqueness";

  // This filtering is needed only on WAPI. Once we move to Drive API we can
  // drop this.
  std::string resource_id;
  ParentType parent_type = PARENT_TYPE_DIRECTORY;
  if (parent_resource_id.empty()) {
    resource_id = GetRootResourceId();
    DCHECK(!resource_id.empty());
    parent_type = PARENT_TYPE_ROOT_OR_EMPTY;
  } else {
    resource_id = parent_resource_id;
  }
  ScopedVector<google_apis::ResourceEntry> entries;
  entries.swap(*feed->mutable_entries());
  FilterEntriesByTitleAndParent(
      &entries, expected_title, resource_id, parent_type);

  if (entries.empty()) {
    DVLOG(2) << "Uploaded file is not found";
    callback.Run(google_apis::HTTP_NOT_FOUND,
                 NO_DUPLICATES_FOUND,
                 scoped_ptr<google_apis::ResourceEntry>());
    return;
  }

  if (entries.size() >= 2) {
    DVLOG(2) << "Conflict detected on creating entry";
    for (size_t i = 0; i < entries.size() - 1; ++i) {
      // TODO(tzik): Replace published_time with creation time after we move to
      // Drive API.
      if (entries[i]->published_time() < entries.back()->published_time())
        std::swap(entries[i], entries.back());
    }

    scoped_ptr<google_apis::ResourceEntry> earliest_entry(entries.back());
    entries.back() = NULL;
    entries.get().pop_back();

    DeleteEntriesForEnsuringTitleUniqueness(
        entries.Pass(),
        base::Bind(&EntryAdapterForEnsureTitleUniqueness,
                   base::Passed(&earliest_entry),
                   callback,
                   RESOLVED_DUPLICATES));
    return;
  }

  DVLOG(2) << "no conflict detected";
  DCHECK_EQ(1u, entries.size());
  scoped_ptr<google_apis::ResourceEntry> entry(entries.front());
  entries.weak_clear();

  callback.Run(google_apis::HTTP_SUCCESS, NO_DUPLICATES_FOUND, entry.Pass());
}

void APIUtil::DeleteEntriesForEnsuringTitleUniqueness(
    ScopedVector<google_apis::ResourceEntry> entries,
    const GDataErrorCallback& callback) {
  DCHECK(CalledOnValidThread());
  DVLOG(2) << "Cleaning up conflict on entry creation";

  if (entries.empty()) {
    callback.Run(google_apis::HTTP_SUCCESS);
    return;
  }

  scoped_ptr<google_apis::ResourceEntry> entry(entries.back());
  entries.back() = NULL;
  entries.get().pop_back();

  // We don't care conflicts here as other clients may be also deleting this
  // file, so passing an empty etag.
  drive_service_->TrashResource(
      entry->resource_id(),
      base::Bind(&APIUtil::DidDeleteEntriesForEnsuringTitleUniqueness,
                 AsWeakPtr(),
                 base::Passed(&entries),
                 callback));
}

void APIUtil::DidDeleteEntriesForEnsuringTitleUniqueness(
    ScopedVector<google_apis::ResourceEntry> entries,
    const GDataErrorCallback& callback,
    google_apis::GDataErrorCode error) {
  DCHECK(CalledOnValidThread());

  if (error != google_apis::HTTP_SUCCESS &&
      error != google_apis::HTTP_NOT_FOUND) {
    DVLOG(2) << "Error on deleting file: " << error;
    callback.Run(error);
    return;
  }

  DVLOG(2) << "Deletion completed";
  DeleteEntriesForEnsuringTitleUniqueness(entries.Pass(), callback);
}

APIUtil::UploadKey APIUtil::RegisterUploadCallback(
    const UploadFileCallback& callback) {
  const bool inserted = upload_callback_map_.insert(
      std::make_pair(upload_next_key_, callback)).second;
  CHECK(inserted);
  return upload_next_key_++;
}

APIUtil::UploadFileCallback APIUtil::GetAndUnregisterUploadCallback(
    UploadKey key) {
  UploadFileCallback callback;
  UploadCallbackMap::iterator found = upload_callback_map_.find(key);
  if (found == upload_callback_map_.end())
    return callback;
  callback = found->second;
  upload_callback_map_.erase(found);
  return callback;
}

void APIUtil::CancelAllUploads(google_apis::GDataErrorCode error) {
  if (upload_callback_map_.empty())
    return;
  for (UploadCallbackMap::iterator iter = upload_callback_map_.begin();
       iter != upload_callback_map_.end();
       ++iter) {
    iter->second.Run(error, std::string(), std::string());
  }
  upload_callback_map_.clear();
  drive_uploader_.reset(new drive::DriveUploader(
      drive_service_.get(), content::BrowserThread::GetBlockingPool()));
}

std::string APIUtil::GetRootResourceId() const {
  if (IsDriveAPIDisabled())
    return drive_service_->GetRootResourceId();
  return root_resource_id_;
}

}  // namespace drive_backend
}  // namespace sync_file_system
