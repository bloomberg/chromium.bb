// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_network_interceptor.h"

#include <limits>

#include "base/time/time.h"
#include "chrome/browser/devtools/devtools_network_conditions.h"

namespace {

int64_t kPacketSize = 1500;

}  // namespace

DevToolsNetworkInterceptor::DevToolsNetworkInterceptor()
    : conditions_(new DevToolsNetworkConditions()),
      weak_ptr_factory_(this) {
}

DevToolsNetworkInterceptor::~DevToolsNetworkInterceptor() {
}

base::WeakPtr<DevToolsNetworkInterceptor>
DevToolsNetworkInterceptor::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void DevToolsNetworkInterceptor::AddThrottable(
    Throttable* throttable) {
  DCHECK(throttables_.find(throttable) == throttables_.end());
  throttables_.insert(throttable);
}

void DevToolsNetworkInterceptor::RemoveThrottable(
    Throttable* throttable) {
  DCHECK(throttables_.find(throttable) != throttables_.end());
  throttables_.erase(throttable);

  if (!conditions_->IsThrottling())
    return;

  base::TimeTicks now = base::TimeTicks::Now();
  UpdateThrottled(now);
  throttled_.erase(
      std::remove(throttled_.begin(), throttled_.end(), throttable),
      throttled_.end());

  Suspended::iterator it = suspended_.begin();
  for (; it != suspended_.end(); ++it) {
    if (it->first == throttable) {
      suspended_.erase(it);
      break;
    }
  }

  ArmTimer(now);
}

void DevToolsNetworkInterceptor::UpdateConditions(
    scoped_ptr<DevToolsNetworkConditions> conditions) {
  DCHECK(conditions);
  base::TimeTicks now = base::TimeTicks::Now();
  if (conditions_->IsThrottling())
    UpdateThrottled(now);

  conditions_ = conditions.Pass();

  if (conditions_->offline()) {
    timer_.Stop();
    throttled_.clear();
    suspended_.clear();
    Throttables old_throttables(throttables_);
    Throttables::iterator it = old_throttables.begin();
    for (;it != old_throttables.end(); ++it) {
      if (throttables_.find(*it) == throttables_.end())
        continue;
      (*it)->Fail();
    }
    return;
  }

  if (conditions_->IsThrottling()) {
    DCHECK(conditions_->download_throughput() != 0);
    offset_ = now;
    last_tick_ = 0;
    int64_t us_tick_length =
        (1000000L * kPacketSize) / conditions_->download_throughput();
    DCHECK(us_tick_length != 0);
    if (us_tick_length == 0)
      us_tick_length = 1;
    tick_length_ = base::TimeDelta::FromMicroseconds(us_tick_length);
    latency_length_ = base::TimeDelta();
    double latency = conditions_->latency();
    if (latency > 0)
      latency_length_ = base::TimeDelta::FromMillisecondsD(latency);
    ArmTimer(now);
  } else {
    timer_.Stop();

    std::vector<Throttable*> throttled;
    throttled.swap(throttled_);
    size_t throttle_count = throttled.size();
    for (size_t i = 0; i < throttle_count; ++i)
      FireThrottledCallback(throttled[i]);

    Suspended suspended;
    suspended.swap(suspended_);
    size_t suspend_count = suspended.size();
    for (size_t i = 0; i < suspend_count; ++i)
      FireThrottledCallback(suspended[i].first);
  }
}

void DevToolsNetworkInterceptor::FireThrottledCallback(
    Throttable* throttable) {
  if (throttables_.find(throttable) != throttables_.end())
    throttable->ThrottleFinished();
}

void DevToolsNetworkInterceptor::UpdateThrottled(
    base::TimeTicks now) {
  int64_t last_tick = (now - offset_) / tick_length_;
  int64_t ticks = last_tick - last_tick_;
  last_tick_ = last_tick;

  int64_t length = throttled_.size();
  if (!length) {
    UpdateSuspended(now);
    return;
  }

  int64_t shift = ticks % length;
  for (int64_t i = 0; i < length; ++i) {
    throttled_[i]->Throttled(
        (ticks / length) * kPacketSize + (i < shift ? kPacketSize : 0));
  }
  std::rotate(throttled_.begin(),
      throttled_.begin() + shift, throttled_.end());

  UpdateSuspended(now);
}

void DevToolsNetworkInterceptor::UpdateSuspended(
    base::TimeTicks now) {
  int64_t activation_baseline =
      (now - latency_length_ - base::TimeTicks()).InMicroseconds();
  Suspended suspended;
  Suspended::iterator it = suspended_.begin();
  for (; it != suspended_.end(); ++it) {
    if (it->second <= activation_baseline)
      throttled_.push_back(it->first);
    else
      suspended.push_back(*it);
  }
  suspended_.swap(suspended);
}

void DevToolsNetworkInterceptor::OnTimer() {
  base::TimeTicks now = base::TimeTicks::Now();
  UpdateThrottled(now);

  std::vector<Throttable*> active;
  std::vector<Throttable*> finished;
  size_t length = throttled_.size();
  for (size_t i = 0; i < length; ++i) {
    if (throttled_[i]->ThrottledByteCount() < 0)
      finished.push_back(throttled_[i]);
    else
      active.push_back(throttled_[i]);
  }
  throttled_.swap(active);

  length = finished.size();
  for (size_t i = 0; i < length; ++i)
    FireThrottledCallback(finished[i]);

  ArmTimer(now);
}

void DevToolsNetworkInterceptor::ArmTimer(base::TimeTicks now) {
  size_t throttle_count = throttled_.size();
  size_t suspend_count = suspended_.size();
  if (!throttle_count && !suspend_count)
    return;
  int64_t min_ticks_left = 0x10000L;
  for (size_t i = 0; i < throttle_count; ++i) {
    int64_t packets_left = (throttled_[i]->ThrottledByteCount() +
        kPacketSize - 1) / kPacketSize;
    int64_t ticks_left = (i + 1) + throttle_count * (packets_left - 1);
    if (i == 0 || ticks_left < min_ticks_left)
      min_ticks_left = ticks_left;
  }
  base::TimeTicks desired_time =
      offset_ + tick_length_ * (last_tick_ + min_ticks_left);

  int64_t min_baseline = std::numeric_limits<int64>::max();
  for (size_t i = 0; i < suspend_count; ++i) {
    if (suspended_[i].second < min_baseline)
      min_baseline = suspended_[i].second;
  }
  if (suspend_count) {
    base::TimeTicks activation_time = base::TimeTicks() +
        base::TimeDelta::FromMicroseconds(min_baseline) + latency_length_;
    if (activation_time < desired_time)
      desired_time = activation_time;
  }

  timer_.Start(
      FROM_HERE,
      desired_time - now,
      base::Bind(
          &DevToolsNetworkInterceptor::OnTimer,
          base::Unretained(this)));
}

void DevToolsNetworkInterceptor::Throttle(
    Throttable* throttable, bool start) {
  base::TimeTicks now = base::TimeTicks::Now();
  UpdateThrottled(now);
  if (start && latency_length_ != base::TimeDelta()) {
    base::TimeTicks send_end;
    throttable->GetSendEndTiming(&send_end);
    if (send_end.is_null())
      send_end = now;
    int64_t us_send_end = (send_end - base::TimeTicks()).InMicroseconds();
    suspended_.push_back(std::make_pair(throttable, us_send_end));
    UpdateSuspended(now);
  } else {
    throttled_.push_back(throttable);
  }
  ArmTimer(now);
}

bool DevToolsNetworkInterceptor::ShouldFail() {
  return conditions_->offline();
}

bool DevToolsNetworkInterceptor::ShouldThrottle() {
  return conditions_->IsThrottling();
}
