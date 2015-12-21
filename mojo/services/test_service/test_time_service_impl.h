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

class ApplicationConnection;
class ApplicationImpl;

namespace test {

class TrackedService;

class TestTimeServiceImpl : public TestTimeService {
 public:
  TestTimeServiceImpl(ApplicationImpl* app_impl,
                      InterfaceRequest<TestTimeService> request);
  ~TestTimeServiceImpl() override;

  // |TestTimeService| methods:
  void GetPartyTime(
      const mojo::Callback<void(int64_t time_usec)>& callback) override;
  void StartTrackingRequests(const mojo::Callback<void()>& callback) override;

 private:
  ApplicationImpl* app_impl_;
  scoped_ptr<TrackedService> tracking_;
  StrongBinding<TestTimeService> binding_;
  MOJO_DISALLOW_COPY_AND_ASSIGN(TestTimeServiceImpl);
};

}  // namespace test
}  // namespace mojo

#endif  // SERVICES_TEST_SERVICE_TEST_TIME_SERVICE_IMPL_H_
