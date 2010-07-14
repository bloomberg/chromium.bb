// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_file.h"

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/stl_util-inl.h"
#include "base/task.h"
#include "base/thread.h"
#include "base/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/download/download_util.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/resource_dispatcher_host.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/chrome_paths.h"
#include "googleurl/src/gurl.h"
#include "net/base/io_buffer.h"
#include "net/base/net_util.h"
#include "net/url_request/url_request_context.h"

#if defined(OS_WIN)
#include "app/win_util.h"
#include "chrome/common/win_safe_util.h"
#elif defined(OS_MACOSX)
#include "chrome/browser/cocoa/file_metadata.h"
#endif

// Throttle updates to the UI thread so that a fast moving download doesn't
// cause it to become unresponsive (in milliseconds).
static const int kUpdatePeriodMs = 500;

// DownloadFile implementation -------------------------------------------------

DownloadFile::DownloadFile(const DownloadCreateInfo* info)
    : file_stream_(info->save_info.file_stream),
      source_url_(info->url),
      referrer_url_(info->referrer_url),
      id_(info->download_id),
      child_id_(info->child_id),
      render_view_id_(info->render_view_id),
      request_id_(info->request_id),
      bytes_so_far_(0),
      full_path_(info->save_info.file_path),
      path_renamed_(false),
      in_progress_(true),
      dont_sleep_(true),
      save_info_(info->save_info) {
}

DownloadFile::~DownloadFile() {
  Close();
}

bool DownloadFile::Initialize() {
  if (!full_path_.empty() ||
      download_util::CreateTemporaryFileForDownload(&full_path_))
    return Open();
  return false;
}

bool DownloadFile::AppendDataToFile(const char* data, int data_len) {
  if (file_stream_.get()) {
    // FIXME bug 595247: handle errors on file writes.
    size_t written = file_stream_->Write(data, data_len, NULL);
    bytes_so_far_ += written;
    return true;
  }
  return false;
}

void DownloadFile::Cancel() {
  Close();
  if (!full_path_.empty())
    file_util::Delete(full_path_, false);
}

// The UI has provided us with our finalized name.
bool DownloadFile::Rename(const FilePath& new_path) {
  // If the new path is same as the old one, there is no need to perform the
  // following renaming logic.
  if (new_path == full_path_) {
    path_renamed_ = true;

    // Don't close the file if we're not done (finished or canceled).
    if (!in_progress_)
      Close();

    return true;
  }

  Close();

#if defined(OS_WIN)
  // We cannot rename because rename will keep the same security descriptor
  // on the destination file. We want to recreate the security descriptor
  // with the security that makes sense in the new path.
  if (!file_util::RenameFileAndResetSecurityDescriptor(full_path_, new_path))
    return false;
#elif defined(OS_POSIX)
  {
    // Similarly, on Unix, we're moving a temp file created with permissions
    // 600 to |new_path|. Here, we try to fix up the destination file with
    // appropriate permissions.
    struct stat st;
    // First check the file existence and create an empty file if it doesn't
    // exist.
    if (!file_util::PathExists(new_path))
      file_util::WriteFile(new_path, "", 0);
    bool stat_succeeded = (stat(new_path.value().c_str(), &st) == 0);

    // TODO(estade): Move() falls back to copying and deleting when a simple
    // rename fails. Copying sucks for large downloads. crbug.com/8737
    if (!file_util::Move(full_path_, new_path))
      return false;

    if (stat_succeeded)
      chmod(new_path.value().c_str(), st.st_mode);
  }
#endif

  full_path_ = new_path;
  path_renamed_ = true;

  // We don't need to re-open the file if we're done (finished or canceled).
  if (!in_progress_)
    return true;

  if (!Open())
    return false;

  // Move to the end of the new file.
  if (file_stream_->Seek(net::FROM_END, 0) < 0)
    return false;

  return true;
}

void DownloadFile::Close() {
  if (file_stream_.get()) {
#if defined(OS_CHROMEOS)
    // Currently we don't really care about the return value, since if it fails
    // theres not much we can do.  But we might in the future.
    file_stream_->Flush();
#endif
    file_stream_->Close();
    file_stream_.reset();
  }
}

bool DownloadFile::Open() {
  DCHECK(!full_path_.empty());

  // Create a new file steram if it is not provided.
  if (!file_stream_.get()) {
    file_stream_.reset(new net::FileStream);
    if (file_stream_->Open(full_path_,
                          base::PLATFORM_FILE_OPEN_ALWAYS |
                              base::PLATFORM_FILE_WRITE) != net::OK) {
      file_stream_.reset();
      return false;
    }
  }

#if defined(OS_WIN)
  AnnotateWithSourceInformation();
#endif
  return true;
}

void DownloadFile::AnnotateWithSourceInformation() {
#if defined(OS_WIN)
  // Sets the Zone to tell Windows that this file comes from the internet.
  // We ignore the return value because a failure is not fatal.
  win_util::SetInternetZoneIdentifier(full_path_);
#elif defined(OS_MACOSX)
  file_metadata::AddQuarantineMetadataToFile(full_path_, source_url_,
                                             referrer_url_);
  file_metadata::AddOriginMetadataToFile(full_path_, source_url_,
                                         referrer_url_);
#endif
}

// DownloadFileManager implementation ------------------------------------------

DownloadFileManager::DownloadFileManager(ResourceDispatcherHost* rdh)
    : next_id_(0),
      resource_dispatcher_host_(rdh) {
}

DownloadFileManager::~DownloadFileManager() {
  // Check for clean shutdown.
  DCHECK(downloads_.empty());
  ui_progress_.clear();
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
  // Delete any partial downloads during shutdown.
  for (DownloadFileMap::iterator it = downloads_.begin();
       it != downloads_.end(); ++it) {
    DownloadFile* download = it->second;
    if (download->in_progress())
      download->Cancel();
    delete download;
  }
  downloads_.clear();
}

// Initiate a request for URL to be downloaded. Called from UI thread,
// runs on IO thread.
void DownloadFileManager::OnDownloadUrl(
    const GURL& url,
    const GURL& referrer,
    const std::string& referrer_charset,
    const DownloadSaveInfo& save_info,
    int render_process_host_id,
    int render_view_id,
    URLRequestContextGetter* request_context_getter) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));

  URLRequestContext* context = request_context_getter->GetURLRequestContext();
  context->set_referrer_charset(referrer_charset);

  resource_dispatcher_host_->BeginDownload(url,
                                           referrer,
                                           save_info,
                                           render_process_host_id,
                                           render_view_id,
                                           context);
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
        NewRunnableFunction(&DownloadManager::OnCancelDownloadRequest,
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
  DownloadManager* manager = LookupManager(id);
  if (manager)
    manager->DownloadFinished(id, bytes_so_far);
  RemoveDownload(id, manager);
  RemoveDownloadFromUIProgress(id);
}

// Lookup one in-progress download.
DownloadFile* DownloadFileManager::LookupDownload(int id) {
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
    DownloadManager* manager = LookupManager(id);
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
        NewRunnableFunction(&DownloadManager::OnCancelDownloadRequest,
                            resource_dispatcher_host_,
                            info->child_id,
                            info->request_id));
    delete info;
    delete download;
    return;
  }

  DCHECK(LookupDownload(info->download_id) == NULL);
  downloads_[info->download_id] = download;
  info->path = download->full_path();
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

  DownloadFile* download = LookupDownload(id);
  for (size_t i = 0; i < contents.size(); ++i) {
    net::IOBuffer* data = contents[i].first;
    const int data_len = contents[i].second;
    if (download)
      download->AppendDataToFile(data->data(), data_len);
    data->Release();
  }

  if (download) {
    AutoLock lock(progress_lock_);
    ui_progress_[download->id()] = download->bytes_so_far();
  }
}

void DownloadFileManager::DownloadFinished(int id, DownloadBuffer* buffer) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));
  delete buffer;
  DownloadFileMap::iterator it = downloads_.find(id);
  if (it != downloads_.end()) {
    DownloadFile* download = it->second;
    download->set_in_progress(false);

    ChromeThread::PostTask(
        ChromeThread::UI, FROM_HERE,
        NewRunnableMethod(
            this, &DownloadFileManager::OnDownloadFinished,
            id, download->bytes_so_far()));

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
    download->set_in_progress(false);

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

void DownloadFileManager::DownloadUrl(
    const GURL& url,
    const GURL& referrer,
    const std::string& referrer_charset,
    const DownloadSaveInfo& save_info,
    int render_process_host_id,
    int render_view_id,
    URLRequestContextGetter* request_context_getter) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  ChromeThread::PostTask(
        ChromeThread::IO, FROM_HERE,
        NewRunnableMethod(this,
                          &DownloadFileManager::OnDownloadUrl,
                          url,
                          referrer,
                          referrer_charset,
                          save_info,
                          render_process_host_id,
                          render_view_id,
                          request_context_getter));
}

// Relate a download ID to its owning DownloadManager.
DownloadManager* DownloadFileManager::LookupManager(int download_id) {
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

// The DownloadManager in the UI thread has provided a final name for the
// download specified by 'id'. Rename the in progress download, and remove it
// from our table if it has been completed or cancelled already.
void DownloadFileManager::OnFinalDownloadName(int id,
                                              const FilePath& full_path,
                                              DownloadManager* manager) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));
  DownloadFileMap::iterator it = downloads_.find(id);
  if (it == downloads_.end())
    return;

  file_util::CreateDirectory(full_path.DirName());

  DownloadFile* download = it->second;
  if (download->Rename(full_path)) {
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
    DownloadManagerMap::iterator dmit = managers_.find(download->id());
    if (dmit != managers_.end()) {
      DownloadManager* dlm = dmit->second;
      ChromeThread::PostTask(
          ChromeThread::UI, FROM_HERE,
          NewRunnableMethod(dlm, &DownloadManager::DownloadCancelled, id));
    } else {
      ChromeThread::PostTask(
          ChromeThread::IO, FROM_HERE,
          NewRunnableFunction(&DownloadManager::OnCancelDownloadRequest,
                              resource_dispatcher_host_,
                              download->child_id(),
                              download->request_id()));
    }
  }

  // If the download has completed before we got this final name, we remove it
  // from our in progress map.
  if (!download->in_progress()) {
    downloads_.erase(it);
    delete download;
  }

  if (downloads_.empty()) {
    ChromeThread::PostTask(
      ChromeThread::UI, FROM_HERE,
      NewRunnableMethod(this, &DownloadFileManager::StopUpdateTimer));
  }
}

