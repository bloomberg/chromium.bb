// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_webrequest_time_tracker.h"

#include "base/metrics/histogram.h"

// TODO(mpcomplete): tweak all these constants.
namespace {
// The number of requests we keep track of at a time.
const size_t kMaxRequestsLogged = 100u;

// If a request completes faster than this amount (in ms), then we ignore it.
// Any delays on such a request was negligible.
const int kMinRequestTimeToCareMs = 10;

// Thresholds above which we consider a delay caused by an extension to be "too
// much". This is given in percentage of total request time that was spent
// waiting on the extension.
const double kThresholdModerateDelay = 0.20;
const double kThresholdExcessiveDelay = 0.50;

// If this many requests (of the past kMaxRequestsLogged) have had "too much"
// delay, then we will warn the user.
const size_t kNumModerateDelaysBeforeWarning = 50u;
const size_t kNumExcessiveDelaysBeforeWarning = 10u;
}  // namespace

ExtensionWebRequestTimeTracker::RequestTimeLog::RequestTimeLog()
    : completed(false) {
}

ExtensionWebRequestTimeTracker::RequestTimeLog::~RequestTimeLog() {
}

ExtensionWebRequestTimeTracker::ExtensionWebRequestTimeTracker() {
}

ExtensionWebRequestTimeTracker::~ExtensionWebRequestTimeTracker() {
}

void ExtensionWebRequestTimeTracker::LogRequestStartTime(
    int64 request_id, const base::Time& start_time, const GURL& url) {
  // Trim old completed request logs.
  while (request_ids_.size() > kMaxRequestsLogged) {
    int64 to_remove = request_ids_.front();
    request_ids_.pop();
    std::map<int64, RequestTimeLog>::iterator iter =
        request_time_logs_.find(to_remove);
    if (iter != request_time_logs_.end() && iter->second.completed) {
      request_time_logs_.erase(iter);
      moderate_delays_.erase(to_remove);
      excessive_delays_.erase(to_remove);
    }
  }
  request_ids_.push(request_id);

  if (request_time_logs_.find(request_id) != request_time_logs_.end()) {
    RequestTimeLog& log = request_time_logs_[request_id];
    DCHECK(!log.completed);
    return;
  }
  RequestTimeLog& log = request_time_logs_[request_id];
  log.request_start_time = start_time;
  log.url = url;
}

void ExtensionWebRequestTimeTracker::LogRequestEndTime(
    int64 request_id, const base::Time& end_time) {
  if (request_time_logs_.find(request_id) == request_time_logs_.end())
    return;

  RequestTimeLog& log = request_time_logs_[request_id];
  if (log.completed)
    return;

  log.request_duration = end_time - log.request_start_time;
  log.completed = true;

  if (log.extension_block_durations.empty())
    return;

  HISTOGRAM_TIMES("Extensions.NetworkDelay", log.block_duration);

  Analyze(request_id);
}

void ExtensionWebRequestTimeTracker::Analyze(int64 request_id) {
  RequestTimeLog& log = request_time_logs_[request_id];

  // Ignore really short requests. Time spent on these is negligible, and any
  // extra delay the extension adds is likely to be noise.
  if (log.request_duration.InMilliseconds() < kMinRequestTimeToCareMs)
    return;

  double percentage =
      log.block_duration.InMillisecondsF() /
      log.request_duration.InMillisecondsF();
  LOG(ERROR) << "WR percent " << request_id << ": " << log.url << ": " <<
      log.block_duration.InMilliseconds() << "/" <<
      log.request_duration.InMilliseconds() << " = " << percentage;

  // TODO(mpcomplete): need actual UI for the warning.
  // TODO(mpcomplete): blame a specific extension. Maybe go through the list
  // of recent requests and find the extension that has caused the most delays.
  if (percentage > kThresholdExcessiveDelay) {
    excessive_delays_.insert(request_id);
    if (excessive_delays_.size() > kNumExcessiveDelaysBeforeWarning) {
      LOG(ERROR) << "WR excessive delays:" << excessive_delays_.size();
    }
  } else if (percentage > kThresholdModerateDelay) {
    moderate_delays_.insert(request_id);
    if (moderate_delays_.size() > kNumModerateDelaysBeforeWarning) {
      LOG(ERROR) << "WR moderate delays:" << moderate_delays_.size();
    }
  }
}

void ExtensionWebRequestTimeTracker::IncrementExtensionBlockTime(
    const std::string& extension_id,
    int64 request_id,
    const base::TimeDelta& block_time) {
  if (request_time_logs_.find(request_id) == request_time_logs_.end())
    return;
  RequestTimeLog& log = request_time_logs_[request_id];
  log.extension_block_durations[extension_id] += block_time;
}

void ExtensionWebRequestTimeTracker::IncrementTotalBlockTime(
    int64 request_id,
    const base::TimeDelta& block_time) {
  if (request_time_logs_.find(request_id) == request_time_logs_.end())
    return;
  RequestTimeLog& log = request_time_logs_[request_id];
  log.block_duration += block_time;
}

void ExtensionWebRequestTimeTracker::SetRequestCanceled(int64 request_id) {
  // Canceled requests won't actually hit the network, so we can't compare
  // their request time to the time spent waiting on the extension. Just ignore
  // them.
  // TODO(mpcomplete): may want to count cancels as "bonuses" for an extension.
  // i.e. if it slows down 50% of requests but cancels 25% of the rest, that
  // might average out to only being "25% slow".
  request_time_logs_.erase(request_id);
}

void ExtensionWebRequestTimeTracker::SetRequestRedirected(int64 request_id) {
  // When a request is redirected, we have no way of knowing how long the
  // request would have taken, so we can't say how much an extension slowed
  // down this request. Just ignore it.
  request_time_logs_.erase(request_id);
}
