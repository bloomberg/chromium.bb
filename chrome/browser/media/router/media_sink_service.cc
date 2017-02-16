// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/media_sink_service.h"

namespace media_router {

MediaSinkService::MediaSinkService(
    const OnSinksDiscoveredCallback& sinks_discovered_callback)
    : sinks_discovered_callback_(sinks_discovered_callback) {}

MediaSinkService::~MediaSinkService() = default;

}  // namespace media_router
