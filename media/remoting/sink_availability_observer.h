// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_REMOTING_SINK_AVAILABILITY_OBSERVER_H_
#define MEDIA_REMOTING_SINK_AVAILABILITY_OBSERVER_H_

#include "media/mojo/interfaces/remoting.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace media {
namespace remoting {

// Implements the RemotingSource interface for the sole purpose of monitoring
// sink availability status.
class SinkAvailabilityObserver final : public mojom::RemotingSource {
 public:
  SinkAvailabilityObserver(mojom::RemotingSourceRequest source_request,
                           mojom::RemoterPtr remoter);
  ~SinkAvailabilityObserver() override;

  mojom::RemotingSinkCapabilities sink_capabilities() const {
    return sink_capabilities_;
  }

  bool is_remote_decryption_available() const {
    return sink_capabilities_ ==
           mojom::RemotingSinkCapabilities::CONTENT_DECRYPTION_AND_RENDERING;
  }

  // RemotingSource implementations.
  void OnSinkAvailable(mojom::RemotingSinkCapabilities capabilities) override;
  void OnSinkGone() override;
  void OnStarted() override {}
  void OnStartFailed(mojom::RemotingStartFailReason reason) override {}
  void OnMessageFromSink(const std::vector<uint8_t>& message) override {}
  void OnStopped(mojom::RemotingStopReason reason) override {}

 private:
  const mojo::Binding<mojom::RemotingSource> binding_;
  const mojom::RemoterPtr remoter_;

  // When the sink is available, this describes its capabilities. When not
  // available, this is always NONE. Updated by OnSinkAvailable/Gone().
  mojom::RemotingSinkCapabilities sink_capabilities_ =
      mojom::RemotingSinkCapabilities::NONE;

  DISALLOW_COPY_AND_ASSIGN(SinkAvailabilityObserver);
};

}  // namespace remoting
}  // namespace media

#endif  // MEDIA_REMOTING_SINK_AVAILABILITY_OBSERVER_H_
