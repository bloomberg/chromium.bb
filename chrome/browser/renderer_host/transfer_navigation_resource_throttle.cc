// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/transfer_navigation_resource_throttle.h"

#include "base/bind.h"
#include "chrome/browser/extensions/extension_info_map.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/common/extensions/extension_process_policy.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_view_host_delegate.h"
#include "content/public/browser/resource_dispatcher_host.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/common/referrer.h"
#include "net/url_request/url_request.h"

using content::GlobalRequestID;
using content::Referrer;
using content::RenderViewHostDelegate;
using content::ResourceDispatcherHost;
using content::ResourceRequestInfo;

namespace {

void RequestTransferURLOnUIThread(int render_process_id,
                                  int render_view_id,
                                  const GURL& new_url,
                                  const Referrer& referrer,
                                  WindowOpenDisposition window_open_disposition,
                                  int64 frame_id,
                                  const GlobalRequestID& global_request_id) {
  content::RenderViewHost* rvh =
      content::RenderViewHost::FromID(render_process_id, render_view_id);
  if (!rvh)
    return;

  RenderViewHostDelegate* delegate = rvh->GetDelegate();
  if (!delegate)
    return;

  delegate->RequestTransferURL(
      new_url, referrer, window_open_disposition, frame_id, global_request_id);
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
  // TODO(darin): Move this logic into src/content.

  const ResourceRequestInfo* info = ResourceRequestInfo::ForRequest(request_);

  // If a toplevel request is redirecting across extension extents, we want to
  // switch processes. We do this by deferring the redirect and resuming the
  // request once the navigation controller properly assigns the right process
  // to host the new URL.
  // TODO(mpcomplete): handle for cases other than extensions (e.g. WebUI).
  content::ResourceContext* resource_context = info->GetContext();
  ProfileIOData* io_data = ProfileIOData::FromResourceContext(resource_context);
  if (extensions::CrossesExtensionProcessBoundary(
          io_data->GetExtensionInfoMap()->extensions(),
          ExtensionURLInfo(request_->url()), ExtensionURLInfo(new_url))) {
    int render_process_id, render_view_id;
    if (info->GetAssociatedRenderView(&render_process_id, &render_view_id)) {
      ResourceDispatcherHost::Get()->MarkAsTransferredNavigation(request_);

      GlobalRequestID global_id(info->GetChildID(), info->GetRequestID());
      content::BrowserThread::PostTask(
          content::BrowserThread::UI,
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
