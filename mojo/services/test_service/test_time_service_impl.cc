// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/time/time.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "mojo/services/test_service/test_request_tracker.mojom.h"
#include "mojo/services/test_service/test_time_service_impl.h"
#include "mojo/services/test_service/tracked_service.h"

namespace mojo {
namespace test {

TestTimeServiceImpl::TestTimeServiceImpl(
    ApplicationImpl* app_impl,
    InterfaceRequest<TestTimeService> request)
    : app_impl_(app_impl), binding_(this, request.Pass()) {
}

TestTimeServiceImpl::~TestTimeServiceImpl() {
}

void TestTimeServiceImpl::StartTrackingRequests(
    const mojo::Callback<void()>& callback) {
  TestRequestTrackerPtr tracker;
  mojo::URLRequestPtr request(mojo::URLRequest::New());
  request->url = mojo::String::From("mojo:test_request_tracker_app");
  app_impl_->ConnectToService(request.Pass(), &tracker);
  tracking_.reset(new TrackedService(tracker.Pass(), Name_, callback));
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
