// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_file_manager.h"

#include "base/file_util.h"
#include "base/stl_util-inl.h"
#include "base/task.h"
#include "base/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/download/download_util.h"
#include "chrome/browser/history/download_types.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/resource_dispatcher_host.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "googleurl/src/gurl.h"
#include "net/base/io_buffer.h"

#if defined(OS_WIN)
#include "app/win_util.h"
#include "chrome/common/win_safe_util.h"
#elif defined(OS_MACOSX)
#include "chrome/browser/cocoa/file_metadata.h"
#endif

namespace {

// Throttle updates to the UI thread so that a fast moving download doesn't
// cause it to become unresponsive (in milliseconds).
const int kUpdatePeriodMs = 500;

}  // namespace

DownloadFileManager::DownloadFileManager(ResourceDispatcherHost* rdh)
    : next_id_(0),
      resource_dispatcher_host_(rdh) {
}

DownloadFileManager::~DownloadFileManager() {
  // Check for clean shutdown.
  DCHECK(downloads_.empty());
}

// Called during the browser shutdown process to clean up any state (open files,
// timers) that live on the download_thread_.
void DownloadFileManager::Shutdown() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  StopUpdateTimer();
  ChromeThread::PostTask(
      ChromeThread::FILE, FROM_HERE,
      NewRunnableMethod(this, &DownloadFileManager::OnShutdown));
}

// Cease download thread operations.
void DownloadFileManager::OnShutdown() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));
  STLDeleteValues(&downloads_);
}

// Notifications sent from the download thread and run on the UI thread.

// Lookup the DownloadManager for this TabContents' profile and inform it of
// a new download.
// TODO(paulg): When implementing download restart via the Downloads tab,
//              there will be no 'render_process_id' or 'render_view_id'.
void DownloadFileManager::OnStartDownload(DownloadCreateInfo* info) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  DownloadManager* manager = DownloadManagerFromRenderIds(info->child_id,
                                                          info->render_view_id);
  if (!manager) {
    ChromeThread::PostTask(
        ChromeThread::IO, FROM_HERE,
        NewRunnableFunction(&download_util::CancelDownloadRequest,
                            resource_dispatcher_host_,
                            info->child_id,
                            info->request_id));
    delete info;
    return;
  }

  StartUpdateTimer();

  // Add the download manager to our request maps for future updates. We want to
  // be able to cancel all in progress downloads when a DownloadManager is
  // deleted, such as when a profile is closed. We also want to be able to look
  // up the DownloadManager associated with a given request without having to
  // rely on using tab information, since a tab may be closed while a download
  // initiated from that tab is still in progress.
  DownloadRequests& downloads = requests_[manager];
  downloads.insert(info->download_id);

  // TODO(paulg): The manager will exist when restarts are implemented.
  DownloadManagerMap::iterator dit = managers_.find(info->download_id);
  if (dit == managers_.end())
    managers_[info->download_id] = manager;
  else
    NOTREACHED();

  // StartDownload will clean up |info|.
  manager->StartDownload(info);
}

// Update the Download Manager with the finish state, and remove the request
// tracking entries.
void DownloadFileManager::OnDownloadFinished(int id,
                                             int64 bytes_so_far) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  DownloadManager* manager = GetDownloadManager(id);
  if (manager)
    manager->DownloadFinished(id, bytes_so_far);
  RemoveDownload(id, manager);
  RemoveDownloadFromUIProgress(id);
}

// Lookup one in-progress download.
DownloadFile* DownloadFileManager::GetDownloadFile(int id) {
  DownloadFileMap::iterator it = downloads_.find(id);
  return it == downloads_.end() ? NULL : it->second;
}

// The UI progress is updated on the file thread and removed on the UI thread.
void DownloadFileManager::RemoveDownloadFromUIProgress(int id) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  AutoLock lock(progress_lock_);
  if (ui_progress_.find(id) != ui_progress_.end())
    ui_progress_.erase(id);
}

// Throttle updates to the UI thread by only posting update notifications at a
// regularly controlled interval.
void DownloadFileManager::StartUpdateTimer() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  if (!update_timer_.IsRunning()) {
    update_timer_.Start(base::TimeDelta::FromMilliseconds(kUpdatePeriodMs),
                        this, &DownloadFileManager::UpdateInProgressDownloads);
  }
}

void DownloadFileManager::StopUpdateTimer() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  update_timer_.Stop();
}

// Our periodic timer has fired so send the UI thread updates on all in progress
// downloads.
void DownloadFileManager::UpdateInProgressDownloads() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  AutoLock lock(progress_lock_);
  ProgressMap::iterator it = ui_progress_.begin();
  for (; it != ui_progress_.end(); ++it) {
    const int id = it->first;
    DownloadManager* manager = GetDownloadManager(id);
    if (manager)
      manager->UpdateDownload(id, it->second);
  }
}

// Called on the IO thread once the ResourceDispatcherHost has decided that a
// request is a download.
int DownloadFileManager::GetNextId() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  return next_id_++;
}

// Notifications sent from the IO thread and run on the download thread:

// The IO thread created 'info', but the download thread (this method) uses it
// to create a DownloadFile, then passes 'info' to the UI thread where it is
// finally consumed and deleted.
void DownloadFileManager::StartDownload(DownloadCreateInfo* info) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));
  DCHECK(info);

  DownloadFile* download = new DownloadFile(info);
  if (!download->Initialize()) {
    // Couldn't open, cancel the operation. The UI thread does not yet know
    // about this download so we have to clean up 'info'. We need to get back
    // to the IO thread to cancel the network request and CancelDownloadRequest
    // on the UI thread is the safe way to do that.
    ChromeThread::PostTask(
        ChromeThread::IO, FROM_HERE,
        NewRunnableFunction(&download_util::CancelDownloadRequest,
                            resource_dispatcher_host_,
                            info->child_id,
                            info->request_id));
    delete info;
    delete download;
    return;
  }

  DCHECK(GetDownloadFile(info->download_id) == NULL);
  downloads_[info->download_id] = download;
  // TODO(phajdan.jr): fix the duplication of path info below.
  info->path = info->save_info.file_path;
  {
    AutoLock lock(progress_lock_);
    ui_progress_[info->download_id] = info->received_bytes;
  }

  ChromeThread::PostTask(
      ChromeThread::UI, FROM_HERE,
      NewRunnableMethod(this, &DownloadFileManager::OnStartDownload, info));
}

// We don't forward an update to the UI thread here, since we want to throttle
// the UI update rate via a periodic timer. If the user has cancelled the
// download (in the UI thread), we may receive a few more updates before the IO
// thread gets the cancel message: we just delete the data since the
// DownloadFile has been deleted.
void DownloadFileManager::UpdateDownload(int id, DownloadBuffer* buffer) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));
  std::vector<DownloadBuffer::Contents> contents;
  {
    AutoLock auto_lock(buffer->lock);
    contents.swap(buffer->contents);
  }

  // Keep track of how many bytes we have successfully saved to update
  // our progress status in the UI.
  int64 progress_bytes = 0;

  DownloadFile* download = GetDownloadFile(id);
  for (size_t i = 0; i < contents.size(); ++i) {
    net::IOBuffer* data = contents[i].first;
    const int data_len = contents[i].second;
    if (download) {
      if (download->AppendDataToFile(data->data(), data_len))
        progress_bytes += data_len;
    }
    data->Release();
  }

  if (download) {
    AutoLock lock(progress_lock_);
    ui_progress_[download->id()] += progress_bytes;
  }
}

void DownloadFileManager::DownloadFinished(int id, DownloadBuffer* buffer) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));
  delete buffer;
  DownloadFileMap::iterator it = downloads_.find(id);
  if (it != downloads_.end()) {
    DownloadFile* download = it->second;
    download->Finish();

    int64 download_size = -1;
    {
      AutoLock lock(progress_lock_);
      download_size = ui_progress_[download->id()];
    }

    ChromeThread::PostTask(
        ChromeThread::UI, FROM_HERE,
        NewRunnableMethod(
            this, &DownloadFileManager::OnDownloadFinished,
            id, download_size));

    // We need to keep the download around until the UI thread has finalized
    // the name.
    if (download->path_renamed()) {
      downloads_.erase(it);
      delete download;
    }
  }

  if (downloads_.empty())
    ChromeThread::PostTask(
      ChromeThread::UI, FROM_HERE,
      NewRunnableMethod(this, &DownloadFileManager::StopUpdateTimer));
}

// This method will be sent via a user action, or shutdown on the UI thread, and
// run on the download thread. Since this message has been sent from the UI
// thread, the download may have already completed and won't exist in our map.
void DownloadFileManager::CancelDownload(int id) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));
  DownloadFileMap::iterator it = downloads_.find(id);
  if (it != downloads_.end()) {
    DownloadFile* download = it->second;
    download->Cancel();

    ChromeThread::PostTask(
        ChromeThread::UI, FROM_HERE,
        NewRunnableMethod(
            this, &DownloadFileManager::RemoveDownloadFromUIProgress,
            download->id()));

    if (download->path_renamed()) {
      downloads_.erase(it);
      delete download;
    }
  }

  if (downloads_.empty()) {
    ChromeThread::PostTask(
      ChromeThread::UI, FROM_HERE,
      NewRunnableMethod(this, &DownloadFileManager::StopUpdateTimer));
  }
}

// Relate a download ID to its owning DownloadManager.
DownloadManager* DownloadFileManager::GetDownloadManager(int download_id) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  DownloadManagerMap::iterator it = managers_.find(download_id);
  if (it != managers_.end())
    return it->second;
  return NULL;
}

// Utility function for look up table maintenance, called on the UI thread.
// A manager may have multiple downloads in progress, so we just look up the
// one download (id) and remove it from the set, and remove the set if it
// becomes empty.
void DownloadFileManager::RemoveDownload(int id, DownloadManager* manager) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  if (manager) {
    RequestMap::iterator it = requests_.find(manager);
    if (it != requests_.end()) {
      DownloadRequests& downloads = it->second;
      DownloadRequests::iterator rit = downloads.find(id);
      if (rit != downloads.end())
        downloads.erase(rit);
      if (downloads.empty())
        requests_.erase(it);
    }
  }

  // A download can only have one manager, so remove it if it exists.
  DownloadManagerMap::iterator dit = managers_.find(id);
  if (dit != managers_.end())
    managers_.erase(dit);
}

// Utility function for converting request IDs to a TabContents. Must be called
// only on the UI thread since Profile operations may create UI objects, such as
// the first call to profile->GetDownloadManager().
// static
DownloadManager* DownloadFileManager::DownloadManagerFromRenderIds(
    int render_process_id, int render_view_id) {
  TabContents* contents = tab_util::GetTabContentsByID(render_process_id,
                                                       render_view_id);
  if (contents) {
    Profile* profile = contents->profile();
    if (profile)
      return profile->GetDownloadManager();
  }

  return NULL;
}

// Called by DownloadManagers in their destructor, and only on the UI thread.
void DownloadFileManager::RemoveDownloadManager(DownloadManager* manager) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  DCHECK(manager);
  RequestMap::iterator it = requests_.find(manager);
  if (it == requests_.end())
    return;

  const DownloadRequests& requests = it->second;
  DownloadRequests::const_iterator i = requests.begin();
  for (; i != requests.end(); ++i) {
    DownloadManagerMap::iterator dit = managers_.find(*i);
    if (dit != managers_.end()) {
      DCHECK(dit->second == manager);
      managers_.erase(dit);
    }
  }

  requests_.erase(it);
}

// Actions from the UI thread and run on the download thread

// Open a download, or show it in a file explorer window. We run on this
// thread to avoid blocking the UI with (potentially) slow Shell operations.
// TODO(paulg): File 'stat' operations.
#if !defined(OS_MACOSX)
void DownloadFileManager::OnShowDownloadInShell(const FilePath& full_path) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));
  platform_util::ShowItemInFolder(full_path);
}
#endif

// Launches the selected download using ShellExecute 'open' verb. For windows,
// if there is a valid parent window, the 'safer' version will be used which can
// display a modal dialog asking for user consent on dangerous files.
#if !defined(OS_MACOSX)
void DownloadFileManager::OnOpenDownloadInShell(const FilePath& full_path,
                                                const GURL& url,
                                                gfx::NativeView parent_window) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));
#if defined(OS_WIN)
  if (NULL != parent_window) {
    win_util::SaferOpenItemViaShell(parent_window, L"", full_path,
                                    UTF8ToWide(url.spec()));
    return;
  }
#endif
  platform_util::OpenItem(full_path);
}
#endif  // OS_MACOSX

// The DownloadManager in the UI thread has provided an intermediate .crdownload
// name for the download specified by 'id'. Rename the in progress download.
void DownloadFileManager::OnIntermediateDownloadName(
    int id, const FilePath& full_path, DownloadManager* download_manager)
{
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));
  DownloadFileMap::iterator it = downloads_.find(id);
  if (it == downloads_.end())
    return;

  DownloadFile* download = it->second;
  if (!download->Rename(full_path, false)) {
    // Error. Between the time the UI thread generated 'full_path' to the time
    // this code runs, something happened that prevents us from renaming.
    CancelDownloadOnRename(id);
  }
}

// The DownloadManager in the UI thread has provided a final name for the
// download specified by 'id'. Rename the in progress download, and remove it
// from our table if it has been completed or cancelled already.
// |need_delete_crdownload| indicates if we explicitly delete an intermediate
// .crdownload file or not.
//
// There are 3 possible rename cases where this method can be called:
// 1. tmp -> foo            (need_delete_crdownload=T)
// 2. foo.crdownload -> foo (need_delete_crdownload=F)
// 3. tmp-> unconfirmed.xxx.crdownload (need_delete_crdownload=F)
void DownloadFileManager::OnFinalDownloadName(int id,
                                              const FilePath& full_path,
                                              bool need_delete_crdownload,
                                              DownloadManager* manager) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));

  DownloadFile* download = GetDownloadFile(id);
  if (!download)
    return;

  if (download->Rename(full_path, true)) {
#if defined(OS_MACOSX)
    // Done here because we only want to do this once; see
    // http://crbug.com/13120 for details.
    download->AnnotateWithSourceInformation();
#endif
    ChromeThread::PostTask(
        ChromeThread::UI, FROM_HERE,
        NewRunnableMethod(
            manager, &DownloadManager::DownloadRenamedToFinalName, id,
            full_path));
  } else {
    // Error. Between the time the UI thread generated 'full_path' to the time
    // this code runs, something happened that prevents us from renaming.
    CancelDownloadOnRename(id);
  }

  if (need_delete_crdownload)
    download->DeleteCrDownload();

  // If the download has completed before we got this final name, we remove it
  // from our in progress map.
  if (!download->in_progress()) {
    downloads_.erase(id);
    delete download;
  }

  if (downloads_.empty()) {
    ChromeThread::PostTask(
      ChromeThread::UI, FROM_HERE,
      NewRunnableMethod(this, &DownloadFileManager::StopUpdateTimer));
  }
}

// Called only from OnFinalDownloadName or OnIntermediateDownloadName
// on the FILE thread.
void DownloadFileManager::CancelDownloadOnRename(int id) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));

  DownloadFile* download = GetDownloadFile(id);
  if (!download)
    return;

  DownloadManagerMap::iterator dmit = managers_.find(download->id());
  if (dmit != managers_.end()) {
    DownloadManager* dlm = dmit->second;
    ChromeThread::PostTask(
        ChromeThread::UI, FROM_HERE,
        NewRunnableMethod(dlm, &DownloadManager::DownloadCancelled, id));
  } else {
    download->CancelDownloadRequest(resource_dispatcher_host_);
  }
}
