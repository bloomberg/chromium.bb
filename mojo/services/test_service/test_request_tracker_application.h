// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TEST_SERVICE_TEST_REQUEST_TRACKER_APPLICATION_H_
#define SERVICES_TEST_SERVICE_TEST_REQUEST_TRACKER_APPLICATION_H_

#include "mojo/public/cpp/system/macros.h"
#include "mojo/services/test_service/test_request_tracker_impl.h"
#include "mojo/shell/public/cpp/application_delegate.h"
#include "mojo/shell/public/cpp/interface_factory_impl.h"

namespace mojo {
class ApplicationImpl;
namespace test {
class TestTimeService;

// Embeds TestRequestTracker mojo services into an application.
class TestRequestTrackerApplication
    : public ApplicationDelegate,
      public InterfaceFactory<TestTimeService>,
      public InterfaceFactory<TestRequestTracker>,
      public InterfaceFactory<TestTrackedRequestService> {
 public:
  TestRequestTrackerApplication();
  ~TestRequestTrackerApplication() override;

  void Initialize(ApplicationImpl* app) override;

  // ApplicationDelegate methods:
  bool AcceptConnection(ApplicationConnection* connection) override;

  // InterfaceFactory<TestTimeService> methods:
  void Create(ApplicationConnection* connection,
              InterfaceRequest<TestTimeService> request) override;

  // InterfaceFactory<TestRequestTracker> methods:
  void Create(ApplicationConnection* connection,
              InterfaceRequest<TestRequestTracker> request) override;

  // InterfaceFactory<TestTrackedRequestService> methods:
  void Create(ApplicationConnection* connection,
              InterfaceRequest<TestTrackedRequestService> request) override;

 private:
  ApplicationImpl* app_impl_;
  TrackingContext context_;
  MOJO_DISALLOW_COPY_AND_ASSIGN(TestRequestTrackerApplication);
};

}  // namespace test
}  // namespace mojo

#endif  // SERVICES_TEST_SERVICE_TEST_REQUEST_TRACKER_APPLICATION_H_
