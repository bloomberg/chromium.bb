// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/media_routes_observer.h"

#include "base/logging.h"
#include "chrome/browser/media/router/media_router.h"

namespace media_router {

MediaRoutesObserver::MediaRoutesObserver(MediaRouter* router)
    : router_(router) {
  DCHECK(router_);
  if (!router_->RegisterMediaRoutesObserver(this))
    LOG(ERROR) << "RegisterMediaRoutesObserver failed.";
}

MediaRoutesObserver::~MediaRoutesObserver() {
  router_->UnregisterMediaRoutesObserver(this);
}

}  // namespace media_router
