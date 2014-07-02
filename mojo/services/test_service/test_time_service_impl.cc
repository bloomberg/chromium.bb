// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/time/time.h"
#include "mojo/public/cpp/application/application_connection.h"
#include "mojo/services/test_service/test_request_tracker.mojom.h"
#include "mojo/services/test_service/test_request_tracker_client_impl.h"
#include "mojo/services/test_service/test_time_service_impl.h"

namespace mojo {
namespace test {

TestTimeServiceImpl::TestTimeServiceImpl(ApplicationConnection* application)
    : application_(application) {
}

TestTimeServiceImpl::~TestTimeServiceImpl() {
}

void TestTimeServiceImpl::StartTrackingRequests(
    const mojo::Callback<void()>& callback) {
  TestRequestTrackerPtr tracker;
  application_->ConnectToService(
      "mojo:mojo_test_request_tracker_app", &tracker);
  tracking_.reset(new TestRequestTrackerClientImpl(
      tracker.Pass(), Name_, callback));
}

void TestTimeServiceImpl::GetPartyTime(
    const mojo::Callback<void(int64_t)>& callback) {
  if (tracking_)
    tracking_->RecordNewRequest();
  base::Time frozen_time(base::Time::UnixEpoch()
      + base::TimeDelta::FromDays(10957)
      + base::TimeDelta::FromHours(7)
      + base::TimeDelta::FromMinutes(59));
  int64 time(frozen_time.ToInternalValue());
  callback.Run(time);
}

}  // namespace test
}  // namespace mojo
