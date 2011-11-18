// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/download_request_handle.h"

#include "base/bind.h"
#include "base/stringprintf.h"
#include "content/browser/browser_context.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/resource_dispatcher_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

// IO Thread indirections to resource dispatcher host.
// Provided as targets for PostTask from within this object
// only.
static void ResourceDispatcherHostPauseRequest(
    ResourceDispatcherHost* rdh,
    int process_unique_id,
    int request_id,
    bool pause) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  rdh->PauseRequest(process_unique_id, request_id, pause);
}

static void ResourceDispatcherHostCancelRequest(
    ResourceDispatcherHost* rdh,
    int process_unique_id,
    int request_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  rdh->CancelRequest(process_unique_id, request_id, false);
}

DownloadRequestHandle::DownloadRequestHandle()
    : rdh_(NULL),
      child_id_(-1),
      render_view_id_(-1),
      request_id_(-1) {
}

DownloadRequestHandle::DownloadRequestHandle(ResourceDispatcherHost* rdh,
                                             int child_id,
                                             int render_view_id,
                                             int request_id)
    : rdh_(rdh),
      child_id_(child_id),
      render_view_id_(render_view_id),
      request_id_(request_id) {
  // ResourceDispatcherHost should not be null for non-default instances
  // of DownloadRequestHandle.
  DCHECK(rdh);
}

TabContents* DownloadRequestHandle::GetTabContents() const {
  RenderViewHost* render_view_host =
      RenderViewHost::FromID(child_id_, render_view_id_);
  if (!render_view_host)
    return NULL;

  return render_view_host->delegate()->GetAsTabContents();
}

DownloadManager* DownloadRequestHandle::GetDownloadManager() const {
  RenderViewHost* rvh = RenderViewHost::FromID(child_id_, render_view_id_);
  if (rvh == NULL)
    return NULL;
  content::RenderProcessHost* rph = rvh->process();
  if (rph == NULL)
    return NULL;
  content::BrowserContext* context = rph->GetBrowserContext();
  if (context == NULL)
    return NULL;
  return context->GetDownloadManager();
}

void DownloadRequestHandle::PauseRequest() const {
  // The post is safe because ResourceDispatcherHost is guaranteed
  // to outlive the IO thread.
  if (rdh_) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&ResourceDispatcherHostPauseRequest,
                   rdh_, child_id_, request_id_, true));
  }
}

void DownloadRequestHandle::ResumeRequest() const {
  // The post is safe because ResourceDispatcherHost is guaranteed
  // to outlive the IO thread.
  if (rdh_) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&ResourceDispatcherHostPauseRequest,
                   rdh_, child_id_, request_id_, false));
  }
}

void DownloadRequestHandle::CancelRequest() const {
  // The post is safe because ResourceDispatcherHost is guaranteed
  // to outlive the IO thread.
  if (rdh_) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&ResourceDispatcherHostCancelRequest,
                  rdh_, child_id_, request_id_));
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
