// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_MEDIA_ROUTER_DISCOVERY_MEDIA_SINK_SERVICE_H_
#define CHROME_COMMON_MEDIA_ROUTER_DISCOVERY_MEDIA_SINK_SERVICE_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "chrome/common/media_router/discovery/media_sink_internal.h"
#include "chrome/common/media_router/media_sink.h"

namespace media_router {

// A service which can be used to start background discovery and resolution of
// MediaSinks. Often these are remote devices, like Chromecast. In addition, the
// service is capable of answering MediaSink queries using the sinks that it
// generated.
// This class is not thread safe. All methods must be called from the UI thread.
class MediaSinkService {
 public:
  // Callback to be invoked when this class finishes sink discovering.
  // Arg 0: Sinks discovered and resolved by the service.
  using OnSinksDiscoveredCallback =
      base::Callback<void(std::vector<MediaSinkInternal>)>;

  explicit MediaSinkService(
      const OnSinksDiscoveredCallback& sink_discovery_callback);

  virtual ~MediaSinkService();

  // Starts sink discovery. No-ops if already started.
  // Sinks discovered and resolved are continuously passed to
  // |callback|.
  virtual void Start() = 0;

  // Stops sink discovery. No-ops if already stopped.
  virtual void Stop() = 0;

  // Forces invoking |sink_discovery_callback_| with |sinks|.
  virtual void ForceSinkDiscoveryCallback() = 0;

 protected:
  OnSinksDiscoveredCallback sink_discovery_callback_;

  DISALLOW_COPY_AND_ASSIGN(MediaSinkService);
};

}  // namespace media_router

#endif  // CHROME_COMMON_MEDIA_ROUTER_DISCOVERY_MEDIA_SINK_SERVICE_H_
