// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/issues_observer.h"

#include "base/logging.h"
#include "chrome/browser/media/router/media_router.h"

namespace media_router {

IssuesObserver::IssuesObserver(MediaRouter* router)
    : router_(router), initialized_(false) {
  DCHECK(router_);
}

IssuesObserver::~IssuesObserver() {
  if (initialized_)
    router_->UnregisterIssuesObserver(this);
}

void IssuesObserver::Init() {
  if (initialized_)
    return;

  router_->RegisterIssuesObserver(this);
  initialized_ = true;
}

}  // namespace media_router
