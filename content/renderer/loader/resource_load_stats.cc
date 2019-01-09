// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/loader/resource_load_stats.h"

#include "base/metrics/histogram_macros.h"
#include "content/common/net/record_load_histograms.h"
#include "content/public/common/resource_load_info.mojom.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_thread_impl.h"
#include "services/network/public/cpp/resource_response.h"
#include "services/network/public/cpp/url_loader_completion_status.h"

namespace content {

namespace {

#if defined(OS_ANDROID)
void UpdateUserGestureCarryoverInfo(int render_frame_id) {
  RenderFrameImpl* frame = RenderFrameImpl::FromRoutingID(render_frame_id);
  if (frame)
    frame->GetFrameHost()->UpdateUserGestureCarryoverInfo();
}
#endif

void ResourceLoadStarted(int render_frame_id,
                         int request_id,
                         const GURL& response_url,
                         const network::ResourceResponseHead& response_head,
                         content::ResourceType resource_type) {
  RenderFrameImpl* frame = RenderFrameImpl::FromRoutingID(render_frame_id);
  if (!frame)
    return;
  if (!IsResourceTypeFrame(resource_type)) {
    frame->GetFrameHost()->SubresourceResponseStarted(
        response_url, response_head.cert_status);
  }
  frame->DidStartResponse(response_url, request_id, response_head,
                          resource_type);
}

void ResourceTransferSizeUpdated(int render_frame_id,
                                 int request_id,
                                 int transfer_size_diff) {
  RenderFrameImpl* frame = RenderFrameImpl::FromRoutingID(render_frame_id);
  if (frame)
    frame->DidReceiveTransferSizeUpdate(request_id, transfer_size_diff);
}

void ResourceLoadCompleted(int render_frame_id,
                           mojom::ResourceLoadInfoPtr resource_load_info,
                           const network::URLLoaderCompletionStatus& status) {
  RenderFrameImpl* frame = RenderFrameImpl::FromRoutingID(render_frame_id);
  if (!frame)
    return;
  frame->DidCompleteResponse(resource_load_info->request_id, status);
  frame->GetFrameHost()->ResourceLoadComplete(std::move(resource_load_info));
}

void ResourceLoadCanceled(int render_frame_id, int request_id) {
  RenderFrameImpl* frame = RenderFrameImpl::FromRoutingID(render_frame_id);
  if (frame)
    frame->DidCancelResponse(request_id);
}

}  // namespace

#if defined(OS_ANDROID)
void NotifyUpdateUserGestureCarryoverInfo(int render_frame_id) {
  auto task_runner = RenderThreadImpl::DeprecatedGetMainTaskRunner();
  if (!task_runner)
    return;
  if (task_runner->BelongsToCurrentThread()) {
    UpdateUserGestureCarryoverInfo(render_frame_id);
    return;
  }
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(UpdateUserGestureCarryoverInfo, render_frame_id));
}
#endif

void NotifyResourceLoadStarted(
    int render_frame_id,
    int request_id,
    const GURL& response_url,
    const network::ResourceResponseHead& response_head,
    content::ResourceType resource_type) {
  if (response_head.network_accessed) {
    if (resource_type == RESOURCE_TYPE_MAIN_FRAME) {
      UMA_HISTOGRAM_ENUMERATION("Net.ConnectionInfo.MainFrame",
                                response_head.connection_info,
                                net::HttpResponseInfo::NUM_OF_CONNECTION_INFOS);
    } else {
      UMA_HISTOGRAM_ENUMERATION("Net.ConnectionInfo.SubResource",
                                response_head.connection_info,
                                net::HttpResponseInfo::NUM_OF_CONNECTION_INFOS);
    }
  }

  auto task_runner = RenderThreadImpl::DeprecatedGetMainTaskRunner();
  if (!task_runner)
    return;
  if (task_runner->BelongsToCurrentThread()) {
    ResourceLoadStarted(render_frame_id, request_id, response_url,
                        response_head, resource_type);
    return;
  }

  // Make a deep copy of ResourceResponseHead before passing it cross-thread.
  auto resource_response = base::MakeRefCounted<network::ResourceResponse>();
  resource_response->head = response_head;
  auto deep_copied_response = resource_response->DeepCopy();
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(ResourceLoadStarted, render_frame_id, request_id,
                     response_url, deep_copied_response->head, resource_type));
}

void NotifyResourceTransferSizeUpdated(int render_frame_id,
                                       int request_id,
                                       int transfer_size_diff) {
  auto task_runner = RenderThreadImpl::DeprecatedGetMainTaskRunner();
  if (!task_runner)
    return;
  if (task_runner->BelongsToCurrentThread()) {
    ResourceTransferSizeUpdated(render_frame_id, request_id,
                                transfer_size_diff);
    return;
  }
  task_runner->PostTask(
      FROM_HERE, base::BindOnce(ResourceTransferSizeUpdated, render_frame_id,
                                request_id, transfer_size_diff));
}

void NotifyResourceLoadCompleted(
    int render_frame_id,
    mojom::ResourceLoadInfoPtr resource_load_info,
    const network::URLLoaderCompletionStatus& status) {
  RecordLoadHistograms(resource_load_info->url,
                       resource_load_info->resource_type, status.error_code);

  auto task_runner = RenderThreadImpl::DeprecatedGetMainTaskRunner();
  if (!task_runner)
    return;
  if (task_runner->BelongsToCurrentThread()) {
    ResourceLoadCompleted(render_frame_id, std::move(resource_load_info),
                          status);
    return;
  }
  task_runner->PostTask(FROM_HERE,
                        base::BindOnce(ResourceLoadCompleted, render_frame_id,
                                       std::move(resource_load_info), status));
}

void NotifyResourceLoadCanceled(int render_frame_id,
                                int request_id,
                                const GURL& response_url,
                                ResourceType resource_type,
                                int net_error) {
  RecordLoadHistograms(response_url, resource_type, net_error);

  auto task_runner = RenderThreadImpl::DeprecatedGetMainTaskRunner();
  if (!task_runner)
    return;
  if (task_runner->BelongsToCurrentThread()) {
    ResourceLoadCanceled(render_frame_id, request_id);
    return;
  }
  task_runner->PostTask(FROM_HERE, base::BindOnce(ResourceLoadCanceled,
                                                  render_frame_id, request_id));
}

}  // namespace content
