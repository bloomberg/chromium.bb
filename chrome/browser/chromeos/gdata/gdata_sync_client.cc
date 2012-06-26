// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/gdata_sync_client.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <vector>

#include "base/bind.h"
#include "base/file_util.h"
#include "base/message_loop_proxy.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/gdata/gdata_file_system.h"
#include "chrome/browser/prefs/pref_change_registrar.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace gdata {

GDataSyncClient::SyncTask::SyncTask(SyncType in_sync_type,
                                    const std::string& in_resource_id)
    : sync_type(in_sync_type),
      resource_id(in_resource_id) {
}

GDataSyncClient::GDataSyncClient(Profile* profile,
                                 GDataFileSystemInterface* file_system,
                                 GDataCache* cache)
    : profile_(profile),
      file_system_(file_system),
      cache_(cache),
      registrar_(new PrefChangeRegistrar),
      sync_loop_is_running_(false),
      weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

GDataSyncClient::~GDataSyncClient() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (cache_)
    cache_->RemoveObserver(this);

  chromeos::NetworkLibrary* network_library =
      chromeos::CrosLibrary::Get()->GetNetworkLibrary();
  if (network_library)
    network_library->RemoveNetworkManagerObserver(this);
}

void GDataSyncClient::Initialize() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  file_system_->AddObserver(this);
  cache_->AddObserver(this);

  chromeos::NetworkLibrary* network_library =
      chromeos::CrosLibrary::Get()->GetNetworkLibrary();
  if (network_library)
    network_library->AddNetworkManagerObserver(this);
  else
    LOG(ERROR) << "NetworkLibrary is not present";

  registrar_->Init(profile_->GetPrefs());
  registrar_->Add(prefs::kDisableGData, this);
  registrar_->Add(prefs::kDisableGDataOverCellular, this);
}

void GDataSyncClient::StartProcessingPinnedButNotFetchedFiles() {
  cache_->GetResourceIdsOfPinnedButNotFetchedFilesOnUIThread(
      base::Bind(&GDataSyncClient::OnGetResourceIdsOfPinnedButNotFetchedFiles,
                 weak_ptr_factory_.GetWeakPtr()));
}

std::vector<std::string> GDataSyncClient::GetResourceIdsForTesting(
    SyncType sync_type) const {
  std::vector<std::string> resource_ids;
  for (size_t i = 0; i < queue_.size(); ++i) {
    const SyncTask& sync_task = queue_[i];
    if (sync_task.sync_type == sync_type)
      resource_ids.push_back(sync_task.resource_id);
  }
  return resource_ids;
}

void GDataSyncClient::StartSyncLoop() {
  if (!sync_loop_is_running_)
    DoSyncLoop();
}

void GDataSyncClient::DoSyncLoop() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (queue_.empty() || ShouldStopSyncLoop()) {
    // Note that |queue_| is not cleared so the sync loop can resume.
    sync_loop_is_running_ = false;
    return;
  }
  sync_loop_is_running_ = true;

  const SyncTask& sync_task = queue_.front();
  DCHECK_EQ(FETCH, sync_task.sync_type);
  const std::string resource_id = sync_task.resource_id;
  queue_.pop_front();

  DVLOG(1) << "Fetching " << resource_id;
  file_system_->GetFileByResourceId(
      resource_id,
      base::Bind(&GDataSyncClient::OnFetchFileComplete,
                 weak_ptr_factory_.GetWeakPtr(),
                 resource_id),
      GetDownloadDataCallback());
}

bool GDataSyncClient::ShouldStopSyncLoop() {
  // Should stop if the gdata feature was disabled while running the fetch
  // loop.
  if (profile_->GetPrefs()->GetBoolean(prefs::kDisableGData))
    return true;

  // Something must be wrong if the network library is not present.
  chromeos::NetworkLibrary* network_library =
      chromeos::CrosLibrary::Get()->GetNetworkLibrary();
  if (!network_library)
    return true;

  // Something must be wrong if the active network is not present.
  const chromeos::Network* active_network = network_library->active_network();
  if (!active_network)
    return true;

  // Should stop if the network is not online.
  if (!active_network->online())
    return true;

  // Should stop if the current connection is on cellular network, and
  // fetching is disabled over cellular.
  if (profile_->GetPrefs()->GetBoolean(prefs::kDisableGDataOverCellular) &&
      (active_network->type() == chromeos::TYPE_CELLULAR ||
       active_network->type() == chromeos::TYPE_WIMAX)) {
    return true;
  }

  return false;
}

void GDataSyncClient::OnInitialLoadFinished() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  StartProcessingPinnedButNotFetchedFiles();
}

void GDataSyncClient::OnCachePinned(const std::string& resource_id,
                                    const std::string& md5) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Add it to the queue, kick off the loop.
  queue_.push_back(SyncTask(FETCH, resource_id));
  StartSyncLoop();
}

void GDataSyncClient::OnCacheUnpinned(const std::string& resource_id,
                                      const std::string& md5) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Remove the resource_id if it's in the queue. This can happen if the user
  // cancels pinning before the file is fetched.
  for (std::deque<SyncTask>::iterator iter = queue_.begin();
       iter != queue_.end(); ++iter) {
    const SyncTask& sync_task = *iter;
    if (sync_task.sync_type == FETCH && sync_task.resource_id == resource_id) {
      queue_.erase(iter);
      break;
    }
  }
}

void GDataSyncClient::OnCacheCommitted(const std::string& resource_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // TODO(satorux): Initiate uploading of the committed file.
  // crbug.com/127080
}

void GDataSyncClient::OnGetResourceIdsOfPinnedButNotFetchedFiles(
    const std::vector<std::string>& resource_ids) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  for (size_t i = 0; i < resource_ids.size(); ++i) {
    const std::string& resource_id = resource_ids[i];
    DVLOG(1) << "Queuing " << resource_id;
    queue_.push_back(SyncTask(FETCH, resource_id));
  }

  StartSyncLoop();
}

void GDataSyncClient::OnFetchFileComplete(const std::string& resource_id,
                                          base::PlatformFileError error,
                                          const FilePath& local_path,
                                          const std::string& ununsed_mime_type,
                                          GDataFileType file_type) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (error == base::PLATFORM_FILE_OK) {
    DVLOG(1) << "Fetched " << resource_id << ": " << local_path.value();
  } else {
    // TODO(satorux): We should re-queue if the error is recoverable.
    LOG(WARNING) << "Failed to fetch " << resource_id << ": " << error;
  }

  // Continue the loop.
  DoSyncLoop();
}

void GDataSyncClient::OnNetworkManagerChanged(
    chromeos::NetworkLibrary* network_library) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Resume the sync loop if the network is back online. Note that we don't
  // need to check the type of the network as it will be checked in
  // ShouldStopSyncLoop() as soon as the loop is resumed.
  const chromeos::Network* active_network = network_library->active_network();
  if (active_network && active_network->online())
    StartSyncLoop();
}

void GDataSyncClient::Observe(int type,
                              const content::NotificationSource& source,
                              const content::NotificationDetails& details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Resume the sync loop if gdata preferences are changed. Note that we
  // don't need to check the new values here as these will be checked in
  // ShouldStopSyncLoop() as soon as the loop is resumed.
  StartSyncLoop();
}

}  // namespace gdata
