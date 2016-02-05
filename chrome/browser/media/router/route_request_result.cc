// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/route_request_result.h"

#include "chrome/browser/media/router/media_route.h"

namespace media_router {

// static
scoped_ptr<RouteRequestResult> RouteRequestResult::FromSuccess(
    scoped_ptr<MediaRoute> route,
    const std::string& presentation_id) {
  DCHECK(route);
  return make_scoped_ptr(new RouteRequestResult(
      std::move(route), presentation_id, "", RouteRequestResult::OK));
}

// static
scoped_ptr<RouteRequestResult> RouteRequestResult::FromError(
    const std::string& error,
    ResultCode result_code) {
  return make_scoped_ptr(
      new RouteRequestResult(nullptr, "", error, result_code));
}

RouteRequestResult::RouteRequestResult(scoped_ptr<MediaRoute> route,
                                       const std::string& presentation_id,
                                       const std::string& error,
                                       ResultCode result_code)
    : route_(std::move(route)),
      presentation_id_(presentation_id),
      error_(error),
      result_code_(result_code) {}

RouteRequestResult::~RouteRequestResult() = default;

}  // namespace media_router
