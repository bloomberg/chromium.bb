// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/media_route.h"

#include "base/logging.h"
#include "chrome/browser/media/router/media_source.h"

namespace media_router {

MediaRoute::MediaRoute(const MediaRouteId& media_route_id,
                       const MediaSource& media_source,
                       const MediaSink& media_sink,
                       const std::string& description,
                       bool is_local)
    : media_route_id_(media_route_id),
      media_source_(media_source),
      media_sink_(media_sink),
      description_(description),
      is_local_(is_local),
      state_(MEDIA_ROUTE_STATE_NEW) {
}

MediaRoute::~MediaRoute() {
}

bool MediaRoute::Equals(const MediaRoute& other) const {
  return media_route_id_ == other.media_route_id_;
}

}  // namespace media_router
