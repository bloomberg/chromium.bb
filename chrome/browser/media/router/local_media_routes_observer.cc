// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/local_media_routes_observer.h"

#include "chrome/browser/media/router/media_router.h"

namespace media_router {

LocalMediaRoutesObserver::LocalMediaRoutesObserver(MediaRouter* router)
    : router_(router) {
  DCHECK(router_);
  router_->RegisterLocalMediaRoutesObserver(this);
}

LocalMediaRoutesObserver::~LocalMediaRoutesObserver() {
  router_->UnregisterLocalMediaRoutesObserver(this);
}

}  // namespace media_router
