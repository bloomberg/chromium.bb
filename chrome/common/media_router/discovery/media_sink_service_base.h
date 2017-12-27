// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_MEDIA_ROUTER_DISCOVERY_MEDIA_SINK_SERVICE_BASE_H_
#define CHROME_COMMON_MEDIA_ROUTER_DISCOVERY_MEDIA_SINK_SERVICE_BASE_H_

#include <memory>

#include "base/containers/flat_set.h"
#include "base/gtest_prod_util.h"
#include "base/sequence_checker.h"
#include "base/timer/timer.h"
#include "chrome/common/media_router/discovery/media_sink_internal.h"
#include "chrome/common/media_router/discovery/media_sink_service_util.h"

namespace media_router {

// Base class for discovering MediaSinks and notifying the caller with updated
// list. The class rate-limits notifications to |callback|, so that multiple
// updates received in quick succession will be batched.
// This class may be created on any thread, but all subsequent methods must be
// invoked on the same thread.
class MediaSinkServiceBase {
 public:
  // |callback|: The callback to invoke when the list of MediaSinks has been
  // updated.
  explicit MediaSinkServiceBase(const OnSinksDiscoveredCallback& callback);
  virtual ~MediaSinkServiceBase();

 protected:
  void SetTimerForTest(std::unique_ptr<base::Timer> timer);

  // Called when |discovery_timer_| expires.
  virtual void OnDiscoveryComplete();

  // Overriden by subclass to report device counts.
  virtual void RecordDeviceCounts() {}

  // Creates |discovery_timer_| and starts it.
  void StartTimer();

  // Restarts |discovery_timer| if it is stopped. Can only be called after
  // |StartTimer()| is called.
  void RestartTimer();

  // Sorted sinks from current round of discovery.
  base::flat_set<MediaSinkInternal> current_sinks_;

 private:
  friend class MediaSinkServiceBaseTest;
  FRIEND_TEST_ALL_PREFIXES(MediaSinkServiceBaseTest,
                           TestOnDiscoveryComplete_SameSink);

  // Helper method to start |discovery_timer_|.
  void DoStart();

  OnSinksDiscoveredCallback on_sinks_discovered_cb_;

  // Sorted sinks sent to Media Router Provider in last |OnDiscoveryComplete()|
  base::flat_set<MediaSinkInternal> mrp_sinks_;

  // Timer for completing the current round of discovery.
  std::unique_ptr<base::Timer> discovery_timer_;

  SEQUENCE_CHECKER(sequence_checker_);
  DISALLOW_COPY_AND_ASSIGN(MediaSinkServiceBase);
};

}  // namespace media_router

#endif  // CHROME_COMMON_MEDIA_ROUTER_DISCOVERY_MEDIA_SINK_SERVICE_BASE_H_
