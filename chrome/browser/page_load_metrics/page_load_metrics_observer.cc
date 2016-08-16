// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/page_load_metrics_observer.h"

namespace page_load_metrics {

PageLoadExtraInfo::PageLoadExtraInfo(
    const base::Optional<base::TimeDelta>& first_background_time,
    const base::Optional<base::TimeDelta>& first_foreground_time,
    bool started_in_foreground,
    bool user_gesture,
    const GURL& committed_url,
    const base::Optional<base::TimeDelta>& time_to_commit,
    UserAbortType abort_type,
    bool abort_user_initiated,
    const base::Optional<base::TimeDelta>& time_to_abort,
    int num_cache_requests,
    int num_network_requests,
    const PageLoadMetadata& metadata)
    : first_background_time(first_background_time),
      first_foreground_time(first_foreground_time),
      started_in_foreground(started_in_foreground),
      user_gesture(user_gesture),
      committed_url(committed_url),
      time_to_commit(time_to_commit),
      abort_type(abort_type),
      abort_user_initiated(abort_user_initiated),
      time_to_abort(time_to_abort),
      num_cache_requests(num_cache_requests),
      num_network_requests(num_network_requests),
      metadata(metadata) {}

PageLoadExtraInfo::PageLoadExtraInfo(const PageLoadExtraInfo& other) = default;

PageLoadExtraInfo::~PageLoadExtraInfo() {}

FailedProvisionalLoadInfo::FailedProvisionalLoadInfo(base::TimeDelta interval,
                                                     net::Error error)
    : time_to_failed_provisional_load(interval), error(error) {}

FailedProvisionalLoadInfo::~FailedProvisionalLoadInfo() {}

}  // namespace page_load_metrics
