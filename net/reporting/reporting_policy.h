// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_REPORTING_REPORTING_POLICY_H_
#define NET_REPORTING_REPORTING_POLICY_H_

#include "base/time/time.h"
#include "net/base/backoff_entry.h"
#include "net/base/net_export.h"

namespace net {

// Various policy knobs for the Reporting system.
struct NET_EXPORT ReportingPolicy {
  // Provides a reasonable default for use in a browser embedder.
  ReportingPolicy();
  ReportingPolicy(const ReportingPolicy& other);
  ~ReportingPolicy();

  // Maximum age a report can be queued for before being discarded as expired.
  base::TimeDelta max_report_age;

  // Maximum number of delivery attempts a report can have before being
  // discarded as failed.
  int max_report_attempts;

  // Backoff policy for failing endpoints.
  BackoffEntry::Policy endpoint_backoff_policy;

  // Minimum interval at which to garbage-collect the cache.
  base::TimeDelta garbage_collection_interval;
};

}  // namespace net

#endif  // NET_REPORTING_REPORTING_POLICY_H_
