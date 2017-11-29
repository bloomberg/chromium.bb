// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/host/forwarding_compositing_mode_reporter_impl.h"

namespace viz {

ForwardingCompositingModeReporterImpl::ForwardingCompositingModeReporterImpl()
    : forwarding_source_binding_(this) {}

ForwardingCompositingModeReporterImpl::
    ~ForwardingCompositingModeReporterImpl() = default;

mojom::CompositingModeWatcherPtr
ForwardingCompositingModeReporterImpl::BindAsWatcher() {
  if (forwarding_source_binding_.is_bound())
    forwarding_source_binding_.Unbind();
  mojom::CompositingModeWatcherPtr ptr;
  forwarding_source_binding_.Bind(mojo::MakeRequest(&ptr));
  return ptr;
}

void ForwardingCompositingModeReporterImpl::BindRequest(
    mojom::CompositingModeReporterRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void ForwardingCompositingModeReporterImpl::AddCompositingModeWatcher(
    mojom::CompositingModeWatcherPtr watcher) {
  if (!gpu_)
    watcher->CompositingModeFallbackToSoftware();
  watchers_.AddPtr(std::move(watcher));
}

void ForwardingCompositingModeReporterImpl::
    CompositingModeFallbackToSoftware() {
  // For an authoritative reporter, this mode can only change once, but the
  // forwarder may get re-attached to a new reporter which claims to fallback to
  // software. This class should only notify its own observers a single time.
  if (!gpu_)
    return;
  gpu_ = false;
  watchers_.ForAllPtrs([](mojom::CompositingModeWatcher* watcher) {
    watcher->CompositingModeFallbackToSoftware();
  });
}

}  // namespace viz
