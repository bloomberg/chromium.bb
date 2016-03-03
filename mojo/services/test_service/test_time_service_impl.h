// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TEST_SERVICE_TEST_TIME_SERVICE_IMPL_H_
#define SERVICES_TEST_SERVICE_TEST_TIME_SERVICE_IMPL_H_

#include <stdint.h>

#include "base/memory/scoped_ptr.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/public/cpp/system/macros.h"
#include "mojo/services/test_service/test_service.mojom.h"

namespace mojo {
class Connection;
class Connector;

namespace test {

class TrackedService;

class TestTimeServiceImpl : public TestTimeService {
 public:
  TestTimeServiceImpl(Connector* connector, TestTimeServiceRequest request);
  ~TestTimeServiceImpl() override;

  // |TestTimeService| methods:
  void GetPartyTime(
      const mojo::Callback<void(int64_t time_usec)>& callback) override;
  void StartTrackingRequests(const mojo::Callback<void()>& callback) override;

 private:
  Connector* connector_;
  scoped_ptr<TrackedService> tracking_;
  StrongBinding<TestTimeService> binding_;
  DISALLOW_COPY_AND_ASSIGN(TestTimeServiceImpl);
};

}  // namespace test
}  // namespace mojo

#endif  // SERVICES_TEST_SERVICE_TEST_TIME_SERVICE_IMPL_H_
