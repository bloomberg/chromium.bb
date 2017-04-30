// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/media_router/discovery/media_sink_service.h"

namespace media_router {

MediaSinkService::MediaSinkService(
    const OnSinksDiscoveredCallback& sink_discovery_callback)
    : sink_discovery_callback_(sink_discovery_callback) {}

MediaSinkService::~MediaSinkService() = default;

}  // namespace media_router
