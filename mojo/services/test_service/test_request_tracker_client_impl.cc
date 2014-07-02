// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/test_service/test_request_tracker_client_impl.h"

namespace mojo {
namespace test {

TestRequestTrackerClientImpl::TestRequestTrackerClientImpl(
    TestRequestTrackerPtr tracker,
    const std::string& service_name,
    const mojo::Callback<void()>& callback)
    : id_(0),
      requests_since_upload_(0),
      service_name_(service_name),
      tracker_(tracker.Pass()),
      tracking_connected_callback_(callback) {
  tracker_.set_client(this);
}

TestRequestTrackerClientImpl::~TestRequestTrackerClientImpl() {
}

void TestRequestTrackerClientImpl::RecordNewRequest() {
  requests_since_upload_++;
  if (id_ == kInvalidId)
   return;
  SendStats();
}

void TestRequestTrackerClientImpl::SendStats() {
  ServiceStatsPtr stats(ServiceStats::New());
  stats->num_new_requests = requests_since_upload_;
  stats->health = 0.7;
  tracker_->RecordStats(id_, stats.Pass());
  requests_since_upload_ = 0;
}

void TestRequestTrackerClientImpl::SetIdAndReturnName(
    uint64_t id,
    const mojo::Callback<void(mojo::String)>& callback) {
  assert(id != kInvalidId);
  assert(id_ == kInvalidId);
  id_ = id;
  callback.Run(service_name_);
  tracking_connected_callback_.Run();
  if (requests_since_upload_ == 0)
    return;
  SendStats();
}

}  // namespace test
}  // namespace mojo
