// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_FAKE_SYNCHRONOUS_SHUTDOWN_OBJECT_CONTAINER_H_
#define CHROMEOS_COMPONENTS_TETHER_FAKE_SYNCHRONOUS_SHUTDOWN_OBJECT_CONTAINER_H_

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/macros.h"
#include "chromeos/components/tether/synchronous_shutdown_object_container.h"

namespace chromeos {

namespace tether {

// Test double for SynchronousShutdownObjectContainer.
class FakeSynchronousShutdownObjectContainer
    : public SynchronousShutdownObjectContainer {
 public:
  // |deletion_callback| will be invoked when the object is deleted.
  FakeSynchronousShutdownObjectContainer(
      const base::Closure& deletion_callback = base::DoNothing());
  ~FakeSynchronousShutdownObjectContainer() override;

  void set_active_host(ActiveHost* active_host) { active_host_ = active_host; }

  void set_host_scan_cache(HostScanCache* host_scan_cache) {
    host_scan_cache_ = host_scan_cache;
  }

  void set_host_scan_scheduler(HostScanScheduler* host_scan_scheduler) {
    host_scan_scheduler_ = host_scan_scheduler;
  }

  void set_tether_disconnector(TetherDisconnector* tether_disconnector) {
    tether_disconnector_ = tether_disconnector;
  }

  // SynchronousShutdownObjectContainer:
  ActiveHost* active_host() override;
  HostScanCache* host_scan_cache() override;
  HostScanScheduler* host_scan_scheduler() override;
  TetherDisconnector* tether_disconnector() override;

 private:
  base::Closure deletion_callback_;

  ActiveHost* active_host_ = nullptr;
  HostScanCache* host_scan_cache_ = nullptr;
  HostScanScheduler* host_scan_scheduler_ = nullptr;
  TetherDisconnector* tether_disconnector_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(FakeSynchronousShutdownObjectContainer);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_FAKE_SYNCHRONOUS_SHUTDOWN_OBJECT_CONTAINER_H_
