// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/base/latency_info_swap_promise_monitor.h"

#include "cc/base/latency_info_swap_promise.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_host_impl.h"
#include "cc/trees/layer_tree_impl.h"

namespace {

bool AddRenderingScheduledComponent(ui::LatencyInfo* latency_info) {
  if (latency_info->FindLatency(
          ui::INPUT_EVENT_LATENCY_RENDERING_SCHEDULED_COMPONENT, 0, 0))
    return false;
  latency_info->AddLatencyNumber(
      ui::INPUT_EVENT_LATENCY_RENDERING_SCHEDULED_COMPONENT, 0, 0);
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
  if (AddRenderingScheduledComponent(latency_)) {
    scoped_ptr<SwapPromise> swap_promise(new LatencyInfoSwapPromise(*latency_));
    layer_tree_host_impl_->QueueSwapPromiseForMainThreadScrollUpdate(
        swap_promise.Pass());
  }
}

}  // namespace cc
