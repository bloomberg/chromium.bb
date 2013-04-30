// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_time_notifier.h"

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/i18n/time_formatting.h"
#include "base/location.h"
#include "base/message_loop.h"
#include "base/stl_util.h"
#include "base/utf_string_conversions.h"

namespace {

// Clock resolution is platform dependent.
#if defined(OS_WIN)
const int64 kTicksResolutionMs = base::Time::kMinLowResolutionThresholdMs;
#else
const int64 kTicksResolutionMs = 1;  // Assume 1ms for non-windows platforms.
#endif

// Number of time measurements performed in a given network time calculation.
const int kNumTimeMeasurements = 5;

}  // namespace

namespace net {

NetworkTimeNotifier::NetworkTimeNotifier(
    scoped_ptr<base::TickClock> tick_clock) {
  tick_clock_ = tick_clock.Pass();
}

NetworkTimeNotifier::~NetworkTimeNotifier() {}

void NetworkTimeNotifier::UpdateNetworkTime(const base::Time& network_time,
                                            const base::TimeDelta& resolution,
                                            const base::TimeDelta& latency,
                                            const base::TimeTicks& post_time) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "Network time updating to "
           << UTF16ToUTF8(base::TimeFormatFriendlyDateAndTime(network_time));
  // Update network time on every request to limit dependency on ticks lag.
  // TODO(mad): Find a heuristic to avoid augmenting the
  // network_time_uncertainty_ too much by a particularly long latency.
  // Maybe only update when the the new time either improves in accuracy or
  // drifts too far from |network_time_|.
  network_time_ = network_time;

  // Calculate the delay since the network time was received.
  base::TimeTicks now = tick_clock_->NowTicks();
  base::TimeDelta task_delay = now - post_time;
  // Estimate that the time was set midway through the latency time.
  network_time_ticks_ = now - task_delay - latency / 2;

  // Can't assume a better time than the resolution of the given time
  // and 5 ticks measurements are involved, each with their own uncertainty.
  // 1 & 2 are the ones used to compute the latency, 3 is the Now() from when
  // this task was posted, 4 is the Now() above and 5 will be the Now() used in
  // GetNetworkTime().
  network_time_uncertainty_ =
      resolution + latency + kNumTimeMeasurements *
      base::TimeDelta::FromMilliseconds(kTicksResolutionMs);

  for (size_t i = 0; i < observers_.size(); ++i) {
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(observers_[i],
                   network_time_,
                   network_time_ticks_,
                   network_time_uncertainty_));
  }
}

void NetworkTimeNotifier::AddObserver(
    const ObserverCallback& observer_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  observers_.push_back(observer_callback);
  if (!network_time_.is_null()) {
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(observer_callback,
                   network_time_,
                   network_time_ticks_,
                   network_time_uncertainty_));
  }
}

}  // namespace net
