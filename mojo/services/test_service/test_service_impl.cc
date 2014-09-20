// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/test_service/test_service_impl.h"

#include "base/bind.h"
#include "base/i18n/time_formatting.h"
#include "base/strings/utf_string_conversions.h"
#include "mojo/public/cpp/application/application_connection.h"
#include "mojo/services/test_service/test_request_tracker_client_impl.h"
#include "mojo/services/test_service/test_service_application.h"
#include "mojo/services/test_service/test_time_service_impl.h"

namespace mojo {
namespace test {

TestServiceImpl::TestServiceImpl(ApplicationConnection* connection,
                                 TestServiceApplication* application)
    : application_(application), connection_(connection) {
}

TestServiceImpl::~TestServiceImpl() {
}

void TestServiceImpl::OnConnectionEstablished() {
  application_->AddRef();
}

void TestServiceImpl::OnConnectionError() {
  application_->ReleaseRef();
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
  connection_->ConnectToService(app_url, &time_service_);
  if (tracking_) {
    tracking_->RecordNewRequest();
    time_service_->StartTrackingRequests(mojo::Callback<void()>());
  }
  time_service_->GetPartyTime(base::Bind(&SendTimeResponse, callback));
}

void TestServiceImpl::StartTrackingRequests(
    const mojo::Callback<void()>& callback) {
  TestRequestTrackerPtr tracker;
  connection_->ConnectToService(
      "mojo:mojo_test_request_tracker_app", &tracker);
  tracking_.reset(
      new TestRequestTrackerClientImpl(tracker.Pass(), Name_, callback));
}

}  // namespace test
}  // namespace mojo
