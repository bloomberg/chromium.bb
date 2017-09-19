// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/test_helper.h"

#include "base/base64.h"
#include "base/json/string_escape.h"
#include "chrome/common/media_router/media_source.h"

namespace media_router {

std::string PresentationConnectionMessageToString(
    const content::PresentationConnectionMessage& message) {
  if (!message.message && !message.data)
    return "null";
  std::string result;
  if (message.message) {
    result = "text=";
    base::EscapeJSONString(*message.message, true, &result);
  } else {
    const base::StringPiece src(
        reinterpret_cast<const char*>(message.data->data()),
        message.data->size());
    base::Base64Encode(src, &result);
    result = "binary=" + result;
  }
  return result;
}

MockIssuesObserver::MockIssuesObserver(IssueManager* issue_manager)
    : IssuesObserver(issue_manager) {}
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
