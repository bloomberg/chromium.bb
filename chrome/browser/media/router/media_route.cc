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
      for_display_(for_display) {}

MediaRoute::~MediaRoute() {
}

bool MediaRoute::Equals(const MediaRoute& other) const {
  return media_route_id_ == other.media_route_id_;
}

MediaRouteIdToPresentationSessionMapping::
    MediaRouteIdToPresentationSessionMapping() {
}

MediaRouteIdToPresentationSessionMapping::
    ~MediaRouteIdToPresentationSessionMapping() {
}

void MediaRouteIdToPresentationSessionMapping::Add(
    const MediaRoute::Id& route_id,
    const content::PresentationSessionInfo& session_info) {
  route_id_to_presentation_.insert(std::make_pair(route_id, session_info));
}

void MediaRouteIdToPresentationSessionMapping::Remove(
    const MediaRoute::Id& route_id) {
  route_id_to_presentation_.erase(route_id);
}

void MediaRouteIdToPresentationSessionMapping::Clear() {
  route_id_to_presentation_.clear();
}

const content::PresentationSessionInfo*
MediaRouteIdToPresentationSessionMapping::Get(
    const MediaRoute::Id& route_id) const {
  auto it = route_id_to_presentation_.find(route_id);
  return it == route_id_to_presentation_.end() ? nullptr : &it->second;
}

}  // namespace media_router
