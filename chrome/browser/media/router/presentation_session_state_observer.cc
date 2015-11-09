// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/presentation_session_state_observer.h"

#include "chrome/browser/media/router/media_router.h"

namespace media_router {

PresentationSessionStateObserver::PresentationSessionStateObserver(
    const content::SessionStateChangedCallback& state_changed_callback,
    const MediaRouteIdToPresentationSessionMapping* route_id_to_presentation,
    MediaRouter* router)
    : MediaRoutesObserver(router),
      state_changed_callback_(state_changed_callback),
      route_id_to_presentation_(route_id_to_presentation) {
  DCHECK(route_id_to_presentation_);
}

PresentationSessionStateObserver::~PresentationSessionStateObserver() {
}

void PresentationSessionStateObserver::OnPresentationSessionConnected(
    const MediaRoute::Id& route_id) {
  if (!ContainsValue(tracked_route_ids_, route_id))
    tracked_route_ids_.push_back(route_id);
  InvokeCallback(route_id, content::PRESENTATION_CONNECTION_STATE_CONNECTED);
}

void PresentationSessionStateObserver::Reset() {
  tracked_route_ids_.clear();
  previous_route_ids_.clear();
}

void PresentationSessionStateObserver::OnRoutesUpdated(
    const std::vector<MediaRoute>& routes) {
  std::vector<MediaRoute::Id> current_route_ids_;
  current_route_ids_.reserve(routes.size());
  for (const MediaRoute& route : routes)
    current_route_ids_.push_back(route.media_route_id());

  for (auto it = tracked_route_ids_.begin(); it != tracked_route_ids_.end();
       /*no-op*/) {
    const MediaRoute::Id& route_id = *it;
    if (ContainsValue(previous_route_ids_, route_id) &&
        !ContainsValue(current_route_ids_, route_id)) {
      InvokeCallback(route_id, content::PRESENTATION_CONNECTION_STATE_CLOSED);
      it = tracked_route_ids_.erase(it);
    } else {
      ++it;
    }
  }
  previous_route_ids_.swap(current_route_ids_);
}

void PresentationSessionStateObserver::InvokeCallback(
    const MediaRoute::Id& route_id,
    content::PresentationConnectionState new_state) {
  const content::PresentationSessionInfo* session_info =
      route_id_to_presentation_->Get(route_id);
  if (session_info)
    state_changed_callback_.Run(*session_info, new_state);
}

}  // namespace media_router
