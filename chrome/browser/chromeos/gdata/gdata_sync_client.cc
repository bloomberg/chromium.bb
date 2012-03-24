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
#include "chrome/browser/chromeos/gdata/gdata_file_system.h"
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

GDataSyncClient::GDataSyncClient(GDataFileSystemInterface* file_system)
    : file_system_(file_system),
      weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

GDataSyncClient::~GDataSyncClient() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (file_system_)
    file_system_->RemoveObserver(this);
}

void GDataSyncClient::Initialize() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  file_system_->AddObserver(this);
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
                     file_system_->GetGDataCachePinnedDirectory(),
                     resource_ids),
          base::Bind(&GDataSyncClient::OnInitialScanComplete,
                     weak_ptr_factory_.GetWeakPtr(),
                     closure,
                     base::Owned(resource_ids)));
  DCHECK(posted);
}

std::vector<std::string> GDataSyncClient::GetResourceIdInQueueForTesting() {
  std::queue<std::string> copied_queue = queue_;
  std::vector<std::string> output;
  while (!copied_queue.empty()) {
    output.push_back(copied_queue.front());
    copied_queue.pop();
  }
  return output;
}

void GDataSyncClient::OnCacheInitialized() {
  // TODO(satorux): We should initiate the initial scan. I'm too lazy to do
  // this now.
  LOG(WARNING) << "Not implemented";
}

void GDataSyncClient::OnFilePinned(const std::string& resource_id,
                                   const std::string& md5) {
  // TODO(satorux): As of now, "available offline" column in the file browser
  // is not clickable and the user action is not propagated to the
  // GDataFileSystem.  crosbug.com/27961
  LOG(WARNING) << "Not implemented";
}

void GDataSyncClient::OnFileUnpinned(const std::string& resource_id,
                                     const std::string& md5) {
  // TODO(satorux): As of now, "available offline" column in the file browser
  // is not clickable and the user action is not propagated to the
  // GDataFileSystem.  crosbug.com/27961
  LOG(WARNING) << "Not implemented";
}

void GDataSyncClient::OnInitialScanComplete(
    const base::Closure& closure,
    std::vector<std::string>* resource_ids) {
  DCHECK(resource_ids);
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  for (size_t i = 0; i < resource_ids->size(); ++i) {
    const std::string& resource_id = (*resource_ids)[i];
    DVLOG(1) << "Queuing " << resource_id;
    queue_.push(resource_id);
  }

  closure.Run();
}

}  // namespace gdata
