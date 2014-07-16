// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "data_file_browser_cld_data_provider.h"

#include "base/basictypes.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/path_service.h"
#include "base/synchronization/lock.h"
#include "base/task_runner.h"
#include "components/translate/content/common/data_file_cld_data_provider_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_platform_file.h"

namespace {
// The data file,  cached as long as the process stays alive.
// We also track the offset at which the data starts, and its length.
base::FilePath g_cached_filepath;  // guarded by g_file_lock_
base::File* g_cached_file = NULL;  // guarded by g_file_lock_
uint64 g_cached_data_offset = -1;  // guarded by g_file_lock_
uint64 g_cached_data_length = -1;  // guarded by g_file_lock_

// Guards g_cached_filepath
base::LazyInstance<base::Lock> g_file_lock_;
}  // namespace

namespace translate {

// Implementation of the static factory method from BrowserCldDataProvider,
// hooking up this specific implementation for all of Chromium.
BrowserCldDataProvider* CreateBrowserCldDataProviderFor(
    content::WebContents* web_contents) {
  VLOG(1) << "Creating DataFileBrowserCldDataProvider";
  return new DataFileBrowserCldDataProvider(web_contents);
}

void DataFileBrowserCldDataProvider::SetCldDataFilePath(
    const base::FilePath& path) {
  VLOG(1) << "Setting CLD data file path to: " << path.value();
  base::AutoLock lock(g_file_lock_.Get());
  if (g_cached_filepath == path)
    return;  // no change necessary
  g_cached_filepath = path;
  // For sanity, clean these other values up just in case.
  g_cached_file = NULL;
  g_cached_data_length = -1;
  g_cached_data_offset = -1;
}

base::FilePath DataFileBrowserCldDataProvider::GetCldDataFilePath() {
  base::AutoLock lock(g_file_lock_.Get());
  return g_cached_filepath;
}

DataFileBrowserCldDataProvider::DataFileBrowserCldDataProvider(
    content::WebContents* web_contents)
    : web_contents_(web_contents), weak_pointer_factory_() {
}

DataFileBrowserCldDataProvider::~DataFileBrowserCldDataProvider() {
}

bool DataFileBrowserCldDataProvider::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(DataFileBrowserCldDataProvider, message)
  IPC_MESSAGE_HANDLER(ChromeViewHostMsg_NeedCldDataFile, OnCldDataRequest)
  IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void DataFileBrowserCldDataProvider::OnCldDataRequest() {
  // Quickly try to read g_cached_file. If valid, the file handle is
  // cached and can be used immediately. Else, queue the caching task to the
  // blocking pool.
  VLOG(1) << "Received request for CLD data file.";
  base::File* handle = NULL;
  uint64 data_offset = 0;
  uint64 data_length = 0;
  {
    base::AutoLock lock(g_file_lock_.Get());
    handle = g_cached_file;
    data_offset = g_cached_data_offset;
    data_length = g_cached_data_length;
  }

  if (handle && handle->IsValid()) {
    // Cached data available. Respond to the request.
    VLOG(1) << "CLD data file is already cached, replying immediately.";
    SendCldDataResponseInternal(handle, data_offset, data_length);
    return;
  }

  if (weak_pointer_factory_.get() == NULL) {
    weak_pointer_factory_.reset(
        new base::WeakPtrFactory<DataFileBrowserCldDataProvider>(this));
    weak_pointer_factory_.get()->GetWeakPtr().get();
  }

  // Else, we don't have the data file yet. Queue a caching attempt.
  // The caching attempt happens in the blocking pool because it may involve
  // arbitrary filesystem access.
  // After the caching attempt is made, we call MaybeSendCLDDataAvailable
  // to pass the file handle to the renderer. This only results in an IPC
  // message if the caching attempt was successful.
  VLOG(1) << "CLD data file not yet cached, deferring lookup";
  content::BrowserThread::PostBlockingPoolTaskAndReply(
      FROM_HERE,
      base::Bind(&DataFileBrowserCldDataProvider::OnCldDataRequestInternal),
      base::Bind(&DataFileBrowserCldDataProvider::SendCldDataResponse,
                 weak_pointer_factory_.get()->GetWeakPtr()));
}

void DataFileBrowserCldDataProvider::SendCldDataResponse() {
  base::File* handle = NULL;
  uint64 data_offset = 0;
  uint64 data_length = 0;
  {
    base::AutoLock lock(g_file_lock_.Get());
    handle = g_cached_file;
    data_offset = g_cached_data_offset;
    data_length = g_cached_data_length;
  }

  if (handle && handle->IsValid())
    SendCldDataResponseInternal(handle, data_offset, data_length);
}

void DataFileBrowserCldDataProvider::SendCldDataResponseInternal(
    const base::File* handle,
    const uint64 data_offset,
    const uint64 data_length) {
  VLOG(1) << "Sending CLD data file response.";

  content::RenderViewHost* render_view_host =
      web_contents_->GetRenderViewHost();
  if (render_view_host == NULL) {
    // Render view destroyed, no need to bother.
    VLOG(1) << "Lost render view host, giving up";
    return;
  }

  content::RenderProcessHost* render_process_host =
      render_view_host->GetProcess();
  if (render_process_host == NULL) {
    // Render process destroyed, render view not yet dead. No need to bother.
    VLOG(1) << "Lost render process, giving up";
    return;
  }

  // Data available, respond to the request.
  const int render_process_handle = render_process_host->GetHandle();
  IPC::PlatformFileForTransit ipc_platform_file =
      IPC::GetFileHandleForProcess(handle->GetPlatformFile(),
                                   render_process_handle, false);

  // In general, sending a response from within the code path that is processing
  // a request is discouraged because there is potential for deadlock (if the
  // methods are sent synchronously) or loops (if the response can trigger a
  // new request). Neither of these concerns is relevant in this code, so
  // sending the response from within the code path of the request handler is
  // safe.
  render_view_host->Send(
      new ChromeViewMsg_CldDataFileAvailable(render_view_host->GetRoutingID(),
                                             ipc_platform_file,
                                             data_offset,
                                             data_length));
}

void DataFileBrowserCldDataProvider::OnCldDataRequestInternal() {
  // Because this function involves arbitrary file system access, it must run
  // on the blocking pool.
  DCHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  DCHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  VLOG(1) << "CLD data file caching attempt starting.";

  {
    base::AutoLock lock(g_file_lock_.Get());
    if (g_cached_file) {
      VLOG(1) << "CLD data file is already cached, aborting caching attempt";
      return;  // Already done, duplicate request
    }
  }

  const base::FilePath path = GetCldDataFilePath();
  if (path.empty()) {
    VLOG(1) << "CLD data file does not yet have a known location.";
    return;
  }

  // If the file exists, we can send an IPC-safe construct back to the
  // renderer process immediately; otherwise, nothing to do here.
  if (!base::PathExists(path)) {
    VLOG(1) << "CLD data file does not exist.";
    return;
  }

  // Attempt to open the file for reading.
  scoped_ptr<base::File> file(
      new base::File(path, base::File::FLAG_OPEN | base::File::FLAG_READ));
  if (!file->IsValid()) {
    LOG(WARNING) << "CLD data file exists but cannot be opened";
    return;
  }

  base::File::Info file_info;
  if (!file->GetInfo(&file_info)) {
    LOG(WARNING) << "CLD data file exists but cannot be inspected";
    return;
  }

  // For now, our offset and length are simply 0 and the length of the file,
  // respectively. If we later decide to include the CLD2 data file inside of
  // a larger binary context, these params can be twiddled appropriately.
  const uint64 data_offset = 0;
  const uint64 data_length = file_info.size;

  {
    base::AutoLock lock(g_file_lock_.Get());
    if (g_cached_file) {
      // Idempotence: Racing another request on the blocking pool, abort.
      VLOG(1) << "Another thread finished caching first, aborting.";
    } else {
      // Else, this request has taken care of it all. Cache all info.
      VLOG(1) << "Caching CLD data file information.";
      g_cached_file = file.release();
      g_cached_data_offset = data_offset;
      g_cached_data_length = data_length;
    }
  }
}

}  // namespace translate
