// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/media_router_base.h"

#include "base/bind.h"
#include "base/guid.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"

namespace media_router {

MediaRouterBase::MediaRouterBase() = default;

MediaRouterBase::~MediaRouterBase() = default;

std::unique_ptr<PresentationConnectionStateSubscription>
MediaRouterBase::AddPresentationConnectionStateChangedCallback(
    const MediaRoute::Id& route_id,
    const content::PresentationConnectionStateChangedCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  auto* callbacks = presentation_connection_state_callbacks_.get(route_id);
  if (!callbacks) {
    callbacks = new PresentationConnectionStateChangedCallbacks;
    callbacks->set_removal_callback(base::Bind(
        &MediaRouterBase::OnPresentationConnectionStateCallbackRemoved,
        base::Unretained(this), route_id));
    presentation_connection_state_callbacks_.add(route_id,
                                                 base::WrapUnique(callbacks));
  }

  return callbacks->Add(callback);
}

void MediaRouterBase::OnOffTheRecordProfileShutdown() {
  // TODO(mfoltz): There is a race condition where off-the-record routes created
  // by pending CreateRoute requests won't be terminated.  Fixing this would
  // extra bookeeping of route requests in progress, and a way to cancel them
  // in-flight.
  for (auto route_ids_it = off_the_record_route_ids_.begin();
       route_ids_it != off_the_record_route_ids_.end();
       /* no-op */) {
    // TerminateRoute will erase |route_id| from |off_the_record_route_ids_|,
    // make a copy as the iterator will be invalidated.
    const MediaRoute::Id route_id = *route_ids_it++;
    TerminateRoute(route_id);
  }
}

// static
std::string MediaRouterBase::CreatePresentationId() {
  return "mr_" + base::GenerateGUID();
}

void MediaRouterBase::NotifyPresentationConnectionStateChange(
    const MediaRoute::Id& route_id,
    content::PresentationConnectionState state) {
  if (state == content::PRESENTATION_CONNECTION_STATE_TERMINATED)
    OnRouteTerminated(route_id);

  auto* callbacks = presentation_connection_state_callbacks_.get(route_id);
  if (!callbacks)
    return;

  callbacks->Notify(content::PresentationConnectionStateChangeInfo(state));
}

void MediaRouterBase::NotifyPresentationConnectionClose(
    const MediaRoute::Id& route_id,
    content::PresentationConnectionCloseReason reason,
    const std::string& message) {
  auto* callbacks = presentation_connection_state_callbacks_.get(route_id);
  if (!callbacks)
    return;

  content::PresentationConnectionStateChangeInfo info(
      content::PRESENTATION_CONNECTION_STATE_CLOSED);
  info.close_reason = reason;
  info.message = message;
  callbacks->Notify(info);
}

void MediaRouterBase::OnOffTheRecordRouteCreated(
    const MediaRoute::Id& route_id) {
  DCHECK(!ContainsKey(off_the_record_route_ids_, route_id));
  off_the_record_route_ids_.insert(route_id);
}

void MediaRouterBase::OnRouteTerminated(const MediaRoute::Id& route_id) {
  // NOTE: This is called for all routes (off the record or not).
  off_the_record_route_ids_.erase(route_id);
}

void MediaRouterBase::OnPresentationConnectionStateCallbackRemoved(
    const MediaRoute::Id& route_id) {
  auto* callbacks = presentation_connection_state_callbacks_.get(route_id);
  if (callbacks && callbacks->empty())
    presentation_connection_state_callbacks_.erase(route_id);
}

}  // namespace media_router
