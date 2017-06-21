// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/test_helper.h"

#include "chrome/common/media_router/media_source.h"

namespace media_router {

MockIssuesObserver::MockIssuesObserver(MediaRouter* router)
    : IssuesObserver(router) {}
MockIssuesObserver::~MockIssuesObserver() {}

MockMediaSinksObserver::MockMediaSinksObserver(MediaRouter* router,
                                               const MediaSource& source,
                                               const url::Origin& origin)
    : MediaSinksObserver(router, source, origin) {}
MockMediaSinksObserver::~MockMediaSinksObserver() {
}

MockMediaRoutesObserver::MockMediaRoutesObserver(MediaRouter* router,
    const MediaSource::Id source_id)
    : MediaRoutesObserver(router, source_id) {
}
MockMediaRoutesObserver::~MockMediaRoutesObserver() {
}

MockPresentationConnectionProxy::MockPresentationConnectionProxy() {}
MockPresentationConnectionProxy::~MockPresentationConnectionProxy() {}

}  // namespace media_router
