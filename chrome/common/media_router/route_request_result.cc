// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/media_router/route_request_result.h"

#include "chrome/common/media_router/media_route.h"

namespace media_router {

// static
std::unique_ptr<RouteRequestResult> RouteRequestResult::FromSuccess(
    const MediaRoute& route,
    const std::string& presentation_id) {
  return std::make_unique<RouteRequestResult>(
      std::make_unique<MediaRoute>(route), presentation_id, std::string(),
      RouteRequestResult::OK);
}

// static
std::unique_ptr<RouteRequestResult> RouteRequestResult::FromError(
    const std::string& error,
    ResultCode result_code) {
  return std::make_unique<RouteRequestResult>(nullptr, std::string(), error,
                                              result_code);
}

RouteRequestResult::RouteRequestResult(std::unique_ptr<MediaRoute> route,
                                       const std::string& presentation_id,
                                       const std::string& error,
                                       ResultCode result_code)
    : route_(std::move(route)),
      presentation_id_(presentation_id),
      error_(error),
      result_code_(result_code) {
  if (route_)
    presentation_url_ = route_->media_source().url();
}

RouteRequestResult::~RouteRequestResult() = default;

}  // namespace media_router
