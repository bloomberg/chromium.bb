// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_MEDIA_SINK_SERVICE_H_
#define CHROME_BROWSER_MEDIA_ROUTER_MEDIA_SINK_SERVICE_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "chrome/browser/media/router/media_sink.h"

namespace media_router {

class MediaSinksObserver;

// A service which can be used to start background discovery and resolution of
// MediaSinks. Often these are remote devices, like Chromecast. In addition, the
// service is capable of answering MediaSink queries using the sinks that it
// generated.
// This class is not thread safe. All methods must be called from the IO thread.
class MediaSinkService {
 public:
  // Callback to be invoked when this class finishes sink discovering.
  // Arg 0: Sinks discovered and resolved by the service.
  using OnSinksDiscoveredCallback =
      base::Callback<void(const std::vector<MediaSink>&)>;

  explicit MediaSinkService(
      const OnSinksDiscoveredCallback& sinks_discovered_callback);

  virtual ~MediaSinkService();

  // Starts sink discovery. No-ops if already started.
  // Sinks discovered and resolved are continuously passed to
  // |callback|.
  virtual void Start() = 0;

  // Adds a sink query to observe for MediaSink updates.
  // Multiple observers can be added for a given MediaSource.
  // Start() must be called first. This class does not take
  // ownership of |observer|.
  virtual void AddSinkQuery(MediaSinksObserver* observer) = 0;

  // Removes a sink query and stops observing MediaSink updates. No-op if
  // |observer| is not registered with this class.
  virtual void RemoveSinkQuery(MediaSinksObserver* observer) = 0;

 protected:
  OnSinksDiscoveredCallback sinks_discovered_callback_;

  DISALLOW_COPY_AND_ASSIGN(MediaSinkService);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_MEDIA_SINK_SERVICE_H_
