// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/output_surface.h"

#include <stdint.h>

#include "base/bind.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "components/viz/service/display/output_surface_client.h"
#include "components/viz/service/display/output_surface_frame.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "ui/gfx/swap_result.h"

namespace viz {

OutputSurface::OutputSurface(scoped_refptr<ContextProvider> context_provider)
    : context_provider_(std::move(context_provider)) {
  DCHECK(context_provider_);
}

OutputSurface::OutputSurface(
    std::unique_ptr<SoftwareOutputDevice> software_device)
    : software_device_(std::move(software_device)) {
  DCHECK(software_device_);
}

OutputSurface::OutputSurface(
    scoped_refptr<VulkanContextProvider> vulkan_context_provider)
    : vulkan_context_provider_(std::move(vulkan_context_provider)) {
  DCHECK(vulkan_context_provider_);
}

OutputSurface::~OutputSurface() = default;

OutputSurface::LatencyInfoCache::SwapInfo::SwapInfo(
    uint64_t id,
    std::vector<ui::LatencyInfo> info)
    : swap_id(id), latency_info(std::move(info)) {}

OutputSurface::LatencyInfoCache::SwapInfo::SwapInfo(SwapInfo&& src) = default;

OutputSurface::LatencyInfoCache::SwapInfo&
OutputSurface::LatencyInfoCache::SwapInfo::operator=(SwapInfo&& src) = default;

OutputSurface::LatencyInfoCache::SwapInfo::~SwapInfo() = default;

OutputSurface::LatencyInfoCache::LatencyInfoCache(Client* client)
    : client_(client) {
  DCHECK(client);
}

OutputSurface::LatencyInfoCache::~LatencyInfoCache() = default;

bool OutputSurface::LatencyInfoCache::WillSwap(
    std::vector<ui::LatencyInfo> latency_info) {
  bool snapshot_requested = false;
  for (const auto& latency : latency_info) {
    if (latency.FindLatency(ui::BROWSER_SNAPSHOT_FRAME_NUMBER_COMPONENT,
                            nullptr)) {
      snapshot_requested = true;
      break;
    }
  }

  // Don't grow unbounded in case of error.
  while (swap_infos_.size() >= kCacheCountMax) {
    client_->LatencyInfoCompleted(swap_infos_.front().latency_info);
    swap_infos_.pop_front();
  }
  swap_infos_.emplace_back(swap_id_++, std::move(latency_info));

  return snapshot_requested;
}

void OutputSurface::LatencyInfoCache::OnSwapBuffersCompleted(
    const gfx::SwapResponse& response) {
  auto it = std::find_if(
      swap_infos_.begin(), swap_infos_.end(),
      [&](const SwapInfo& si) { return si.swap_id == response.swap_id; });

  if (it != swap_infos_.end()) {
    for (auto& latency : it->latency_info) {
      latency.AddLatencyNumberWithTimestamp(
          ui::INPUT_EVENT_GPU_SWAP_BUFFER_COMPONENT, 0, 0, response.swap_start,
          1);
      latency.AddLatencyNumberWithTimestamp(
          ui::INPUT_EVENT_LATENCY_TERMINATED_FRAME_SWAP_COMPONENT, 0, 0,
          response.swap_end, 1);
    }
    client_->LatencyInfoCompleted(it->latency_info);
    swap_infos_.erase(it);
  }
}

}  // namespace viz
