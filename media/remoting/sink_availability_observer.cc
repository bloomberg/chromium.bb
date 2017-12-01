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

SinkAvailabilityObserver::~SinkAvailabilityObserver() = default;

bool SinkAvailabilityObserver::IsRemoteDecryptionAvailable() const {
  return std::find(std::begin(sink_metadata_.features),
                   std::end(sink_metadata_.features),
                   mojom::RemotingSinkFeature::CONTENT_DECRYPTION) !=
         std::end(sink_metadata_.features);
}

void SinkAvailabilityObserver::OnSinkAvailable(
    mojom::RemotingSinkMetadataPtr metadata) {
  sink_metadata_ = *metadata;
}

void SinkAvailabilityObserver::OnSinkGone() {
  sink_metadata_ = mojom::RemotingSinkMetadata();
}

}  // namespace remoting
}  // namespace media
