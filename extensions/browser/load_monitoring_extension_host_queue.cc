// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/load_monitoring_extension_host_queue.h"

#include <algorithm>

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram_macros.h"
#include "content/public/browser/render_frame_host.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_host_observer.h"
#include "extensions/browser/serial_extension_host_queue.h"

namespace extensions {

LoadMonitoringExtensionHostQueue::LoadMonitoringExtensionHostQueue(
    scoped_ptr<ExtensionHostQueue> delegate,
    base::TimeDelta monitor_time,
    const FinishedCallback& finished_callback)
    : delegate_(delegate.Pass()),
      monitor_time_(monitor_time),
      finished_callback_(finished_callback),
      started_(false),
      num_queued_(0u),
      num_loaded_(0u),
      max_in_queue_(0u),
      max_active_loading_(0u),
      weak_ptr_factory_(this) {
}

LoadMonitoringExtensionHostQueue::LoadMonitoringExtensionHostQueue(
    scoped_ptr<ExtensionHostQueue> delegate)
    : LoadMonitoringExtensionHostQueue(delegate.Pass(),
                                       base::TimeDelta::FromMinutes(1),
                                       FinishedCallback()) {
}

LoadMonitoringExtensionHostQueue::~LoadMonitoringExtensionHostQueue() {
}

void LoadMonitoringExtensionHostQueue::StartMonitoring() {
  if (started_) {
    return;
  }
  started_ = true;
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE, base::Bind(&LoadMonitoringExtensionHostQueue::FinishMonitoring,
                            weak_ptr_factory_.GetWeakPtr()),
      monitor_time_);
}

void LoadMonitoringExtensionHostQueue::Add(DeferredStartRenderHost* host) {
  StartMonitoring();
  delegate_->Add(host);
  if (in_queue_.insert(host).second) {
    ++num_queued_;
    max_in_queue_ = std::max(max_in_queue_, in_queue_.size());
    host->AddDeferredStartRenderHostObserver(this);
  }
}

void LoadMonitoringExtensionHostQueue::Remove(DeferredStartRenderHost* host) {
  delegate_->Remove(host);
  RemoveFromQueue(host);
}

void LoadMonitoringExtensionHostQueue::OnDeferredStartRenderHostDidStartLoading(
    const DeferredStartRenderHost* host) {
  StartMonitoringHost(host);
}

void LoadMonitoringExtensionHostQueue::OnDeferredStartRenderHostDidStopLoading(
    const DeferredStartRenderHost* host) {
  FinishMonitoringHost(host);
}

void LoadMonitoringExtensionHostQueue::OnDeferredStartRenderHostDestroyed(
    const DeferredStartRenderHost* host) {
  FinishMonitoringHost(host);
}

void LoadMonitoringExtensionHostQueue::StartMonitoringHost(
    const DeferredStartRenderHost* host) {
  if (active_loading_.insert(host).second) {
    max_active_loading_ = std::max(max_active_loading_, active_loading_.size());
  }
  RemoveFromQueue(host);
}

void LoadMonitoringExtensionHostQueue::FinishMonitoringHost(
    const DeferredStartRenderHost* host) {
  if (active_loading_.erase(host)) {
    ++num_loaded_;
  }
}

void LoadMonitoringExtensionHostQueue::RemoveFromQueue(
    const DeferredStartRenderHost* const_host) {
  // This odd code is needed because StartMonitoringHost() gives us a const
  // host, but we need a non-const one for
  // RemoveDeferredStartRenderHostObserver().
  for (DeferredStartRenderHost* host : in_queue_) {
    if (host == const_host) {
      host->RemoveDeferredStartRenderHostObserver(this);
      in_queue_.erase(host);  // uhoh, iterator invalidated!
      break;
    }
  }
}

void LoadMonitoringExtensionHostQueue::FinishMonitoring() {
  CHECK(started_);
  UMA_HISTOGRAM_COUNTS_100("Extensions.ExtensionHostMonitoring.NumQueued",
                           num_queued_);
  UMA_HISTOGRAM_COUNTS_100("Extensions.ExtensionHostMonitoring.NumLoaded",
                           num_loaded_);
  UMA_HISTOGRAM_COUNTS_100("Extensions.ExtensionHostMonitoring.MaxInQueue",
                           max_in_queue_);
  UMA_HISTOGRAM_COUNTS_100(
      "Extensions.ExtensionHostMonitoring.MaxActiveLoading",
      max_active_loading_);
  if (!finished_callback_.is_null()) {
    finished_callback_.Run(num_queued_, num_loaded_, max_in_queue_,
                           max_active_loading_);
  }
}

}  // namespace extensions
