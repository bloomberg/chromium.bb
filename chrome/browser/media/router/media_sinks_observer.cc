// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/media_sinks_observer.h"

#include "base/logging.h"
#include "chrome/browser/media/router/media_router.h"

namespace media_router {

MediaSinksObserver::MediaSinksObserver(MediaRouter* router,
                                       const MediaSource& source)
    : source_(source), router_(router), initialized_(false) {
  DCHECK(router_);
}

MediaSinksObserver::~MediaSinksObserver() {
  if (initialized_)
    router_->UnregisterMediaSinksObserver(this);
}

bool MediaSinksObserver::Init() {
  if (initialized_)
    return true;

  initialized_ = router_->RegisterMediaSinksObserver(this);
  return initialized_;
}

}  // namespace media_router
