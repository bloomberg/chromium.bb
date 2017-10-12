// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_MEDIA_SINK_SERVICE_BASE_H_
#define CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_MEDIA_SINK_SERVICE_BASE_H_

#include <memory>
#include <set>

#include "base/gtest_prod_util.h"
#include "base/timer/timer.h"
#include "chrome/common/media_router/discovery/media_sink_internal.h"
#include "chrome/common/media_router/discovery/media_sink_service.h"

namespace media_router {

class MediaSinkServiceBase : public MediaSinkService {
 public:
  explicit MediaSinkServiceBase(const OnSinksDiscoveredCallback& callback);
  ~MediaSinkServiceBase() override;

  void ForceSinkDiscoveryCallback() override;

 protected:
  void SetTimerForTest(std::unique_ptr<base::Timer> timer);

  // Called when |finish_timer_| expires.
  virtual void OnFetchCompleted();

  // Helper function to start |finish_timer_|. Create a new timer if none
  // exists.
  void StartTimer();

  // Helper function to stop |finish_timer_|.
  void StopTimer();

  // Helper function to restart |finish_timer|. No-op if timer does not exist or
  // timer is currently running.
  void RestartTimer();

  // Overriden by subclass to report device counts.
  virtual void RecordDeviceCounts() {}

  // Time out value for |finish_timer_|
  int fetch_complete_timeout_secs_;

  // Sorted sinks from current round of discovery.
  std::set<MediaSinkInternal> current_sinks_;

 private:
  friend class MediaSinkServiceBaseTest;
  FRIEND_TEST_ALL_PREFIXES(MediaSinkServiceBaseTest,
                           TestFetchCompleted_SameSink);

  // Sorted sinks sent to Media Router Provider in last FetchCompleted().
  std::set<MediaSinkInternal> mrp_sinks_;

  // Timer for finishing fetching.
  std::unique_ptr<base::Timer> finish_timer_;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_MEDIA_SINK_SERVICE_BASE_H_
