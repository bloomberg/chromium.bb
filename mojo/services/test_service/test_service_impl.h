// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_TEST_SERVICE_TEST_SERVICE_IMPL_H_
#define MOJO_SERVICES_TEST_SERVICE_TEST_SERVICE_IMPL_H_

#include "base/memory/scoped_ptr.h"
#include "mojo/public/cpp/system/macros.h"
#include "mojo/services/test_service/test_service.mojom.h"

namespace mojo {
class ApplicationConnection;
namespace test {

class TestRequestTrackerClientImpl;
class TestServiceApplication;

class TestServiceImpl : public InterfaceImpl<TestService> {
 public:
  TestServiceImpl(ApplicationConnection* connection,
                  TestServiceApplication* application);
  ~TestServiceImpl() override;

  // |TestService| methods:
  void OnConnectionEstablished() override;
  void OnConnectionError() override;
  void Ping(const mojo::Callback<void()>& callback) override;
  void ConnectToAppAndGetTime(
      const mojo::String& app_url,
      const mojo::Callback<void(int64_t)>& callback) override;
  void StartTrackingRequests(const mojo::Callback<void()>& callback) override;

 private:
  TestServiceApplication* const application_;
  ApplicationConnection* const connection_;
  TestTimeServicePtr time_service_;
  scoped_ptr<TestRequestTrackerClientImpl> tracking_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(TestServiceImpl);
};

}  // namespace test
}  // namespace mojo

#endif  // MOJO_SERVICES_TEST_SERVICE_TEST_SERVICE_IMPL_H_
