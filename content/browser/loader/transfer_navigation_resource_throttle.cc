// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/transfer_navigation_resource_throttle.h"

#include "base/bind.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/common/referrer.h"
#include "net/url_request/url_request.h"

namespace content {

namespace {

void RequestTransferURLOnUIThread(int render_process_id,
                                  int render_view_id,
                                  const GURL& new_url,
                                  const Referrer& referrer,
                                  WindowOpenDisposition window_open_disposition,
                                  int64 frame_id,
                                  const GlobalRequestID& global_request_id) {
  RenderViewHost* rvh =
      RenderViewHost::FromID(render_process_id, render_view_id);
  if (!rvh)
    return;

  RenderViewHostDelegate* delegate = rvh->GetDelegate();
  if (!delegate)
    return;

  delegate->RequestTransferURL(
      new_url, referrer, window_open_disposition,
      frame_id, global_request_id, false);
}

}  // namespace

TransferNavigationResourceThrottle::TransferNavigationResourceThrottle(
    net::URLRequest* request)
    : request_(request) {
}

TransferNavigationResourceThrottle::~TransferNavigationResourceThrottle() {
}

void TransferNavigationResourceThrottle::WillRedirectRequest(
    const GURL& new_url,
    bool* defer) {
  const ResourceRequestInfo* info = ResourceRequestInfo::ForRequest(request_);

  // If a toplevel request is redirecting across extension extents, we want to
  // switch processes. We do this by deferring the redirect and resuming the
  // request once the navigation controller properly assigns the right process
  // to host the new URL.
  // TODO(mpcomplete): handle for cases other than extensions (e.g. WebUI).
  ResourceContext* resource_context = info->GetContext();
  if (GetContentClient()->browser()->ShouldSwapProcessesForRedirect(
          resource_context, request_->url(), new_url)) {
    int render_process_id, render_view_id;
    if (info->GetAssociatedRenderView(&render_process_id, &render_view_id)) {
      GlobalRequestID global_id(info->GetChildID(), info->GetRequestID());

      ResourceDispatcherHostImpl::Get()->MarkAsTransferredNavigation(global_id);

      BrowserThread::PostTask(
          BrowserThread::UI,
          FROM_HERE,
          base::Bind(&RequestTransferURLOnUIThread,
              render_process_id,
              render_view_id,
              new_url,
              Referrer(GURL(request_->referrer()), info->GetReferrerPolicy()),
              CURRENT_TAB,
              info->GetFrameID(),
              global_id));

      *defer = true;
    }
  }
}

}  // namespace content
