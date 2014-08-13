// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/base/latency_info_swap_promise_monitor.h"

#include "base/threading/platform_thread.h"
#include "cc/base/latency_info_swap_promise.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_host_impl.h"
#include "cc/trees/layer_tree_impl.h"

namespace {

bool AddRenderingScheduledComponent(ui::LatencyInfo* latency_info) {
  if (latency_info->FindLatency(
          ui::INPUT_EVENT_LATENCY_RENDERING_SCHEDULED_COMPONENT, 0, NULL))
    return false;
  latency_info->AddLatencyNumber(
      ui::INPUT_EVENT_LATENCY_RENDERING_SCHEDULED_COMPONENT, 0, 0);
  return true;
}

bool AddForwardingScrollUpdateToMainComponent(ui::LatencyInfo* latency_info) {
  if (latency_info->FindLatency(
          ui::INPUT_EVENT_LATENCY_FORWARD_SCROLL_UPDATE_TO_MAIN_COMPONENT,
          0,
          NULL))
    return false;
  latency_info->AddLatencyNumber(
      ui::INPUT_EVENT_LATENCY_FORWARD_SCROLL_UPDATE_TO_MAIN_COMPONENT,
      0,
      latency_info->trace_id);
  return true;
}

}  // namespace

namespace cc {

LatencyInfoSwapPromiseMonitor::LatencyInfoSwapPromiseMonitor(
    ui::LatencyInfo* latency,
    LayerTreeHost* layer_tree_host,
    LayerTreeHostImpl* layer_tree_host_impl)
    : SwapPromiseMonitor(layer_tree_host, layer_tree_host_impl),
      latency_(latency) {}

LatencyInfoSwapPromiseMonitor::~LatencyInfoSwapPromiseMonitor() {}

void LatencyInfoSwapPromiseMonitor::OnSetNeedsCommitOnMain() {
  if (AddRenderingScheduledComponent(latency_)) {
    scoped_ptr<SwapPromise> swap_promise(new LatencyInfoSwapPromise(*latency_));
    layer_tree_host_->QueueSwapPromise(swap_promise.Pass());
  }
}

void LatencyInfoSwapPromiseMonitor::OnSetNeedsRedrawOnImpl() {
  if (AddRenderingScheduledComponent(latency_)) {
    scoped_ptr<SwapPromise> swap_promise(new LatencyInfoSwapPromise(*latency_));
    layer_tree_host_impl_->active_tree()->QueueSwapPromise(swap_promise.Pass());
  }
}

void LatencyInfoSwapPromiseMonitor::OnForwardScrollUpdateToMainThreadOnImpl() {
  if (AddForwardingScrollUpdateToMainComponent(latency_)) {
    int64 new_sequence_number = 0;
    for (ui::LatencyInfo::LatencyMap::const_iterator it =
             latency_->latency_components.begin();
         it != latency_->latency_components.end();
         ++it) {
      if (it->first.first == ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT) {
        new_sequence_number =
            (static_cast<int64>(base::PlatformThread::CurrentId()) << 32) |
            (it->second.sequence_number & 0xffffffff);
        DCHECK(new_sequence_number != it->second.sequence_number);
        break;
      }
    }
    if (!new_sequence_number)
      return;
    ui::LatencyInfo new_latency;
    new_latency.AddLatencyNumber(
        ui::INPUT_EVENT_LATENCY_BEGIN_SCROLL_UPDATE_MAIN_COMPONENT,
        0,
        new_sequence_number);
    new_latency.TraceEventType("ScrollUpdate");
    new_latency.CopyLatencyFrom(
        *latency_,
        ui::INPUT_EVENT_LATENCY_FORWARD_SCROLL_UPDATE_TO_MAIN_COMPONENT);
    scoped_ptr<SwapPromise> swap_promise(
        new LatencyInfoSwapPromise(new_latency));
    layer_tree_host_impl_->QueueSwapPromiseForMainThreadScrollUpdate(
        swap_promise.Pass());
  }
}

}  // namespace cc
