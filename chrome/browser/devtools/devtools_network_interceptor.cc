// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_network_interceptor.h"

#include <algorithm>
#include <limits>

#include "base/time/time.h"
#include "chrome/browser/devtools/devtools_network_conditions.h"
#include "net/base/net_errors.h"

namespace {

int64_t kPacketSize = 1500;

}  // namespace

DevToolsNetworkInterceptor::ThrottleRecord::ThrottleRecord() {
}

DevToolsNetworkInterceptor::ThrottleRecord::~ThrottleRecord() {
}

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

void DevToolsNetworkInterceptor::UpdateConditions(
    scoped_ptr<DevToolsNetworkConditions> conditions) {
  DCHECK(conditions);
  base::TimeTicks now = base::TimeTicks::Now();
  if (conditions_->IsThrottling())
    UpdateThrottled(now);

  conditions_ = conditions.Pass();

  bool offline = conditions_->offline();
  if (offline || !conditions_->IsThrottling()) {
    timer_.Stop();
    ThrottleRecords throttled;
    throttled.swap(throttled_);
    for (const ThrottleRecord& record : throttled) {
      record.callback.Run(
          offline ? net::ERR_INTERNET_DISCONNECTED : record.result,
          record.bytes);
    }

    ThrottleRecords suspended;
    suspended.swap(suspended_);
    for (const ThrottleRecord& record : suspended) {
      record.callback.Run(
          offline ? net::ERR_INTERNET_DISCONNECTED : record.result,
          record.bytes);
    }

    return;
  }

  // Throttling.
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
    throttled_[i].bytes -=
        (ticks / length) * kPacketSize + (i < shift ? kPacketSize : 0);
  }
  std::rotate(throttled_.begin(),
      throttled_.begin() + shift, throttled_.end());

  UpdateSuspended(now);
}

void DevToolsNetworkInterceptor::UpdateSuspended(
    base::TimeTicks now) {
  int64_t activation_baseline =
      (now - latency_length_ - base::TimeTicks()).InMicroseconds();
  ThrottleRecords suspended;
  for (const ThrottleRecord& record : suspended_) {
    if (record.send_end <= activation_baseline)
      throttled_.push_back(record);
    else
      suspended.push_back(record);
  }
  suspended_.swap(suspended);
}

void DevToolsNetworkInterceptor::OnTimer() {
  base::TimeTicks now = base::TimeTicks::Now();
  UpdateThrottled(now);

  ThrottleRecords active;
  ThrottleRecords finished;
  for (const ThrottleRecord& record : throttled_) {
    if (record.bytes < 0)
      finished.push_back(record);
    else
      active.push_back(record);
  }
  throttled_.swap(active);

  for (const ThrottleRecord& record : finished)
    record.callback.Run(record.result, record.bytes);

  ArmTimer(now);
}

void DevToolsNetworkInterceptor::ArmTimer(base::TimeTicks now) {
  size_t throttle_count = throttled_.size();
  size_t suspend_count = suspended_.size();
  if (!throttle_count && !suspend_count)
    return;
  int64_t min_ticks_left = 0x10000L;
  for (size_t i = 0; i < throttle_count; ++i) {
    int64_t packets_left = (throttled_[i].bytes +
        kPacketSize - 1) / kPacketSize;
    int64_t ticks_left = (i + 1) + throttle_count * (packets_left - 1);
    if (i == 0 || ticks_left < min_ticks_left)
      min_ticks_left = ticks_left;
  }
  base::TimeTicks desired_time =
      offset_ + tick_length_ * (last_tick_ + min_ticks_left);

  int64_t min_baseline = std::numeric_limits<int64>::max();
  for (size_t i = 0; i < suspend_count; ++i) {
    if (suspended_[i].send_end < min_baseline)
      min_baseline = suspended_[i].send_end;
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

int DevToolsNetworkInterceptor::StartThrottle(
    int result,
    int64_t bytes,
    base::TimeTicks send_end,
    bool start,
    const ThrottleCallback& callback) {
  if (result < 0)
    return result;

  if (conditions_->offline())
    return net::ERR_INTERNET_DISCONNECTED;

  if (!conditions_->IsThrottling())
    return result;

  ThrottleRecord record;
  record.result = result;
  record.bytes = bytes;
  record.callback = callback;

  base::TimeTicks now = base::TimeTicks::Now();
  UpdateThrottled(now);
  if (start && latency_length_ != base::TimeDelta()) {
    record.send_end = (send_end - base::TimeTicks()).InMicroseconds();
    suspended_.push_back(record);
    UpdateSuspended(now);
  } else {
    throttled_.push_back(record);
  }
  ArmTimer(now);

  return net::ERR_IO_PENDING;
}

void DevToolsNetworkInterceptor::StopThrottle(
    const ThrottleCallback& callback) {
  RemoveRecord(&throttled_, callback);
  RemoveRecord(&suspended_, callback);
}

void DevToolsNetworkInterceptor::RemoveRecord(
    ThrottleRecords* records, const ThrottleCallback& callback) {
  records->erase(
      std::remove_if(records->begin(), records->end(),
                     [&callback](const ThrottleRecord& record){
                       return record.callback.Equals(callback);
                     }),
      records->end());
}

bool DevToolsNetworkInterceptor::IsOffline() {
  return conditions_->offline();
}
