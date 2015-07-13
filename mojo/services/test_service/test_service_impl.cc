// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/test_service/test_service_impl.h"

#include "base/bind.h"
#include "base/i18n/time_formatting.h"
#include "base/strings/utf_string_conversions.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "mojo/services/test_service/test_service_application.h"
#include "mojo/services/test_service/test_time_service_impl.h"
#include "mojo/services/test_service/tracked_service.h"

namespace mojo {
namespace test {

TestServiceImpl::TestServiceImpl(ApplicationImpl* app_impl,
                                 TestServiceApplication* application,
                                 InterfaceRequest<TestService> request)
    : application_(application),
      app_impl_(app_impl),
      binding_(this, request.Pass()) {
  binding_.set_connection_error_handler(
      [this]() { application_->ReleaseRef(); });
}

TestServiceImpl::~TestServiceImpl() {
}

void TestServiceImpl::Ping(const mojo::Callback<void()>& callback) {
  if (tracking_)
    tracking_->RecordNewRequest();
  callback.Run();
}

void SendTimeResponse(
    const mojo::Callback<void(int64_t)>& requestor_callback,
    int64_t time_usec) {
  requestor_callback.Run(time_usec);
}

void TestServiceImpl::ConnectToAppAndGetTime(
    const mojo::String& app_url,
    const mojo::Callback<void(int64_t)>& callback) {
  mojo::URLRequestPtr request(mojo::URLRequest::New());
  request->url = mojo::String::From(app_url);
  app_impl_->ConnectToService(request.Pass(), &time_service_);
  if (tracking_) {
    tracking_->RecordNewRequest();
    time_service_->StartTrackingRequests(mojo::Callback<void()>());
  }
  time_service_->GetPartyTime(base::Bind(&SendTimeResponse, callback));
}

void TestServiceImpl::StartTrackingRequests(
    const mojo::Callback<void()>& callback) {
  TestRequestTrackerPtr tracker;
  mojo::URLRequestPtr request(mojo::URLRequest::New());
  request->url = mojo::String::From("mojo:test_request_tracker_app");
  app_impl_->ConnectToService(request.Pass(), &tracker);
  tracking_.reset(new TrackedService(tracker.Pass(), Name_, callback));
}

}  // namespace test
}  // namespace mojo
