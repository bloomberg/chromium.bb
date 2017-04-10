// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/reporting/reporting_policy.h"

namespace net {

ReportingPolicy::ReportingPolicy()
    : max_report_age(base::TimeDelta::FromMinutes(15)),
      max_report_attempts(5),
      garbage_collection_interval(base::TimeDelta::FromMinutes(5)) {
  endpoint_backoff_policy.num_errors_to_ignore = 0;
  endpoint_backoff_policy.initial_delay_ms = 60 * 1000;  // 1 minute
  endpoint_backoff_policy.multiply_factor = 2.0;
  endpoint_backoff_policy.jitter_factor = 0.1;
  endpoint_backoff_policy.maximum_backoff_ms = -1;  // 1 hour
  endpoint_backoff_policy.entry_lifetime_ms = -1;   // infinite
  endpoint_backoff_policy.always_use_initial_delay = false;
}

ReportingPolicy::ReportingPolicy(const ReportingPolicy& other)
    : max_report_age(other.max_report_age),
      max_report_attempts(other.max_report_attempts),
      endpoint_backoff_policy(other.endpoint_backoff_policy),
      garbage_collection_interval(other.garbage_collection_interval) {}

ReportingPolicy::~ReportingPolicy() {}

}  // namespace net
