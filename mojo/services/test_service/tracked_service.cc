// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/test_service/tracked_service.h"

#include <stdint.h>

#include <utility>

#include "base/bind.h"

namespace mojo {
namespace test {

TrackedService::TrackedService(TestRequestTrackerPtr tracker,
                               const std::string& service_name,
                               const mojo::Callback<void()>& callback)
    : id_(0u),
      requests_since_upload_(0u),
      service_name_(service_name),
      tracker_(std::move(tracker)),
      tracking_connected_callback_(callback) {
  tracker_->SetNameAndReturnId(
      service_name, base::Bind(&TrackedService::SetId, base::Unretained(this)));
}

TrackedService::~TrackedService() {
}

void TrackedService::RecordNewRequest() {
  requests_since_upload_++;
  if (id_ == 0u)
    return;
  SendStats();
}

void TrackedService::SendStats() {
  ServiceStatsPtr stats(ServiceStats::New());
  stats->num_new_requests = requests_since_upload_;
  stats->health = 0.7;
  tracker_->RecordStats(id_, std::move(stats));
  requests_since_upload_ = 0u;
}

void TrackedService::SetId(uint64_t id) {
  assert(id != 0u);
  assert(id_ == 0u);
  id_ = id;
  tracking_connected_callback_.Run();
  if (requests_since_upload_ == 0u)
    return;
  SendStats();
}

}  // namespace test
}  // namespace mojo
