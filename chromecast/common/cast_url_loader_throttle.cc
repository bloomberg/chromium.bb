// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/common/cast_url_loader_throttle.h"

#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/resource_response.h"

namespace chromecast {

CastURLLoaderThrottle::CastURLLoaderThrottle() = default;

CastURLLoaderThrottle::~CastURLLoaderThrottle() = default;

void CastURLLoaderThrottle::DetachFromCurrentSequence() {}

void CastURLLoaderThrottle::WillStartRequest(
    network::ResourceRequest* /* request */,
    bool* defer) {}

}  // namespace chromecast
