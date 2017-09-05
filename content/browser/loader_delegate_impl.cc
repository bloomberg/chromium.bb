// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader_delegate_impl.h"

#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/frame_host/navigator.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_thread.h"

namespace content {

namespace {

void DidGetResourceResponseStartOnUI(
    const ResourceRequestInfo::WebContentsGetter& web_contents_getter,
    std::unique_ptr<ResourceRequestDetails> details) {
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(web_contents_getter.Run());
  if (web_contents)
    web_contents->DidGetResourceResponseStart(*details.get());
}

void DidGetRedirectForResourceRequestOnUI(
    const ResourceRequestInfo::WebContentsGetter& web_contents_getter,
    std::unique_ptr<ResourceRedirectDetails> details) {
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(web_contents_getter.Run());
  if (!web_contents)
    return;
  web_contents->DidGetRedirectForResourceRequest(*details.get());
}

// This method is called in the UI thread to send the timestamp of a resource
// request to the respective Navigator (for an UMA histogram).
void DidGetLogResourceRequestTimeOnUI(base::TimeTicks timestamp,
                                      int render_process_id,
                                      int render_frame_id,
                                      const GURL& url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderFrameHostImpl* host =
      RenderFrameHostImpl::FromID(render_process_id, render_frame_id);
  if (host != nullptr) {
    DCHECK(host->frame_tree_node()->IsMainFrame());
    host->frame_tree_node()->navigator()->LogResourceRequestTime(
      timestamp, url);
  }
}

}  // namespace

LoaderDelegateImpl::~LoaderDelegateImpl() {}

void LoaderDelegateImpl::LoadStateChanged(
    WebContents* web_contents,
    const GURL& url,
    const net::LoadStateWithParam& load_state,
    uint64_t upload_position,
    uint64_t upload_size) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  static_cast<WebContentsImpl*>(web_contents)->LoadStateChanged(
      url, load_state, upload_position, upload_size);
}

void LoaderDelegateImpl::DidGetResourceResponseStart(
    const ResourceRequestInfo::WebContentsGetter& web_contents_getter,
    std::unique_ptr<ResourceRequestDetails> details) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::BindOnce(&DidGetResourceResponseStartOnUI, web_contents_getter,
                     base::Passed(std::move(details))));
}

void LoaderDelegateImpl::DidGetRedirectForResourceRequest(
    const ResourceRequestInfo::WebContentsGetter& web_contents_getter,
    std::unique_ptr<ResourceRedirectDetails> details) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::BindOnce(&DidGetRedirectForResourceRequestOnUI, web_contents_getter,
                     base::Passed(std::move(details))));
}

void LoaderDelegateImpl::LogResourceRequestTime(base::TimeTicks timestamp,
                                                int render_process_id,
                                                int render_frame_id,
                                                const GURL& url) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::BindOnce(&DidGetLogResourceRequestTimeOnUI, timestamp,
                     render_process_id, render_frame_id, url));
}

}  // namespace content
