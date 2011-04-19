// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_file_manager.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "base/stl_util-inl.h"
#include "base/task.h"
#include "base/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/download/download_util.h"
#include "chrome/browser/history/download_create_info.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "content/browser/browser_thread.h"
#include "content/browser/renderer_host/resource_dispatcher_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "googleurl/src/gurl.h"
#include "net/base/io_buffer.h"

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

void DownloadFileManager::OnResponseCompleted(
    int id,
    DownloadBuffer* buffer,
    int os_error,
    const std::string& security_info) {
  VLOG(20) << __FUNCTION__ << "()" << " id = " << id
           << " os_error = " << os_error
           << " security_info = \"" << security_info << "\"";
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  delete buffer;
  DownloadFile* download = GetDownloadFile(id);
  if (!download)
    return;

  download->Finish();

  DownloadManager* download_manager = download->GetDownloadManager();
  if (!download_manager) {
    CancelDownload(id);
    return;
  }

  std::string hash;
  if (!download->GetSha256Hash(&hash))
    hash.clear();

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(
        download_manager, &DownloadManager::OnResponseCompleted,
        id, download->bytes_so_far(), os_error, hash));
  // We need to keep the download around until the UI thread has finalized
  // the name.
}

// This method will be sent via a user action, or shutdown on the UI thread, and
// run on the download thread. Since this message has been sent from the UI
// thread, the download may have already completed and won't exist in our map.
void DownloadFileManager::CancelDownload(int id) {
  VLOG(20) << __FUNCTION__ << "()" << " id = " << id;
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DownloadFileMap::iterator it = downloads_.find(id);
  if (it == downloads_.end())
    return;

  DownloadFile* download = it->second;
  VLOG(20) << __FUNCTION__ << "()"
           << " download = " << download->DebugString();
  download->Cancel();

  EraseDownload(id);
}

void DownloadFileManager::CompleteDownload(int id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  if (!ContainsKey(downloads_, id))
    return;

  DownloadFile* download_file = downloads_[id];

  VLOG(20) << " " << __FUNCTION__ << "()"
           << " id = " << id
           << " download_file = " << download_file->DebugString();

  download_file->Detach();

  EraseDownload(id);
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
//
// There are 2 possible rename cases where this method can be called:
// 1. tmp -> foo.crdownload (not final, safe)
// 2. tmp-> Unconfirmed.xxx.crdownload (not final, dangerous)
void DownloadFileManager::RenameInProgressDownloadFile(
    int id, const FilePath& full_path) {
  VLOG(20) << __FUNCTION__ << "()" << " id = " << id
           << " full_path = \"" << full_path.value() << "\"";
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  DownloadFile* download = GetDownloadFile(id);
  if (!download)
    return;

  VLOG(20) << __FUNCTION__ << "()" << " download = " << download->DebugString();

  if (!download->Rename(full_path)) {
    // Error. Between the time the UI thread generated 'full_path' to the time
    // this code runs, something happened that prevents us from renaming.
    CancelDownloadOnRename(id);
  }
}

// The DownloadManager in the UI thread has provided a final name for the
// download specified by 'id'. Rename the download that's in the process
// of completing.
//
// There are 2 possible rename cases where this method can be called:
// 1. foo.crdownload -> foo (final, safe)
// 2. Unconfirmed.xxx.crdownload -> xxx (final, validated)
void DownloadFileManager::RenameCompletingDownloadFile(
    int id, const FilePath& full_path, bool overwrite_existing_file) {
  VLOG(20) << __FUNCTION__ << "()" << " id = " << id
           << " overwrite_existing_file = " << overwrite_existing_file
           << " full_path = \"" << full_path.value() << "\"";
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  DownloadFile* download = GetDownloadFile(id);
  if (!download)
    return;

  DCHECK(download->GetDownloadManager());
  DownloadManager* download_manager = download->GetDownloadManager();

  VLOG(20) << __FUNCTION__ << "()" << " download = " << download->DebugString();

  int uniquifier = 0;
  FilePath new_path = full_path;
  if (!overwrite_existing_file) {
    // Make our name unique at this point, as if a dangerous file is
    // downloading and a 2nd download is started for a file with the same
    // name, they would have the same path.  This is because we uniquify
    // the name on download start, and at that time the first file does
    // not exists yet, so the second file gets the same name.
    // This should not happen in the SAFE case, and we check for that in the UI
    // thread.
    uniquifier = download_util::GetUniquePathNumber(new_path);
    if (uniquifier > 0) {
      download_util::AppendNumberToPath(&new_path, uniquifier);
    }
  }

  // Rename the file, overwriting if necessary.
  if (!download->Rename(new_path)) {
    // Error. Between the time the UI thread generated 'full_path' to the time
    // this code runs, something happened that prevents us from renaming.
    CancelDownloadOnRename(id);
    return;
  }

#if defined(OS_MACOSX)
  // Done here because we only want to do this once; see
  // http://crbug.com/13120 for details.
  download->AnnotateWithSourceInformation();
#endif

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(
          download_manager, &DownloadManager::OnDownloadRenamedToFinalName, id,
          new_path, uniquifier));
}

// Called only from RenameInProgressDownloadFile and
// RenameCompletingDownloadFile on the FILE thread.
void DownloadFileManager::CancelDownloadOnRename(int id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  DownloadFile* download = GetDownloadFile(id);
  if (!download)
    return;

  DownloadManager* download_manager = download->GetDownloadManager();
  if (!download_manager) {
    // Without a download manager, we can't cancel the request normally, so we
    // need to do it here.  The normal path will also update the download
    // history before cancelling the request.
    download->CancelDownloadRequest(resource_dispatcher_host_);
    return;
  }

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(download_manager,
                        &DownloadManager::DownloadCancelled, id));
}

void DownloadFileManager::EraseDownload(int id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  if (!ContainsKey(downloads_, id))
    return;

  DownloadFile* download_file = downloads_[id];

  VLOG(20) << " " << __FUNCTION__ << "()"
           << " id = " << id
           << " download_file = " << download_file->DebugString();

  downloads_.erase(id);

  delete download_file;

  if (downloads_.empty())
    StopUpdateTimer();
}
