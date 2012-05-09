// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/gdata_sync_client.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

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
namespace {

// Scans the pinned directory and returns pinned-but-not-fetched files.
void ScanPinnedDirectory(const FilePath& directory,
                         std::vector<std::string>* resource_ids) {
  using file_util::FileEnumerator;

  DVLOG(1) << "Scanning " << directory.value();
  FileEnumerator::FileType file_types =
      static_cast<FileEnumerator::FileType>(FileEnumerator::FILES |
                                            FileEnumerator::SHOW_SYM_LINKS);

  FileEnumerator enumerator(directory, false /* recursive */, file_types);
  for (FilePath file_path = enumerator.Next(); !file_path.empty();
       file_path = enumerator.Next()) {
    // Check if it's a symlink.
    FileEnumerator::FindInfo find_info;
    enumerator.GetFindInfo(&find_info);
    const bool is_symlink = S_ISLNK(find_info.stat.st_mode);
    if (!is_symlink) {
      LOG(WARNING) << "Removing " << file_path.value() << " (not a symlink)";
      file_util::Delete(file_path, false /* recursive */);
      continue;
    }
    // Read the symbolic link.
    FilePath destination;
    if (!file_util::ReadSymbolicLink(file_path, &destination)) {
      LOG(WARNING) << "Removing " << file_path.value() << " (not readable)";
      file_util::Delete(file_path, false /* recursive */);
      continue;
    }
    // Remove the symbolic link if it's dangling. Something went wrong.
    if (!file_util::PathExists(destination)) {
      LOG(WARNING) << "Removing " << file_path.value() << " (dangling)";
      file_util::Delete(file_path, false /* recursive */);
      continue;
    }
    // Add it to the output list, if the symlink points to /dev/null.
    if (destination == FilePath::FromUTF8Unsafe("/dev/null")) {
      std::string resource_id = file_path.BaseName().AsUTF8Unsafe();
      resource_ids->push_back(resource_id);
    }
  }
}

}  // namespace

GDataSyncClient::GDataSyncClient(Profile* profile,
                                 GDataFileSystemInterface* file_system)
    : profile_(profile),
      file_system_(file_system),
      registrar_(new PrefChangeRegistrar),
      fetch_loop_is_running_(false),
      weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

GDataSyncClient::~GDataSyncClient() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (file_system_)
    file_system_->RemoveObserver(this);

  chromeos::NetworkLibrary* network_library =
      chromeos::CrosLibrary::Get()->GetNetworkLibrary();
  if (network_library)
    network_library->RemoveNetworkManagerObserver(this);
}

void GDataSyncClient::Initialize() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  file_system_->AddObserver(this);

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

void GDataSyncClient::StartInitialScan(const base::Closure& closure) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!closure.is_null());

  // Start scanning the pinned directory in the cache.
  std::vector<std::string>* resource_ids = new std::vector<std::string>;
  const bool posted =
      BrowserThread::GetBlockingPool()->PostTaskAndReply(
          FROM_HERE,
          base::Bind(&ScanPinnedDirectory,
                     file_system_->GetCacheDirectoryPath(
                         GDataRootDirectory::CACHE_TYPE_PINNED),
                     resource_ids),
          base::Bind(&GDataSyncClient::OnInitialScanComplete,
                     weak_ptr_factory_.GetWeakPtr(),
                     closure,
                     base::Owned(resource_ids)));
  DCHECK(posted);
}

void GDataSyncClient::StartFetchLoop() {
  if (!fetch_loop_is_running_)
    DoFetchLoop();
}

void GDataSyncClient::DoFetchLoop() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (queue_.empty() || ShouldStopFetchLoop()) {
    // Note that |queue_| is not cleared so the fetch loop can resume.
    fetch_loop_is_running_ = false;
    return;
  }
  fetch_loop_is_running_ = true;

  const std::string resource_id = queue_.front();
  queue_.pop_front();

  DVLOG(1) << "Fetching " << resource_id;
  file_system_->GetFileByResourceId(
      resource_id,
      base::Bind(&GDataSyncClient::OnFetchFileComplete,
                 weak_ptr_factory_.GetWeakPtr(),
                 resource_id),
      GetDownloadDataCallback());
}

bool GDataSyncClient::ShouldStopFetchLoop() {
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
      active_network->type() == chromeos::TYPE_CELLULAR)
    return true;

  return false;
}

void GDataSyncClient::OnCacheInitialized() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Start the initial scan. Once it's complete, start the fetch loop.
  StartInitialScan(base::Bind(&GDataSyncClient::StartFetchLoop,
                              weak_ptr_factory_.GetWeakPtr()));
}

void GDataSyncClient::OnFilePinned(const std::string& resource_id,
                                   const std::string& md5) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Add it to the queue, kick off the loop.
  queue_.push_back(resource_id);
  StartFetchLoop();
}

void GDataSyncClient::OnFileUnpinned(const std::string& resource_id,
                                     const std::string& md5) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Remove the resource_id if it's in the queue. This can happen if the user
  // cancels pinning before the file is fetched.
  std::deque<std::string>::iterator iter =
      std::find(queue_.begin(), queue_.end(), resource_id);
  if (iter != queue_.end())
    queue_.erase(iter);
}

void GDataSyncClient::OnInitialScanComplete(
    const base::Closure& closure,
    std::vector<std::string>* resource_ids) {
  DCHECK(resource_ids);
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  for (size_t i = 0; i < resource_ids->size(); ++i) {
    const std::string& resource_id = (*resource_ids)[i];
    DVLOG(1) << "Queuing " << resource_id;
    queue_.push_back(resource_id);
  }

  closure.Run();
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
  DoFetchLoop();
}

void GDataSyncClient::OnNetworkManagerChanged(
    chromeos::NetworkLibrary* network_library) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Resume the fetch loop if the network is back online. Note that we don't
  // need to check the type of the network as it will be checked in
  // ShouldStopFetchLoop() as soon as the loop is resumed.
  const chromeos::Network* active_network = network_library->active_network();
  if (active_network && active_network->online())
    StartFetchLoop();
}

void GDataSyncClient::Observe(int type,
                              const content::NotificationSource& source,
                              const content::NotificationDetails& details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Resume the fetch loop if gdata preferences are changed. Note that we
  // don't need to check the new values here as these will be checked in
  // ShouldStopFetchLoop() as soon as the loop is resumed.
  StartFetchLoop();
}

}  // namespace gdata
