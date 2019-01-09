// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_LOADER_RESOURCE_LOAD_STATS_H_
#define CONTENT_RENDERER_LOADER_RESOURCE_LOAD_STATS_H_

#include "build/build_config.h"
#include "content/public/common/resource_load_info.mojom.h"
#include "content/public/common/resource_type.h"

class GURL;

namespace network {
struct ResourceResponseHead;
struct URLLoaderCompletionStatus;
}  // namespace network

namespace content {

// A collection of methods collecting histograms related to resource load
// and notifying browser process with loading stats.

#if defined(OS_ANDROID)
void NotifyUpdateUserGestureCarryoverInfo(int render_frame_id);
#endif

void NotifyResourceLoadStarted(
    int render_frame_id,
    int request_id,
    const GURL& response_url,
    const network::ResourceResponseHead& response_head,
    ResourceType resource_type);

void NotifyResourceTransferSizeUpdated(int render_frame_id,
                                       int request_id,
                                       int transfer_size_diff);

void NotifyResourceLoadCompleted(
    int render_frame_id,
    mojom::ResourceLoadInfoPtr resource_load_info,
    const network::URLLoaderCompletionStatus& status);

void NotifyResourceLoadCanceled(int render_frame_id,
                                int request_id,
                                const GURL& response_url,
                                ResourceType resource_type,
                                int net_error);

}  // namespace content

#endif  // CONTENT_RENDERER_LOADER_RESOURCE_LOAD_STATS_H_
