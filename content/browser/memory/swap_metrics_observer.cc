// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/memory/swap_metrics_observer.h"

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"

namespace content {

namespace {

// Time between updating swap rates.
const int kSwapMetricsIntervalSeconds = 60;

}  // namespace

SwapMetricsObserver::SwapMetricsObserver()
    : update_interval_(
          base::TimeDelta::FromSeconds(kSwapMetricsIntervalSeconds)) {}

SwapMetricsObserver::~SwapMetricsObserver() {}

void SwapMetricsObserver::Start() {
  timer_.Start(FROM_HERE, update_interval_, this,
               &SwapMetricsObserver::UpdateMetrics);
}

void SwapMetricsObserver::Stop() {
  last_ticks_ = base::TimeTicks();
  timer_.Stop();
}

void SwapMetricsObserver::UpdateMetrics() {
  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeDelta interval =
      last_ticks_.is_null() ? base::TimeDelta() : now - last_ticks_;
  UpdateMetricsInternal(interval);
  last_ticks_ = now;
}

}  // namespace content
