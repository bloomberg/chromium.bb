// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/download_resource_handler.h"

#include <string>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "content/browser/byte_stream.h"
#include "content/browser/download/download_create_info.h"
#include "content/browser/download/download_interrupt_reasons_impl.h"
#include "content/browser/download/download_manager_impl.h"
#include "content/browser/download/download_request_handle.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/loader/resource_request_info_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_interrupt_reasons.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/resource_response.h"

namespace content {

struct DownloadResourceHandler::DownloadTabInfo {
  GURL tab_url;
  GURL tab_referrer_url;
};

namespace {

// Static function in order to prevent any accidental accesses to
// DownloadResourceHandler members from the UI thread.
static void StartOnUIThread(
    scoped_ptr<DownloadCreateInfo> info,
    scoped_ptr<DownloadResourceHandler::DownloadTabInfo> tab_info,
    scoped_ptr<ByteStreamReader> stream,
    const DownloadUrlParameters::OnStartedCallback& started_cb) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  DownloadManager* download_manager =
      info->request_handle->GetDownloadManager();
  if (!download_manager) {
    // NULL in unittests or if the page closed right after starting the
    // download.
    if (!started_cb.is_null())
      started_cb.Run(NULL, DOWNLOAD_INTERRUPT_REASON_USER_CANCELED);

    // |stream| gets deleted on non-FILE thread, but it's ok since
    // we're not using stream_writer_ yet.

    return;
  }

  info->tab_url = tab_info->tab_url;
  info->tab_referrer_url = tab_info->tab_referrer_url;

  download_manager->StartDownload(std::move(info), std::move(stream),
                                  started_cb);
}

void InitializeDownloadTabInfoOnUIThread(
    const DownloadRequestHandle& request_handle,
    DownloadResourceHandler::DownloadTabInfo* tab_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  WebContents* web_contents = request_handle.GetWebContents();
  if (web_contents) {
    NavigationEntry* entry = web_contents->GetController().GetVisibleEntry();
    if (entry) {
      tab_info->tab_url = entry->GetURL();
      tab_info->tab_referrer_url = entry->GetReferrer().url;
    }
  }
}

void DeleteOnUIThread(
    scoped_ptr<DownloadResourceHandler::DownloadTabInfo> tab_info) {}

}  // namespace

DownloadResourceHandler::DownloadResourceHandler(
    uint32_t id,
    net::URLRequest* request,
    const DownloadUrlParameters::OnStartedCallback& started_cb,
    scoped_ptr<DownloadSaveInfo> save_info)
    : ResourceHandler(request),
      download_id_(id),
      started_cb_(started_cb),
      tab_info_(new DownloadTabInfo()),
      core_(request,
            std::move(save_info),
            base::Bind(&DownloadResourceHandler::OnCoreReadyToRead,
                       base::Unretained(this))) {
  // Do UI thread initialization for tab_info_ asap after
  // DownloadResourceHandler creation since the tab could be navigated
  // before StartOnUIThread gets called.  This is safe because deletion
  // will occur via PostTask() as well, which will serialized behind this
  // PostTask()
  const ResourceRequestInfoImpl* request_info = GetRequestInfo();
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&InitializeDownloadTabInfoOnUIThread,
                 DownloadRequestHandle(AsWeakPtr(), request_info->GetChildID(),
                                       request_info->GetRouteID(),
                                       request_info->GetRequestID(),
                                       request_info->frame_tree_node_id()),
                 tab_info_.get()));
}

DownloadResourceHandler::~DownloadResourceHandler() {
  // This won't do anything if the callback was called before.
  // If it goes through, it will likely be because OnWillStart() returned
  // false somewhere in the chain of resource handlers.
  CallStartedCB(DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED);

  if (tab_info_) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&DeleteOnUIThread, base::Passed(&tab_info_)));
  }
}

bool DownloadResourceHandler::OnRequestRedirected(
    const net::RedirectInfo& redirect_info,
    ResourceResponse* response,
    bool* defer) {
  return true;
}

// Send the download creation information to the download thread.
bool DownloadResourceHandler::OnResponseStarted(
    ResourceResponse* response,
    bool* defer) {
  scoped_ptr<DownloadCreateInfo> create_info;
  scoped_ptr<ByteStreamReader> stream_reader;

  core_.OnResponseStarted(&create_info, &stream_reader);

  const ResourceRequestInfoImpl* request_info = GetRequestInfo();
  create_info->download_id = download_id_;
  create_info->has_user_gesture = request_info->HasUserGesture();
  create_info->transition_type = request_info->GetPageTransition();
  create_info->request_handle.reset(new DownloadRequestHandle(
      AsWeakPtr(), request_info->GetChildID(), request_info->GetRouteID(),
      request_info->GetRequestID(), request_info->frame_tree_node_id()));

  // The MIME type in ResourceResponse is the product of
  // MimeTypeResourceHandler.
  create_info->mime_type = response->head.mime_type;

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&StartOnUIThread, base::Passed(&create_info),
                 base::Passed(&tab_info_), base::Passed(&stream_reader),
                 base::ResetAndReturn(&started_cb_)));
  return true;
}

void DownloadResourceHandler::CallStartedCB(
    DownloadInterruptReason interrupt_reason) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (started_cb_.is_null())
    return;
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(base::ResetAndReturn(&started_cb_),
                                     nullptr, interrupt_reason));
}

bool DownloadResourceHandler::OnWillStart(const GURL& url, bool* defer) {
  return true;
}

bool DownloadResourceHandler::OnBeforeNetworkStart(const GURL& url,
                                                   bool* defer) {
  return true;
}

// Create a new buffer, which will be handed to the download thread for file
// writing and deletion.
bool DownloadResourceHandler::OnWillRead(scoped_refptr<net::IOBuffer>* buf,
                                         int* buf_size,
                                         int min_size) {
  return core_.OnWillRead(buf, buf_size, min_size);
}

// Pass the buffer to the download file writer.
bool DownloadResourceHandler::OnReadCompleted(int bytes_read, bool* defer) {
  return core_.OnReadCompleted(bytes_read, defer);
}

void DownloadResourceHandler::OnResponseCompleted(
    const net::URLRequestStatus& status,
    const std::string& security_info,
    bool* defer) {
  DownloadInterruptReason result = core_.OnResponseCompleted(status);
  CallStartedCB(result);
}

void DownloadResourceHandler::OnDataDownloaded(int bytes_downloaded) {
  NOTREACHED();
}

void DownloadResourceHandler::PauseRequest() {
  core_.PauseRequest();
}

void DownloadResourceHandler::ResumeRequest() {
  core_.ResumeRequest();
}

void DownloadResourceHandler::OnCoreReadyToRead() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  controller()->Resume();
}

void DownloadResourceHandler::CancelRequest() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  const ResourceRequestInfoImpl* info = GetRequestInfo();
  ResourceDispatcherHostImpl::Get()->CancelRequest(
      info->GetChildID(),
      info->GetRequestID());
  // This object has been deleted.
}

std::string DownloadResourceHandler::DebugString() const {
  const ResourceRequestInfoImpl* info = GetRequestInfo();
  return base::StringPrintf("{"
                            " url_ = " "\"%s\""
                            " info = {"
                            " child_id = " "%d"
                            " request_id = " "%d"
                            " route_id = " "%d"
                            " }"
                            " }",
                            request() ?
                                request()->url().spec().c_str() :
                                "<NULL request>",
                            info->GetChildID(),
                            info->GetRequestID(),
                            info->GetRouteID());
}

}  // namespace content
