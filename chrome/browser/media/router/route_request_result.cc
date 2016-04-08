// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/route_request_result.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/media/router/media_route.h"

namespace media_router {

// static
std::unique_ptr<RouteRequestResult> RouteRequestResult::FromSuccess(
    std::unique_ptr<MediaRoute> route,
    const std::string& presentation_id) {
  DCHECK(route);
  return base::WrapUnique(new RouteRequestResult(
      std::move(route), presentation_id, "", RouteRequestResult::OK));
}

// static
std::unique_ptr<RouteRequestResult> RouteRequestResult::FromError(
    const std::string& error,
    ResultCode result_code) {
  return base::WrapUnique(
      new RouteRequestResult(nullptr, "", error, result_code));
}

RouteRequestResult::RouteRequestResult(std::unique_ptr<MediaRoute> route,
                                       const std::string& presentation_id,
                                       const std::string& error,
                                       ResultCode result_code)
    : route_(std::move(route)),
      presentation_id_(presentation_id),
      error_(error),
      result_code_(result_code) {}

RouteRequestResult::~RouteRequestResult() = default;

}  // namespace media_router
