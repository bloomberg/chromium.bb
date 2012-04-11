// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/download_request_handle.h"

#include "base/bind.h"
#include "base/stringprintf.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/resource_dispatcher_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;
using content::DownloadManager;
using content::RenderViewHostImpl;
using content::ResourceDispatcherHostImpl;

// IO Thread indirections to resource dispatcher host.
// Provided as targets for PostTask from within this object
// only.
static void DoPauseRequest(
    int process_unique_id,
    int request_id,
    bool pause) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  ResourceDispatcherHostImpl::Get()->PauseRequest(process_unique_id,
                                                  request_id,
                                                  pause);
}

static void DoCancelRequest(
    int process_unique_id,
    int request_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  ResourceDispatcherHostImpl::Get()->CancelRequest(process_unique_id,
                                                   request_id,
                                                   false);
}

DownloadRequestHandle::DownloadRequestHandle()
    : child_id_(-1),
      render_view_id_(-1),
      request_id_(-1) {
}

DownloadRequestHandle::DownloadRequestHandle(int child_id,
                                             int render_view_id,
                                             int request_id)
    : child_id_(child_id),
      render_view_id_(render_view_id),
      request_id_(request_id) {
  // ResourceDispatcherHostImpl should not be null for non-default instances of
  // DownloadRequestHandle.
  DCHECK(ResourceDispatcherHostImpl::Get());
}

content::WebContents* DownloadRequestHandle::GetWebContents() const {
  RenderViewHostImpl* render_view_host =
      RenderViewHostImpl::FromID(child_id_, render_view_id_);
  if (!render_view_host)
    return NULL;

  return render_view_host->GetDelegate()->GetAsWebContents();
}

DownloadManager* DownloadRequestHandle::GetDownloadManager() const {
  RenderViewHostImpl* rvh = RenderViewHostImpl::FromID(
      child_id_, render_view_id_);
  if (rvh == NULL)
    return NULL;
  content::RenderProcessHost* rph = rvh->GetProcess();
  if (rph == NULL)
    return NULL;
  content::BrowserContext* context = rph->GetBrowserContext();
  if (context == NULL)
    return NULL;
  return context->GetDownloadManager();
}

void DownloadRequestHandle::PauseRequest() const {
  // The post is safe because ResourceDispatcherHostImpl is guaranteed
  // to outlive the IO thread.
  if (ResourceDispatcherHostImpl::Get()) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&DoPauseRequest, child_id_, request_id_, true));
  }
}

void DownloadRequestHandle::ResumeRequest() const {
  // The post is safe because ResourceDispatcherHostImpl is guaranteed
  // to outlive the IO thread.
  if (ResourceDispatcherHostImpl::Get()) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&DoPauseRequest, child_id_, request_id_, false));
  }
}

void DownloadRequestHandle::CancelRequest() const {
  // The post is safe because ResourceDispatcherHostImpl is guaranteed
  // to outlive the IO thread.
  if (ResourceDispatcherHostImpl::Get()) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&DoCancelRequest, child_id_, request_id_));
  }
}

std::string DownloadRequestHandle::DebugString() const {
  return base::StringPrintf("{"
                            " child_id = %d"
                            " render_view_id = %d"
                            " request_id = %d"
                            "}",
                            child_id_,
                            render_view_id_,
                            request_id_);
}
