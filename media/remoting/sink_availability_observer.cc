// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/remoting/sink_availability_observer.h"

namespace media {
namespace remoting {

SinkAvailabilityObserver::SinkAvailabilityObserver(
    mojom::RemotingSourceRequest source_request,
    mojom::RemoterPtr remoter)
    : binding_(this, std::move(source_request)), remoter_(std::move(remoter)) {}

SinkAvailabilityObserver::~SinkAvailabilityObserver() {}

void SinkAvailabilityObserver::OnSinkAvailable(
    mojom::RemotingSinkCapabilities capabilities) {
  sink_capabilities_ = capabilities;
}

void SinkAvailabilityObserver::OnSinkGone() {
  sink_capabilities_ = mojom::RemotingSinkCapabilities::NONE;
}

}  // namespace remoting
}  // namespace media
