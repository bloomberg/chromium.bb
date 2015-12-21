// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/download_request_handle.h"

#include "base/bind.h"
#include "base/strings/stringprintf.h"
#include "content/browser/frame_host/navigator.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/browser_side_navigation_policy.h"

namespace content {

DownloadRequestHandleInterface::~DownloadRequestHandleInterface() {}

DownloadRequestHandle::~DownloadRequestHandle() {}

DownloadRequestHandle::DownloadRequestHandle()
    : child_id_(-1),
      render_view_id_(-1),
      request_id_(-1),
      frame_tree_node_id_(-1) {
}

DownloadRequestHandle::DownloadRequestHandle(
    const base::WeakPtr<DownloadResourceHandler>& handler,
    int child_id,
    int render_view_id,
    int request_id,
    int frame_tree_node_id)
    : handler_(handler),
      child_id_(child_id),
      render_view_id_(render_view_id),
      request_id_(request_id),
      frame_tree_node_id_(frame_tree_node_id) {
  DCHECK(handler_.get());
}

WebContents* DownloadRequestHandle::GetWebContents() const {
  // PlzNavigate: if a FrameTreeNodeId was provided, use it to return the
  // WebContents.
  // TODO(davidben): This logic should be abstracted within the ResourceLoader
  // stack. https://crbug.com/376003
  if (IsBrowserSideNavigationEnabled()) {
    FrameTreeNode* frame_tree_node =
        FrameTreeNode::GloballyFindByID(frame_tree_node_id_);
    if (frame_tree_node) {
      return WebContentsImpl::FromFrameTreeNode(frame_tree_node);
    }
  }

  RenderViewHostImpl* render_view_host =
      RenderViewHostImpl::FromID(child_id_, render_view_id_);
  if (!render_view_host)
    return nullptr;

  return render_view_host->GetDelegate()->GetAsWebContents();
}

DownloadManager* DownloadRequestHandle::GetDownloadManager() const {
  // PlzNavigate: if a FrameTreeNodeId was provided, use it to return the
  // DownloadManager.
  // TODO(davidben): This logic should be abstracted within the ResourceLoader
  // stack. https://crbug.com/376003
  if (IsBrowserSideNavigationEnabled()) {
    FrameTreeNode* frame_tree_node =
        FrameTreeNode::GloballyFindByID(frame_tree_node_id_);
    if (frame_tree_node) {
      BrowserContext* context =
          frame_tree_node->navigator()->GetController()->GetBrowserContext();
      if (context)
        return BrowserContext::GetDownloadManager(context);
    }
  }

  RenderViewHostImpl* rvh = RenderViewHostImpl::FromID(
      child_id_, render_view_id_);
  if (rvh == NULL)
    return NULL;
  RenderProcessHost* rph = rvh->GetProcess();
  if (rph == NULL)
    return NULL;
  BrowserContext* context = rph->GetBrowserContext();
  if (context == NULL)
    return NULL;
  return BrowserContext::GetDownloadManager(context);
}

void DownloadRequestHandle::PauseRequest() const {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&DownloadResourceHandler::PauseRequest, handler_));
}

void DownloadRequestHandle::ResumeRequest() const {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&DownloadResourceHandler::ResumeRequest, handler_));
}

void DownloadRequestHandle::CancelRequest() const {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&DownloadResourceHandler::CancelRequest, handler_));
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

}  // namespace content
