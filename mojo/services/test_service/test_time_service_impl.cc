// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/test_service/test_time_service_impl.h"

#include <stdint.h>

#include <utility>

#include "base/time/time.h"
#include "mojo/services/test_service/test_request_tracker.mojom.h"
#include "mojo/services/test_service/tracked_service.h"
#include "mojo/shell/public/cpp/shell.h"

namespace mojo {
namespace test {

TestTimeServiceImpl::TestTimeServiceImpl(
      Shell* shell,
      InterfaceRequest<TestTimeService> request)
    : shell_(shell), binding_(this, std::move(request)) {}

TestTimeServiceImpl::~TestTimeServiceImpl() {
}

void TestTimeServiceImpl::StartTrackingRequests(
    const mojo::Callback<void()>& callback) {
  TestRequestTrackerPtr tracker;
  shell_->ConnectToService("mojo:test_request_tracker_app", &tracker);
  tracking_.reset(new TrackedService(std::move(tracker), Name_, callback));
}

void TestTimeServiceImpl::GetPartyTime(
    const mojo::Callback<void(int64_t)>& callback) {
  if (tracking_)
    tracking_->RecordNewRequest();
  base::Time frozen_time(base::Time::UnixEpoch()
      + base::TimeDelta::FromDays(10957)
      + base::TimeDelta::FromHours(7)
      + base::TimeDelta::FromMinutes(59));
  int64_t time(frozen_time.ToInternalValue());
  callback.Run(time);
}

}  // namespace test
}  // namespace mojo
