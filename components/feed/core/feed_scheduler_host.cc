// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/feed_scheduler_host.h"

#include <utility>

#include "base/time/time.h"

namespace feed {

FeedSchedulerHost::FeedSchedulerHost() {}

FeedSchedulerHost::~FeedSchedulerHost() {}

NativeRequestBehavior FeedSchedulerHost::ShouldSessionRequestData(
    bool has_content,
    base::Time content_creation_date_time,
    bool has_outstanding_request) {
  return REQUEST_WITH_CONTENT;
}

void FeedSchedulerHost::OnReceiveNewContent(
    base::Time content_creation_date_time) {}

void FeedSchedulerHost::OnRequestError(int network_response_code) {}

void FeedSchedulerHost::RegisterTriggerRefreshCallback(
    base::RepeatingClosure callback) {
  // There should only ever be one scheduler host and bridge created. This may
  // stop being true eventually.
  DCHECK(trigger_refresh_.is_null());

  trigger_refresh_ = std::move(callback);
}

}  // namespace feed
