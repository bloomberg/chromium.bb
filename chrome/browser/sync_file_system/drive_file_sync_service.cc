// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_file_sync_service.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/google_apis/gdata_wapi_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync_file_system/drive_file_sync_client.h"
#include "chrome/browser/sync_file_system/drive_file_sync_util.h"
#include "chrome/browser/sync_file_system/drive_metadata_store.h"
#include "content/public/browser/browser_thread.h"
#include "webkit/fileapi/file_system_url.h"
#include "webkit/fileapi/syncable/sync_file_metadata.h"
#include "webkit/fileapi/syncable/sync_file_type.h"
#include "webkit/fileapi/syncable/syncable_file_system_util.h"

namespace sync_file_system {

const char DriveFileSyncService::kServiceName[] = "drive";

class DriveFileSyncService::TaskToken {
 public:
  explicit TaskToken(const base::WeakPtr<DriveFileSyncService>& sync_service)
      : sync_service_(sync_service) {
  }

  void set_location(const tracked_objects::Location& from_here) {
    from_here_ = from_here;
  }

  ~TaskToken() {
    // All task on DriveFileSyncService must hold TaskToken instance to ensure
    // no other tasks are running. Also, as soon as a task finishes to work,
    // it must return the token to DriveFileSyncService.
    // Destroying a token with valid |sync_service_| indicates the token was
    // dropped by a task without returning.
    DCHECK(!sync_service_);
    if (sync_service_) {
      LOG(ERROR) << "Unexpected TaskToken deletion from: "
                 << from_here_.ToString();
    }
  }

 private:
  tracked_objects::Location from_here_;
  base::WeakPtr<DriveFileSyncService> sync_service_;

  DISALLOW_COPY_AND_ASSIGN(TaskToken);
};

DriveFileSyncService::DriveFileSyncService(Profile* profile)
    : status_(fileapi::SYNC_STATUS_OK),
      largest_changestamp_(0),
      weak_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  token_.reset(new TaskToken(weak_factory_.GetWeakPtr()));

  sync_client_.reset(new DriveFileSyncClient(profile));

  metadata_store_.reset(new DriveMetadataStore(
      profile->GetPath(),
      content::BrowserThread::GetMessageLoopProxyForThread(
          content::BrowserThread::FILE)));

  metadata_store_->Initialize(
      base::Bind(&DriveFileSyncService::DidInitializeMetadataStore,
                 weak_factory_.GetWeakPtr(),
                 base::Passed(GetToken(FROM_HERE))));
}

// static
scoped_ptr<DriveFileSyncService> DriveFileSyncService::CreateForTesting(
    scoped_ptr<DriveFileSyncClient> sync_client,
    scoped_ptr<DriveMetadataStore> metadata_store) {
  return make_scoped_ptr(new DriveFileSyncService(
      sync_client.Pass(), metadata_store.Pass()));
}

// Called by CreateForTesting.
DriveFileSyncService::DriveFileSyncService(
    scoped_ptr<DriveFileSyncClient> sync_client,
    scoped_ptr<DriveMetadataStore> metadata_store)
    : status_(fileapi::SYNC_STATUS_OK),
      largest_changestamp_(0),
      weak_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  token_.reset(new TaskToken(weak_factory_.GetWeakPtr()));
  sync_client_ = sync_client.Pass();
  metadata_store_ = metadata_store.Pass();

  DidInitializeMetadataStore(GetToken(FROM_HERE),
                             fileapi::SYNC_STATUS_OK, false);
}

DriveFileSyncService::~DriveFileSyncService() {
}

void DriveFileSyncService::DidInitializeMetadataStore(
    scoped_ptr<TaskToken> token,
    fileapi::SyncStatusCode status,
    bool created) {
  if (status != fileapi::SYNC_STATUS_OK) {
    NotifyTaskDone(status, token.Pass());
    return;
  }

  if (metadata_store_->sync_root_directory().empty()) {
    DCHECK(metadata_store_->batch_sync_origins().empty());
    DCHECK(metadata_store_->incremental_sync_origins().empty());

    sync_client_->GetDriveDirectoryForSyncRoot(
        base::Bind(&DriveFileSyncService::DidGetSyncRootDirectory,
                   weak_factory_.GetWeakPtr(),
                   base::Passed(&token)));
    return;
  }
  NotifyTaskDone(status, token.Pass());

  for (std::map<GURL, std::string>::const_iterator itr =
           metadata_store_->batch_sync_origins().begin();
       itr != metadata_store_->batch_sync_origins().end();
       ++itr) {
    StartBatchSyncForOrigin(itr->first, itr->second);
  }
}

void DriveFileSyncService::DidGetSyncRootDirectory(
    scoped_ptr<TaskToken> token,
    google_apis::GDataErrorCode error,
    const std::string& resource_id) {
  if (error != google_apis::HTTP_SUCCESS &&
      error != google_apis::HTTP_CREATED) {
    NotifyTaskDone(GDataErrorCodeToSyncStatusCode(error), token.Pass());
    return;
  }

  metadata_store_->SetSyncRootDirectory(resource_id);
  NotifyTaskDone(fileapi::SYNC_STATUS_OK, token.Pass());
}

scoped_ptr<DriveFileSyncService::TaskToken> DriveFileSyncService::GetToken(
    const tracked_objects::Location& from_here) {
  if (!token_)
    return scoped_ptr<TaskToken>();
  token_->set_location(from_here);
  return token_.Pass();
}

void DriveFileSyncService::NotifyTaskDone(fileapi::SyncStatusCode status,
                                          scoped_ptr<TaskToken> token) {
  DCHECK(token);
  bool status_changed = status_ != status;
  status_ = status;
  token_ = token.Pass();
  token_->set_location(tracked_objects::Location());

  if (status == fileapi::SYNC_STATUS_OK && !pending_tasks_.empty()) {
    base::Closure closure = pending_tasks_.front();
    pending_tasks_.pop_front();
    closure.Run();
  }

  if (status_changed) {
    // TODO(tzik): Refine the status to track the error.
    FOR_EACH_OBSERVER(Observer, observers_, OnRemoteSyncStatusChanged(status));
  }
}

void DriveFileSyncService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void DriveFileSyncService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void DriveFileSyncService::RegisterOriginForTrackingChanges(
    const GURL& origin,
    const fileapi::SyncStatusCallback& callback) {
  scoped_ptr<TaskToken> token(GetToken(FROM_HERE));
  if (!token) {
    pending_tasks_.push_back(base::Bind(
        &DriveFileSyncService::RegisterOriginForTrackingChanges,
        weak_factory_.GetWeakPtr(), origin, callback));
    return;
  }

  if (status_ != fileapi::SYNC_STATUS_OK) {
    NotifyTaskDone(status_, token.Pass());
    callback.Run(status_);
    return;
  }

  if (metadata_store_->IsIncrementalSyncOrigin(origin) ||
      metadata_store_->IsBatchSyncOrigin(origin)) {
    NotifyTaskDone(fileapi::SYNC_STATUS_OK, token.Pass());
    callback.Run(fileapi::SYNC_STATUS_OK);
    return;
  }

  DCHECK(!metadata_store_->sync_root_directory().empty());
  sync_client_->GetDriveDirectoryForOrigin(
      metadata_store_->sync_root_directory(), origin,
      base::Bind(&DriveFileSyncService::DidGetDirectoryForOrigin,
                 weak_factory_.GetWeakPtr(), base::Passed(&token),
                 origin, callback));
}

void DriveFileSyncService::DidGetDirectoryForOrigin(
    scoped_ptr<TaskToken> token,
    const GURL& origin,
    const fileapi::SyncStatusCallback& callback,
    google_apis::GDataErrorCode error,
    const std::string& resource_id) {
  if (error != google_apis::HTTP_SUCCESS &&
      error != google_apis::HTTP_CREATED) {
    fileapi::SyncStatusCode status = GDataErrorCodeToSyncStatusCode(error);
    NotifyTaskDone(status, token.Pass());
    callback.Run(status);
    return;
  }

  metadata_store_->AddBatchSyncOrigin(origin, resource_id);

  NotifyTaskDone(fileapi::SYNC_STATUS_OK, token.Pass());
  callback.Run(fileapi::SYNC_STATUS_OK);

  StartBatchSyncForOrigin(origin, resource_id);
}

void DriveFileSyncService::UnregisterOriginForTrackingChanges(
    const GURL& origin,
    const fileapi::SyncStatusCallback& callback) {
  scoped_ptr<TaskToken> token(GetToken(FROM_HERE));
  if (!token) {
    pending_tasks_.push_back(base::Bind(
        &DriveFileSyncService::UnregisterOriginForTrackingChanges,
        weak_factory_.GetWeakPtr(), origin, callback));
    return;
  }

  changestamp_map_.erase(origin);

  std::vector<RemoteChange> new_queue;
  while (!pending_changes_.empty()) {
    if (pending_changes_.top().url.origin() != origin)
      new_queue.push_back(pending_changes_.top());
    pending_changes_.pop();
  }
  pending_changes_ = ChangeQueue(new_queue.begin(), new_queue.end());

  metadata_store_->RemoveOrigin(origin, base::Bind(
      &DriveFileSyncService::DidRemoveOriginOnMetadataStore,
      weak_factory_.GetWeakPtr(), base::Passed(&token), callback));
}

void DriveFileSyncService::DidRemoveOriginOnMetadataStore(
    scoped_ptr<TaskToken> token,
    const fileapi::SyncStatusCallback& callback,
    fileapi::SyncStatusCode status) {
  NotifyTaskDone(status, token.Pass());
  callback.Run(status);
}

LocalChangeProcessor* DriveFileSyncService::GetLocalChangeProcessor() {
  return this;
}

void DriveFileSyncService::ProcessRemoteChange(
    RemoteChangeProcessor* processor,
    const fileapi::SyncFileCallback& callback) {
  NOTIMPLEMENTED();
  callback.Run(fileapi::SYNC_STATUS_FAILED,
               fileapi::FileSystemURL());
}

void DriveFileSyncService::GetConflictFiles(
    const GURL& origin,
    const fileapi::SyncFileSetCallback& callback) {
  NOTIMPLEMENTED();
  callback.Run(fileapi::SYNC_STATUS_FAILED,
               fileapi::FileSystemURLSet());
}

void DriveFileSyncService::GetRemoteFileMetadata(
    const fileapi::FileSystemURL& url,
    const fileapi::SyncFileMetadataCallback& callback) {
  NOTIMPLEMENTED();
  callback.Run(fileapi::SYNC_STATUS_FAILED,
               fileapi::SyncFileMetadata());
}

void DriveFileSyncService::ApplyLocalChange(
    const fileapi::FileChange& change,
    const FilePath& local_path,
    const fileapi::FileSystemURL& url,
    const fileapi::SyncStatusCallback& callback) {
  NOTIMPLEMENTED();
  callback.Run(fileapi::SYNC_STATUS_FAILED);
}

void DriveFileSyncService::StartBatchSyncForOrigin(
    const GURL& origin,
    const std::string& resource_id) {
  scoped_ptr<TaskToken> token(GetToken(FROM_HERE));
  if (!token) {
    pending_tasks_.push_back(base::Bind(
        &DriveFileSyncService::StartBatchSyncForOrigin,
        weak_factory_.GetWeakPtr(), origin, resource_id));
    return;
  }

  sync_client_->GetLargestChangeStamp(
      base::Bind(&DriveFileSyncService::DidGetLargestChangeStampForBatchSync,
                 weak_factory_.GetWeakPtr(),
                 base::Passed(&token), origin, resource_id));
}

void DriveFileSyncService::DidGetLargestChangeStampForBatchSync(
    scoped_ptr<TaskToken> token,
    const GURL& origin,
    const std::string& resource_id,
    google_apis::GDataErrorCode error,
    int64 largest_changestamp) {
  if (error != google_apis::HTTP_SUCCESS) {
    // TODO(tzik): Refine this error code.
    NotifyTaskDone(GDataErrorCodeToSyncStatusCode(error), token.Pass());
    return;
  }

  sync_client_->ListFiles(
      resource_id,
      base::Bind(&DriveFileSyncService::DidGetDirectoryContentForBatchSync,
                 weak_factory_.GetWeakPtr(),
                 base::Passed(&token), origin, largest_changestamp));
}

void DriveFileSyncService::DidGetDirectoryContentForBatchSync(
    scoped_ptr<TaskToken> token,
    const GURL& origin,
    int64 largest_changestamp,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::DocumentFeed> feed) {
  if (error != google_apis::HTTP_SUCCESS) {
    // TODO(tzik): Refine this error code.
    NotifyTaskDone(GDataErrorCodeToSyncStatusCode(error), token.Pass());
    return;
  }

  typedef ScopedVector<google_apis::DocumentEntry>::const_iterator iterator;
  for (iterator itr = feed->entries().begin();
       itr != feed->entries().end(); ++itr) {
    AppendNewRemoteChange(origin, *itr, largest_changestamp,
                          true /* batch_sync */);
  }

  GURL next_feed_url;
  if (feed->GetNextFeedURL(&next_feed_url)) {
    sync_client_->ContinueListing(
        next_feed_url,
        base::Bind(&DriveFileSyncService::DidGetDirectoryContentForBatchSync,
                   weak_factory_.GetWeakPtr(), base::Passed(&token),
                   origin, largest_changestamp));
    return;
  }
  NotifyTaskDone(fileapi::SYNC_STATUS_OK, token.Pass());
}

void DriveFileSyncService::AppendNewRemoteChange(
    const GURL& origin,
    google_apis::DocumentEntry* entry,
    int64 changestamp,
    bool is_batch_sync) {
  FilePath path = FilePath::FromUTF8Unsafe(UTF16ToUTF8(entry->title()));
  fileapi::FileSystemURL url(
      fileapi::CreateSyncableFileSystemURL(origin, kServiceName, path));

  fileapi::FileChange::ChangeType change_type;
  fileapi::SyncFileType file_type;
  if (entry->deleted()) {
    change_type = fileapi::FileChange::FILE_CHANGE_DELETE;
    file_type = fileapi::SYNC_FILE_TYPE_UNKNOWN;
  } else {
    change_type = fileapi::FileChange::FILE_CHANGE_ADD_OR_UPDATE;
    if (entry->kind() == google_apis::ENTRY_KIND_FOLDER)
      file_type = fileapi::SYNC_FILE_TYPE_DIRECTORY;
    else
      file_type = fileapi::SYNC_FILE_TYPE_FILE;
  }

  fileapi::FileChange file_change(change_type, file_type);
  RemoteChange change(changestamp, url, file_change);

  std::pair<PathToChangeStamp::iterator, bool> inserted =
      changestamp_map_[url.origin()].insert(
          std::make_pair(url.path(), changestamp));
  if (!inserted.second) {
    int64 another_changestamp = inserted.first->second;
    if (another_changestamp > changestamp)
      return;
    inserted.first->second = changestamp;
  }

  pending_changes_.push(change);
}

DriveFileSyncService::RemoteChange::RemoteChange()
    : changestamp(0),
      change(fileapi::FileChange::FILE_CHANGE_ADD_OR_UPDATE,
             fileapi::SYNC_FILE_TYPE_UNKNOWN) {
}

DriveFileSyncService::RemoteChange::RemoteChange(
    int64 changestamp,
    const fileapi::FileSystemURL& url,
    const fileapi::FileChange& change)
    : changestamp(changestamp),
      url(url),
      change(change) {
}

DriveFileSyncService::RemoteChange::~RemoteChange() {
}

bool DriveFileSyncService::RemoteChangeComparator::operator()(
    const RemoteChange& left,
    const RemoteChange& right) {
  // This should return true if |right| has higher priority than |left|.
  // Smaller changestamps have higher priorities (i.e. need to be processed
  // earlier).
  if (left.changestamp != right.changestamp)
    return left.changestamp > right.changestamp;
  return false;
}

}  // namespace sync_file_system
