// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/drive_sync_client.h"

#include <algorithm>
#include <vector>

#include "base/bind.h"
#include "base/message_loop_proxy.h"
#include "chrome/browser/api/prefs/pref_change_registrar.h"
#include "chrome/browser/chromeos/gdata/drive.pb.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace gdata {

namespace {

// The delay constant is used to delay processing a SyncTask in
// DoSyncLoop(). We should not process SyncTasks immediately for the
// following reasons:
//
// 1) For fetching, the user may accidentally click on "Make available
//    offline" checkbox on a file, and immediately cancel it in a second.
//    It's a waste to fetch the file in this case.
//
// 2) For uploading, file writing via HTML5 file system API is performed in
//    two steps: 1) truncate a file to 0 bytes, 2) write contents. We
//    shouldn't start uploading right after the step 1). Besides, the user
//    may edit the same file repeatedly in a short period of time.
//
// TODO(satorux): We should find a way to handle the upload case more nicely,
// and shorten the delay. crbug.com/134774
const int kDelaySeconds = 5;

// Functor for std::find_if() search for a sync task that matches
// |in_sync_type| and |in_resource_id|.
struct CompareTypeAndResourceId {
  CompareTypeAndResourceId(const DriveSyncClient::SyncType& in_sync_type,
                           const std::string& in_resource_id)
      : sync_type(in_sync_type),
        resource_id(in_resource_id) {}

  bool operator()(const DriveSyncClient::SyncTask& sync_task) {
    return (sync_type == sync_task.sync_type &&
            resource_id == sync_task.resource_id);
  }

  const DriveSyncClient::SyncType sync_type;
  const std::string resource_id;
};

// Returns true if the current network connection is over cellular.
bool IsConnectionTypeCellular() {
  bool is_cellular = false;
  // Use switch, not if, to allow compiler to catch future enum changes.
  // (e.g. Addition of CONNECTION_5G)
  switch (net::NetworkChangeNotifier::GetConnectionType()) {
    case net::NetworkChangeNotifier::CONNECTION_2G:
    case net::NetworkChangeNotifier::CONNECTION_3G:
    case net::NetworkChangeNotifier::CONNECTION_4G:
      is_cellular = true;
      break;
    case net::NetworkChangeNotifier::CONNECTION_UNKNOWN:
    case net::NetworkChangeNotifier::CONNECTION_ETHERNET:
    case net::NetworkChangeNotifier::CONNECTION_WIFI:
    case net::NetworkChangeNotifier::CONNECTION_NONE:
      is_cellular = false;
      break;
  }
  return is_cellular;
}

}  // namespace

DriveSyncClient::SyncTask::SyncTask(SyncType in_sync_type,
                                    const std::string& in_resource_id,
                                    const base::Time& in_timestamp)
    : sync_type(in_sync_type),
      resource_id(in_resource_id),
      timestamp(in_timestamp) {
}

DriveSyncClient::DriveSyncClient(Profile* profile,
                                 DriveFileSystemInterface* file_system,
                                 DriveCache* cache)
    : profile_(profile),
      file_system_(file_system),
      cache_(cache),
      registrar_(new PrefChangeRegistrar),
      delay_(base::TimeDelta::FromSeconds(kDelaySeconds)),
      sync_loop_is_running_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

DriveSyncClient::~DriveSyncClient() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (file_system_)
    file_system_->RemoveObserver(this);
  if (cache_)
    cache_->RemoveObserver(this);
  net::NetworkChangeNotifier::RemoveConnectionTypeObserver(this);
}

void DriveSyncClient::Initialize() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  file_system_->AddObserver(this);
  cache_->AddObserver(this);

  net::NetworkChangeNotifier::AddConnectionTypeObserver(this);

  registrar_->Init(profile_->GetPrefs());
  registrar_->Add(prefs::kDisableGData, this);
  registrar_->Add(prefs::kDisableGDataOverCellular, this);
}

void DriveSyncClient::StartProcessingBacklog() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  cache_->GetResourceIdsOfBacklogOnUIThread(
      base::Bind(&DriveSyncClient::OnGetResourceIdsOfBacklog,
                 weak_ptr_factory_.GetWeakPtr()));
}

void DriveSyncClient::StartCheckingExistingPinnedFiles() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  cache_->GetResourceIdsOfExistingPinnedFilesOnUIThread(
      base::Bind(&DriveSyncClient::OnGetResourceIdsOfExistingPinnedFiles,
                 weak_ptr_factory_.GetWeakPtr()));
}

std::vector<std::string> DriveSyncClient::GetResourceIdsForTesting(
    SyncType sync_type) const {
  std::vector<std::string> resource_ids;
  for (size_t i = 0; i < queue_.size(); ++i) {
    const SyncTask& sync_task = queue_[i];
    if (sync_task.sync_type == sync_type)
      resource_ids.push_back(sync_task.resource_id);
  }
  return resource_ids;
}

void DriveSyncClient::StartSyncLoop() {
  if (!sync_loop_is_running_)
    DoSyncLoop();
}

void DriveSyncClient::DoSyncLoop() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (queue_.empty() || ShouldStopSyncLoop()) {
    // Note that |queue_| is not cleared so the sync loop can resume.
    sync_loop_is_running_ = false;
    return;
  }
  sync_loop_is_running_ = true;

  // Should copy before calling queue_.pop_front().
  const SyncTask sync_task = queue_.front();

  // Check if we are ready to process the task.
  const base::TimeDelta elapsed = base::Time::Now() - sync_task.timestamp;
  if (elapsed < delay_) {
    // Not yet ready. Revisit at a later time.
    const bool posted = base::MessageLoopProxy::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&DriveSyncClient::DoSyncLoop,
                   weak_ptr_factory_.GetWeakPtr()),
        delay_);
    DCHECK(posted);
    return;
  }

  queue_.pop_front();
  if (sync_task.sync_type == FETCH) {
    DVLOG(1) << "Fetching " << sync_task.resource_id;
    file_system_->GetFileByResourceId(
        sync_task.resource_id,
        base::Bind(&DriveSyncClient::OnFetchFileComplete,
                   weak_ptr_factory_.GetWeakPtr(),
                   sync_task),
        GetContentCallback());
  } else if (sync_task.sync_type == UPLOAD) {
    DVLOG(1) << "Uploading " << sync_task.resource_id;
    file_system_->UpdateFileByResourceId(
        sync_task.resource_id,
        base::Bind(&DriveSyncClient::OnUploadFileComplete,
                   weak_ptr_factory_.GetWeakPtr(),
                   sync_task.resource_id));
  } else {
    NOTREACHED() << ": Unexpected sync type: " << sync_task.sync_type;
  }
}

bool DriveSyncClient::ShouldStopSyncLoop() {
  // Should stop if the drive feature was disabled while running the fetch
  // loop.
  if (profile_->GetPrefs()->GetBoolean(prefs::kDisableGData))
    return true;

  // Should stop if the network is not online.
  if (net::NetworkChangeNotifier::IsOffline())
    return true;

  // Should stop if the current connection is on cellular network, and
  // fetching is disabled over cellular.
  if (profile_->GetPrefs()->GetBoolean(prefs::kDisableGDataOverCellular) &&
      IsConnectionTypeCellular())
    return true;

  return false;
}

void DriveSyncClient::OnInitialLoadFinished() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  StartProcessingBacklog();
}

void DriveSyncClient::OnFeedFromServerLoaded() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  StartCheckingExistingPinnedFiles();
}

void DriveSyncClient::OnCachePinned(const std::string& resource_id,
                                    const std::string& md5) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  AddTaskToQueue(SyncTask(FETCH, resource_id, base::Time::Now()));
  StartSyncLoop();
}

void DriveSyncClient::OnCacheUnpinned(const std::string& resource_id,
                                      const std::string& md5) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Remove the resource_id if it's in the queue. This can happen if the user
  // cancels pinning before the file is fetched.
  std::deque<SyncTask>::iterator iter =
      std::find_if(queue_.begin(), queue_.end(),
                   CompareTypeAndResourceId(FETCH, resource_id));
  if (iter != queue_.end())
    queue_.erase(iter);
}

void DriveSyncClient::OnCacheCommitted(const std::string& resource_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  AddTaskToQueue(SyncTask(UPLOAD, resource_id, base::Time::Now()));
  StartSyncLoop();
}

void DriveSyncClient::AddTaskToQueue(const SyncTask& sync_task) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  std::deque<SyncTask>::iterator iter =
      std::find_if(queue_.begin(), queue_.end(),
                   CompareTypeAndResourceId(sync_task.sync_type,
                                            sync_task.resource_id));
  // If the same task is already queued, remove it. We'll add back the new
  // task to the end of the queue.
  if (iter != queue_.end())
    queue_.erase(iter);

  queue_.push_back(sync_task);
}

void DriveSyncClient::OnGetResourceIdsOfBacklog(
    const std::vector<std::string>& to_fetch,
    const std::vector<std::string>& to_upload) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Give priority to upload tasks over fetch tasks, so that dirty files are
  // uploaded as soon as possible.
  for (size_t i = 0; i < to_upload.size(); ++i) {
    const std::string& resource_id = to_upload[i];
    DVLOG(1) << "Queuing to upload: " << resource_id;
    AddTaskToQueue(SyncTask(UPLOAD, resource_id, base::Time::Now()));
  }

  for (size_t i = 0; i < to_fetch.size(); ++i) {
    const std::string& resource_id = to_fetch[i];
    DVLOG(1) << "Queuing to fetch: " << resource_id;
    AddTaskToQueue(SyncTask(FETCH, resource_id, base::Time::Now()));
  }

  StartSyncLoop();
}

void DriveSyncClient::OnGetResourceIdsOfExistingPinnedFiles(
    const std::vector<std::string>& resource_ids) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  for (size_t i = 0; i < resource_ids.size(); ++i) {
    const std::string& resource_id = resource_ids[i];
    file_system_->GetEntryInfoByResourceId(
        resource_id,
        base::Bind(&DriveSyncClient::OnGetEntryInfoByResourceId,
                   weak_ptr_factory_.GetWeakPtr(),
                   resource_id));
  }
}

void DriveSyncClient::OnGetEntryInfoByResourceId(
    const std::string& resource_id,
    DriveFileError error,
    const FilePath& /* drive_file_path */,
    scoped_ptr<DriveEntryProto> entry_proto) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (entry_proto.get() && !entry_proto->has_file_specific_info())
    error = DRIVE_FILE_ERROR_NOT_FOUND;

  if (error != DRIVE_FILE_OK) {
    LOG(WARNING) << "Entry not found: " << resource_id;
    return;
  }

  cache_->GetCacheEntryOnUIThread(
      resource_id,
      "" /* don't check MD5 */,
      base::Bind(&DriveSyncClient::OnGetCacheEntry,
                 weak_ptr_factory_.GetWeakPtr(),
                 resource_id,
                 entry_proto->file_specific_info().file_md5()));
}

void DriveSyncClient::OnGetCacheEntry(
    const std::string& resource_id,
    const std::string& latest_md5,
    bool success,
    const DriveCacheEntry& cache_entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!success) {
    LOG(WARNING) << "Cache entry not found: " << resource_id;
    return;
  }

  // If MD5s don't match, it indicates the local cache file is stale, unless
  // the file is dirty (the MD5 is "local"). We should never re-fetch the
  // file when we have a locally modified version.
  if (latest_md5 != cache_entry.md5() && !cache_entry.is_dirty()) {
    cache_->RemoveOnUIThread(
        resource_id,
        base::Bind(&DriveSyncClient::OnRemove,
                   weak_ptr_factory_.GetWeakPtr()));
  }
}

void DriveSyncClient::OnRemove(DriveFileError error,
                               const std::string& resource_id,
                               const std::string& md5) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (error != DRIVE_FILE_OK) {
    LOG(WARNING) << "Failed to remove cache entry: " << resource_id;
    return;
  }

  // Before fetching, we should pin this file again, so that the fetched file
  // is downloaded properly to the persistent directory and marked pinned.
  cache_->PinOnUIThread(resource_id,
                        md5,
                        base::Bind(&DriveSyncClient::OnPinned,
                                   weak_ptr_factory_.GetWeakPtr()));
}

void DriveSyncClient::OnPinned(DriveFileError error,
                               const std::string& resource_id,
                               const std::string& /* md5 */) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (error != DRIVE_FILE_OK) {
    LOG(WARNING) << "Failed to pin cache entry: " << resource_id;
    return;
  }

  // Finally, adding to the queue.
  AddTaskToQueue(SyncTask(FETCH, resource_id, base::Time::Now()));
  StartSyncLoop();
}

void DriveSyncClient::OnFetchFileComplete(const SyncTask& sync_task,
                                          DriveFileError error,
                                          const FilePath& local_path,
                                          const std::string& ununsed_mime_type,
                                          DriveFileType file_type) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (error == DRIVE_FILE_OK) {
    DVLOG(1) << "Fetched " << sync_task.resource_id << ": "
             << local_path.value();
  } else {
    switch (error) {
      case DRIVE_FILE_ERROR_NO_CONNECTION:
        // Re-queue the task so that we'll retry once the connection is back.
        queue_.push_front(sync_task);
        break;
      default:
        LOG(WARNING) << "Failed to fetch " << sync_task.resource_id
                     << ": " << error;
    }
  }

  // Continue the loop.
  DoSyncLoop();
}

void DriveSyncClient::OnUploadFileComplete(const std::string& resource_id,
                                           DriveFileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (error == DRIVE_FILE_OK) {
    DVLOG(1) << "Uploaded " << resource_id;
  } else {
    // TODO(satorux): We should re-queue if the error is recoverable.
    LOG(WARNING) << "Failed to upload " << resource_id << ": " << error;
  }

  // Continue the loop.
  DoSyncLoop();
}

void DriveSyncClient::Observe(int type,
                              const content::NotificationSource& source,
                              const content::NotificationDetails& details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Resume the sync loop if gdata preferences are changed. Note that we
  // don't need to check the new values here as these will be checked in
  // ShouldStopSyncLoop() as soon as the loop is resumed.
  StartSyncLoop();
}

void DriveSyncClient::OnConnectionTypeChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Resume the sync loop if the network is back online. Note that we don't
  // need to check the type of the network as it will be checked in
  // ShouldStopSyncLoop() as soon as the loop is resumed.
  if (!net::NetworkChangeNotifier::IsOffline())
    StartSyncLoop();
}

}  // namespace gdata
