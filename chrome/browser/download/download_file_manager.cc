// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_file_manager.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "base/stl_util-inl.h"
#include "base/task.h"
#include "base/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/download/download_util.h"
#include "chrome/browser/history/download_create_info.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_host/resource_dispatcher_host.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "googleurl/src/gurl.h"
#include "net/base/io_buffer.h"

#if defined(OS_WIN)
#include "chrome/common/win_safe_util.h"
#elif defined(OS_MACOSX)
#include "chrome/browser/ui/cocoa/file_metadata.h"
#endif

namespace {

// Throttle updates to the UI thread so that a fast moving download doesn't
// cause it to become unresponsive (in milliseconds).
const int kUpdatePeriodMs = 500;

DownloadManager* DownloadManagerForRenderViewHost(int render_process_id,
                                                  int render_view_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  TabContents* contents = tab_util::GetTabContentsByID(render_process_id,
                                                       render_view_id);
  if (contents) {
    Profile* profile = contents->profile();
    if (profile)
      return profile->GetDownloadManager();
  }

  return NULL;
}

}  // namespace

DownloadFileManager::DownloadFileManager(ResourceDispatcherHost* rdh)
    : next_id_(0),
      resource_dispatcher_host_(rdh) {
}

DownloadFileManager::~DownloadFileManager() {
  DCHECK(downloads_.empty());
}

void DownloadFileManager::Shutdown() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(this, &DownloadFileManager::OnShutdown));
}

void DownloadFileManager::OnShutdown() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  StopUpdateTimer();
  STLDeleteValues(&downloads_);
}

void DownloadFileManager::CreateDownloadFile(DownloadCreateInfo* info,
                                             DownloadManager* download_manager,
                                             bool get_hash) {
  VLOG(20) << __FUNCTION__ << "()" << " info = " << info->DebugString();
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  scoped_ptr<DownloadFile>
      download_file(new DownloadFile(info, download_manager));
  if (!download_file->Initialize(get_hash)) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        NewRunnableFunction(&download_util::CancelDownloadRequest,
                            resource_dispatcher_host_,
                            info->child_id,
                            info->request_id));
    delete info;
    return;
  }

  DCHECK(GetDownloadFile(info->download_id) == NULL);
  downloads_[info->download_id] = download_file.release();
  // TODO(phajdan.jr): fix the duplication of path info below.
  info->path = info->save_info.file_path;

  // The file is now ready, we can un-pause the request and start saving data.
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(this, &DownloadFileManager::ResumeDownloadRequest,
                        info->child_id, info->request_id));

  StartUpdateTimer();

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(download_manager,
                        &DownloadManager::StartDownload, info));
}

void DownloadFileManager::ResumeDownloadRequest(int child_id, int request_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // This balances the pause in DownloadResourceHandler::OnResponseStarted.
  resource_dispatcher_host_->PauseRequest(child_id, request_id, false);
}

DownloadFile* DownloadFileManager::GetDownloadFile(int id) {
  DownloadFileMap::iterator it = downloads_.find(id);
  return it == downloads_.end() ? NULL : it->second;
}

void DownloadFileManager::StartUpdateTimer() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  if (!update_timer_.IsRunning()) {
    update_timer_.Start(base::TimeDelta::FromMilliseconds(kUpdatePeriodMs),
                        this, &DownloadFileManager::UpdateInProgressDownloads);
  }
}

void DownloadFileManager::StopUpdateTimer() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  update_timer_.Stop();
}

void DownloadFileManager::UpdateInProgressDownloads() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  for (DownloadFileMap::iterator i = downloads_.begin();
       i != downloads_.end(); ++i) {
    int id = i->first;
    DownloadFile* download_file = i->second;
    DownloadManager* manager = download_file->GetDownloadManager();
    if (manager) {
      BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
          NewRunnableMethod(manager, &DownloadManager::UpdateDownload,
                            id, download_file->bytes_so_far()));
    }
  }
}

// Called on the IO thread once the ResourceDispatcherHost has decided that a
// request is a download.
int DownloadFileManager::GetNextId() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return next_id_++;
}

void DownloadFileManager::StartDownload(DownloadCreateInfo* info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(info);

  DownloadManager* manager = DownloadManagerForRenderViewHost(
      info->child_id, info->render_view_id);
  if (!manager) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        NewRunnableFunction(&download_util::CancelDownloadRequest,
                            resource_dispatcher_host_,
                            info->child_id,
                            info->request_id));
    delete info;
    return;
  }

  manager->CreateDownloadItem(info);

  bool hash_needed = resource_dispatcher_host_->safe_browsing_service()->
      DownloadBinHashNeeded();

  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(this, &DownloadFileManager::CreateDownloadFile,
                        info, make_scoped_refptr(manager), hash_needed));
}

// We don't forward an update to the UI thread here, since we want to throttle
// the UI update rate via a periodic timer. If the user has cancelled the
// download (in the UI thread), we may receive a few more updates before the IO
// thread gets the cancel message: we just delete the data since the
// DownloadFile has been deleted.
void DownloadFileManager::UpdateDownload(int id, DownloadBuffer* buffer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  std::vector<DownloadBuffer::Contents> contents;
  {
    base::AutoLock auto_lock(buffer->lock);
    contents.swap(buffer->contents);
  }

  DownloadFile* download = GetDownloadFile(id);
  for (size_t i = 0; i < contents.size(); ++i) {
    net::IOBuffer* data = contents[i].first;
    const int data_len = contents[i].second;
    if (download)
      download->AppendDataToFile(data->data(), data_len);
    data->Release();
  }
}

void DownloadFileManager::OnResponseCompleted(int id, DownloadBuffer* buffer) {
  VLOG(20) << __FUNCTION__ << "()" << " id = " << id;
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  delete buffer;
  DownloadFileMap::iterator it = downloads_.find(id);
  if (it != downloads_.end()) {
    DownloadFile* download = it->second;
    download->Finish();

    DownloadManager* download_manager = download->GetDownloadManager();
    if (download_manager) {
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          NewRunnableMethod(
              download_manager, &DownloadManager::OnAllDataSaved,
              id, download->bytes_so_far()));
    }

    // We need to keep the download around until the UI thread has finalized
    // the name.
    if (download->path_renamed()) {
      downloads_.erase(it);
      delete download;
    }
  }

  if (downloads_.empty())
    StopUpdateTimer();
}

// This method will be sent via a user action, or shutdown on the UI thread, and
// run on the download thread. Since this message has been sent from the UI
// thread, the download may have already completed and won't exist in our map.
void DownloadFileManager::CancelDownload(int id) {
  VLOG(20) << __FUNCTION__ << "()" << " id = " << id;
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DownloadFileMap::iterator it = downloads_.find(id);
  if (it != downloads_.end()) {
    DownloadFile* download = it->second;
    VLOG(20) << __FUNCTION__ << "()"
             << " download = " << download->DebugString();
    download->Cancel();

    if (download->path_renamed()) {
      downloads_.erase(it);
      delete download;
    }
  }

  if (downloads_.empty())
    StopUpdateTimer();
}

void DownloadFileManager::OnDownloadManagerShutdown(DownloadManager* manager) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK(manager);

  std::set<DownloadFile*> to_remove;

  for (DownloadFileMap::iterator i = downloads_.begin();
       i != downloads_.end(); ++i) {
    DownloadFile* download_file = i->second;
    if (download_file->GetDownloadManager() == manager) {
      download_file->CancelDownloadRequest(resource_dispatcher_host_);
      to_remove.insert(download_file);
    }
  }

  for (std::set<DownloadFile*>::iterator i = to_remove.begin();
       i != to_remove.end(); ++i) {
    downloads_.erase((*i)->id());
    delete *i;
  }
}

// Actions from the UI thread and run on the download thread

// The DownloadManager in the UI thread has provided an intermediate .crdownload
// name for the download specified by 'id'. Rename the in progress download.
void DownloadFileManager::OnIntermediateDownloadName(
    int id, const FilePath& full_path, DownloadManager* download_manager)
{
  VLOG(20) << __FUNCTION__ << "()" << " id = " << id
           << " full_path = \"" << full_path.value() << "\"";
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DownloadFileMap::iterator it = downloads_.find(id);
  if (it == downloads_.end())
    return;

  DownloadFile* download = it->second;
  VLOG(20) << __FUNCTION__ << "()" << " download = " << download->DebugString();
  if (!download->Rename(full_path, false /* is_final_rename */)) {
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
void DownloadFileManager::OnFinalDownloadName(
    int id, const FilePath& full_path, bool need_delete_crdownload,
    DownloadManager* download_manager) {
  VLOG(20) << __FUNCTION__ << "()" << " id = " << id
           << " full_path = \"" << full_path.value() << "\""
           << " need_delete_crdownload = " << need_delete_crdownload;
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  DownloadFile* download = GetDownloadFile(id);
  if (!download)
    return;
  VLOG(20) << __FUNCTION__ << "()" << " download = " << download->DebugString();
  if (download->Rename(full_path, true /* is_final_rename */)) {
#if defined(OS_MACOSX)
    // Done here because we only want to do this once; see
    // http://crbug.com/13120 for details.
    download->AnnotateWithSourceInformation();
#endif
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        NewRunnableMethod(
            download_manager, &DownloadManager::DownloadRenamedToFinalName, id,
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

  if (downloads_.empty())
    StopUpdateTimer();
}

// Called only from OnFinalDownloadName or OnIntermediateDownloadName
// on the FILE thread.
void DownloadFileManager::CancelDownloadOnRename(int id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  DownloadFile* download = GetDownloadFile(id);
  if (!download)
    return;

  DownloadManager* download_manager = download->GetDownloadManager();
  if (!download_manager) {
    download->CancelDownloadRequest(resource_dispatcher_host_);
    return;
  }

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(download_manager,
                        &DownloadManager::DownloadCancelled, id));
}
