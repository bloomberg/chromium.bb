// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/media_router_impl.h"

#include <set>

#include "base/bind.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "chrome/browser/media/router/media_route.h"
#include "chrome/browser/media/router/media_routes_observer.h"
#include "chrome/browser/media/router/media_sink.h"
#include "chrome/browser/media/router/media_sinks_observer.h"
#include "chrome/browser/media/router/media_source.h"
#include "chrome/browser/media/router/media_source_helper.h"

namespace media_router {

MediaRouterImpl::MediaRouterImpl() : provider_manager_host_(nullptr) {
}

MediaRouterImpl::~MediaRouterImpl() {
}

void MediaRouterImpl::Initialize(MediaRouter* provider_manager_host) {
  DCHECK(!provider_manager_host_);
  DCHECK(provider_manager_host);
  provider_manager_host_ = provider_manager_host;
}

// TODO(imcheng): Implement NOTIMPLEMENTED() methods.
void MediaRouterImpl::RequestRoute(const MediaSourceId& source,
                                   const MediaSinkId& sink_id,
                                   const MediaRouteResponseCallback& callback) {
  NOTIMPLEMENTED();
}

void MediaRouterImpl::CloseRoute(const MediaRouteId& route_id) {
  NOTIMPLEMENTED();
}

void MediaRouterImpl::PostMessage(const MediaRouteId& route_id,
                                  const std::string& message) {
  NOTIMPLEMENTED();
}

bool MediaRouterImpl::RegisterMediaSinksObserver(MediaSinksObserver* observer) {
  NOTIMPLEMENTED();
  return false;
}

void MediaRouterImpl::UnregisterMediaSinksObserver(
    MediaSinksObserver* observer) {
  NOTIMPLEMENTED();
}

bool MediaRouterImpl::RegisterMediaRoutesObserver(
    MediaRoutesObserver* observer) {
  NOTIMPLEMENTED();
  return false;
}

void MediaRouterImpl::UnregisterMediaRoutesObserver(
    MediaRoutesObserver* observer) {
  NOTIMPLEMENTED();
}

}  // namespace media_router
