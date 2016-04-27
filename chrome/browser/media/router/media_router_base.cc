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

// A MediaRoutesObserver that maintains state about the current set of media
// routes.
class MediaRouterBase::InternalMediaRoutesObserver
    : public MediaRoutesObserver {
 public:
  explicit InternalMediaRoutesObserver(MediaRouter* router)
      : MediaRoutesObserver(router), has_local_route(false) {}
  ~InternalMediaRoutesObserver() override {}

  // MediaRoutesObserver
  void OnRoutesUpdated(
      const std::vector<MediaRoute>& routes,
      const std::vector<MediaRoute::Id>& joinable_route_ids) override {
    off_the_record_route_ids.clear();
    has_local_route = false;
    for (const auto& route : routes) {
      has_local_route |= route.is_local();
      if (route.off_the_record())
        off_the_record_route_ids.push_back(route.media_route_id());
    }
  }

  bool has_local_route;
  std::vector<MediaRoute::Id> off_the_record_route_ids;

 private:
  DISALLOW_COPY_AND_ASSIGN(InternalMediaRoutesObserver);
};

MediaRouterBase::~MediaRouterBase() {
  CHECK(!internal_routes_observer_);
}

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
  for (const auto& route_id :
       internal_routes_observer_->off_the_record_route_ids)
    TerminateRoute(route_id);
}

MediaRouterBase::MediaRouterBase() : initialized_(false) {}

// static
std::string MediaRouterBase::CreatePresentationId() {
  return "mr_" + base::GenerateGUID();
}

void MediaRouterBase::NotifyPresentationConnectionStateChange(
    const MediaRoute::Id& route_id,
    content::PresentationConnectionState state) {
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

bool MediaRouterBase::HasLocalRoute() const {
  return internal_routes_observer_->has_local_route;
}

void MediaRouterBase::Initialize() {
  DCHECK(!initialized_);
  // The observer calls virtual methods on MediaRouter; it must be created
  // outside of the ctor
  internal_routes_observer_.reset(new InternalMediaRoutesObserver(this));
  initialized_ = true;
}

void MediaRouterBase::OnPresentationConnectionStateCallbackRemoved(
    const MediaRoute::Id& route_id) {
  auto* callbacks = presentation_connection_state_callbacks_.get(route_id);
  if (callbacks && callbacks->empty())
    presentation_connection_state_callbacks_.erase(route_id);
}

void MediaRouterBase::Shutdown() {
  // The observer calls virtual methods on MediaRouter; it must be destroyed
  // outside of the dtor
  internal_routes_observer_.reset();
}

}  // namespace media_router
