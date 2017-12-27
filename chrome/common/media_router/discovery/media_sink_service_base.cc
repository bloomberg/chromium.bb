// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/media_router/discovery/media_sink_service_base.h"

#include <vector>

namespace {
// Time interval when media sink service sends sinks to MRP.
const int kFetchCompleteTimeoutSecs = 3;
}  // namespace

namespace media_router {

MediaSinkServiceBase::MediaSinkServiceBase(
    const OnSinksDiscoveredCallback& callback)
    : on_sinks_discovered_cb_(callback) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  DCHECK(!on_sinks_discovered_cb_.is_null());
}

MediaSinkServiceBase::~MediaSinkServiceBase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void MediaSinkServiceBase::SetTimerForTest(std::unique_ptr<base::Timer> timer) {
  DCHECK(!discovery_timer_);
  discovery_timer_ = std::move(timer);
}

void MediaSinkServiceBase::OnDiscoveryComplete() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (current_sinks_ == mrp_sinks_) {
    DVLOG(2) << "No update to sink list.";
    return;
  }

  DVLOG(2) << "Send sinks to media router, [size]: " << current_sinks_.size();
  on_sinks_discovered_cb_.Run(std::vector<MediaSinkInternal>(
      current_sinks_.begin(), current_sinks_.end()));
  mrp_sinks_ = current_sinks_;
  discovery_timer_->Stop();
  RecordDeviceCounts();
}

void MediaSinkServiceBase::StartTimer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // |discovery_timer_| is already set to a mock timer in unit tests only.
  if (!discovery_timer_)
    discovery_timer_.reset(new base::OneShotTimer());

  DoStart();
}

void MediaSinkServiceBase::RestartTimer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!discovery_timer_->IsRunning())
    DoStart();
}

void MediaSinkServiceBase::DoStart() {
  base::TimeDelta finish_delay =
      base::TimeDelta::FromSeconds(kFetchCompleteTimeoutSecs);
  discovery_timer_->Start(
      FROM_HERE, finish_delay,
      base::BindRepeating(&MediaSinkServiceBase::OnDiscoveryComplete,
                          base::Unretained(this)));
}

}  // namespace media_router
