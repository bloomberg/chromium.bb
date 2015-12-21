// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TEST_SERVICE_TRACKED_SERVICE_H_
#define SERVICES_TEST_SERVICE_TRACKED_SERVICE_H_

#include <stdint.h>

#include "mojo/public/cpp/system/macros.h"
#include "mojo/services/test_service/test_request_tracker.mojom.h"

namespace mojo {
namespace test {

class TrackedService {
 public:
  TrackedService(TestRequestTrackerPtr tracker,
                 const std::string& service_name,
                 const mojo::Callback<void()>& tracking_connected_callback);
  ~TrackedService();

  // Call whenever an event happens that you want to be recorded.
  void RecordNewRequest();

 private:
  void SetId(uint64_t id);
  void SendStats();

  uint64_t id_;
  uint64_t requests_since_upload_;
  const std::string service_name_;
  TestRequestTrackerPtr tracker_;
  mojo::Callback<void()> tracking_connected_callback_;
};

}  // namespace test
}  // namespace mojo

#endif  // SERVICES_TEST_SERVICE_TRACKED_SERVICE_H_
