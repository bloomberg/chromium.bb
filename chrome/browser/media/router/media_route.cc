// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/media_route.h"

#include "base/logging.h"
#include "chrome/browser/media/router/media_source.h"

namespace media_router {

MediaRoute::MediaRoute(const MediaRoute::Id& media_route_id,
                       const MediaSource& media_source,
                       const MediaSink::Id& media_sink_id,
                       const std::string& description,
                       bool is_local,
                       const std::string& custom_controller_path,
                       bool for_display)
    : media_route_id_(media_route_id),
      media_source_(media_source),
      media_sink_id_(media_sink_id),
      description_(description),
      is_local_(is_local),
      custom_controller_path_(custom_controller_path),
      for_display_(for_display),
      is_incognito_(false),
      is_offscreen_presentation_(false) {}

MediaRoute::MediaRoute(const MediaRoute& other) = default;

MediaRoute::MediaRoute() {}
MediaRoute::~MediaRoute() = default;

bool MediaRoute::Equals(const MediaRoute& other) const {
  return media_route_id_ == other.media_route_id_;
}

}  // namespace media_router
