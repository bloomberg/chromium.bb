// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_TEST_SERVICE_TEST_TIME_SERVICE_IMPL_H_
#define MOJO_SERVICES_TEST_SERVICE_TEST_TIME_SERVICE_IMPL_H_

#include "base/memory/scoped_ptr.h"
#include "mojo/public/cpp/system/macros.h"
#include "mojo/services/test_service/test_service.mojom.h"

namespace mojo {

class ApplicationConnection;

namespace test {

class TestRequestTrackerClientImpl;

class TestTimeServiceImpl : public InterfaceImpl<TestTimeService> {
 public:
  explicit TestTimeServiceImpl(ApplicationConnection* application);
  virtual ~TestTimeServiceImpl();

  // |TestTimeService| methods:
  virtual void GetPartyTime(
      const mojo::Callback<void(int64_t time_usec)>& callback) MOJO_OVERRIDE;
  virtual void StartTrackingRequests(const mojo::Callback<void()>& callback)
      MOJO_OVERRIDE;

 private:
  ApplicationConnection* application_;
  scoped_ptr<TestRequestTrackerClientImpl> tracking_;
  MOJO_DISALLOW_COPY_AND_ASSIGN(TestTimeServiceImpl);
};

}  // namespace test
}  // namespace mojo

#endif  // MOJO_SERVICES_TEST_SERVICE_TEST_TIME_SERVICE_IMPL_H_
