// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/host_zoom_map_observer.h"

#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/host_zoom_map_impl.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/associated_interface_provider.h"

namespace content {

HostZoomMapObserver::HostZoomMapObserver(WebContents* web_contents)
    : WebContentsObserver(web_contents) {}

HostZoomMapObserver::~HostZoomMapObserver() {}

void HostZoomMapObserver::ReadyToCommitNavigation(
    NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame())
    return;

  DCHECK(host_zoom_.is_bound());
  if (!host_zoom_.is_bound())
    return;

  RenderFrameHost* render_frame_host =
      navigation_handle->GetRenderFrameHost();
  RenderProcessHost* render_process_host = render_frame_host->GetProcess();
  HostZoomMapImpl* host_zoom_map = static_cast<HostZoomMapImpl*>(
      render_process_host->GetStoragePartition()->GetHostZoomMap());
  double zoom_level = host_zoom_map->GetZoomLevelForView(
      navigation_handle->GetURL(), render_process_host->GetID(),
      render_frame_host->GetRenderViewHost()->GetRoutingID());
  host_zoom_->SetHostZoomLevel(navigation_handle->GetURL(), zoom_level);
}

void HostZoomMapObserver::RenderFrameCreated(RenderFrameHost* rfh) {
  rfh->GetRemoteAssociatedInterfaces()->GetInterface(&host_zoom_);
}

}  // namespace content
