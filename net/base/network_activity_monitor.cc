// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_activity_monitor.h"

namespace net {

namespace {

base::LazyInstance<NetworkActivityMonitor>::Leaky g_network_activity_monitor =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

NetworkActivityMonitor::NetworkActivityMonitor()
    : bytes_received_(0), bytes_sent_(0) {
}

NetworkActivityMonitor::~NetworkActivityMonitor() = default;

// static
NetworkActivityMonitor* NetworkActivityMonitor::GetInstance() {
  return g_network_activity_monitor.Pointer();
}

void NetworkActivityMonitor::IncrementBytesReceived(uint64_t bytes_received) {
  base::TimeTicks now = base::TimeTicks::Now();
  base::AutoLock lock(lock_);
  bytes_received_ += bytes_received;
  last_received_ticks_ = now;
}

void NetworkActivityMonitor::IncrementBytesSent(uint64_t bytes_sent) {
  base::TimeTicks now = base::TimeTicks::Now();
  base::AutoLock lock(lock_);
  bytes_sent_ += bytes_sent;
  last_sent_ticks_ = now;
}

uint64_t NetworkActivityMonitor::GetBytesReceived() const {
  base::AutoLock lock(lock_);
  return bytes_received_;
}

uint64_t NetworkActivityMonitor::GetBytesSent() const {
  base::AutoLock lock(lock_);
  return bytes_sent_;
}

base::TimeDelta NetworkActivityMonitor::GetTimeSinceLastReceived() const {
  base::TimeTicks now = base::TimeTicks::Now();
  base::AutoLock lock(lock_);
  return now - last_received_ticks_;
}

base::TimeDelta NetworkActivityMonitor::GetTimeSinceLastSent() const {
  base::TimeTicks now = base::TimeTicks::Now();
  base::AutoLock lock(lock_);
  return now - last_sent_ticks_;
}

}  // namespace net
