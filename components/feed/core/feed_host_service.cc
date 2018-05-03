// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/feed_host_service.h"

#include <utility>

namespace feed {

FeedHostService::FeedHostService(
    std::unique_ptr<FeedNetworkingHost> networking_host)
    : networking_host_(std::move(networking_host)) {}

FeedHostService::~FeedHostService() = default;

FeedNetworkingHost* FeedHostService::GetFeedNetworkingHost() {
  return networking_host_.get();
}

}  // namespace feed
