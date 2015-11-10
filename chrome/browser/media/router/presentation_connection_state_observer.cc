// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/presentation_connection_state_observer.h"

#include "base/logging.h"
#include "chrome/browser/media/router/media_router.h"

namespace media_router {

PresentationConnectionStateObserver::PresentationConnectionStateObserver(
    MediaRouter* router,
    const MediaRoute::Id& route_id)
    : router_(router), route_id_(route_id) {
  DCHECK(router_);
  router_->RegisterPresentationConnectionStateObserver(this);
}

PresentationConnectionStateObserver::~PresentationConnectionStateObserver() {
  router_->UnregisterPresentationConnectionStateObserver(this);
}

}  // namespace media_router
