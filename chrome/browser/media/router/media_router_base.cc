// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/media_router_base.h"

#include "base/bind.h"

namespace media_router {

MediaRouterBase::MediaRouterBase() = default;

MediaRouterBase::~MediaRouterBase() = default;

scoped_ptr<PresentationConnectionStateSubscription>
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
                                                 make_scoped_ptr(callbacks));
  }

  return callbacks->Add(callback);
}

void MediaRouterBase::NotifyPresentationConnectionStateChange(
    const MediaRoute::Id& route_id,
    content::PresentationConnectionState state) {
  auto* callbacks = presentation_connection_state_callbacks_.get(route_id);
  if (!callbacks)
    return;

  callbacks->Notify(state);
}

void MediaRouterBase::OnPresentationConnectionStateCallbackRemoved(
    const MediaRoute::Id& route_id) {
  auto* callbacks = presentation_connection_state_callbacks_.get(route_id);
  if (callbacks && callbacks->empty())
    presentation_connection_state_callbacks_.erase(route_id);
}

}  // namespace media_router
