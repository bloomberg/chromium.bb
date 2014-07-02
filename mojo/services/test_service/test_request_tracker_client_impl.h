// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_TEST_SERVICE_TEST_REQUEST_TRACKER_CLIENT_IMPL_H_
#define MOJO_SERVICES_TEST_SERVICE_TEST_REQUEST_TRACKER_CLIENT_IMPL_H_

#include "mojo/public/cpp/system/macros.h"
#include "mojo/services/test_service/test_request_tracker.mojom.h"

namespace mojo {
namespace test {

class TestRequestTrackerClientImpl : public TestRequestTrackerClient {
 public:
  TestRequestTrackerClientImpl(
      TestRequestTrackerPtr tracker,
      const std::string& service_name,
      const mojo::Callback<void()>& tracking_connected_callback);
  virtual ~TestRequestTrackerClientImpl();

  // Call whenever an event happens that you want to be recorded.
  void RecordNewRequest();

  // TestRequestTrackerClient impl.
  virtual void SetIdAndReturnName(
      uint64_t id,
      const mojo::Callback<void(mojo::String)>& callback) MOJO_OVERRIDE;

 private:
  void SendStats();
  uint64_t id_;
  uint64_t requests_since_upload_;
  const std::string service_name_;
  TestRequestTrackerPtr tracker_;
  mojo::Callback<void()> tracking_connected_callback_;
};

}  // namespace test
}  // namespace mojo

#endif  // MOJO_SERVICES_TEST_SERVICE_TEST_REQUEST_TRACKER_CLIENT_IMPL_H_
