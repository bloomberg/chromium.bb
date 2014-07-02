// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_TEST_SERVICE_TEST_REQUEST_TRACKER_APPLICATION_H_
#define MOJO_SERVICES_TEST_SERVICE_TEST_REQUEST_TRACKER_APPLICATION_H_

#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/system/macros.h"
#include "mojo/services/test_service/test_request_tracker_impl.h"

namespace mojo {
namespace test {

// Embeds TestRequestTracker mojo services into an application.
class TestRequestTrackerApplication : public ApplicationDelegate {
 public:
  TestRequestTrackerApplication();
  virtual ~TestRequestTrackerApplication();

  virtual bool ConfigureIncomingConnection(ApplicationConnection* connection)
      MOJO_OVERRIDE;

 private:
  TrackingContext context_;
  MOJO_DISALLOW_COPY_AND_ASSIGN(TestRequestTrackerApplication);
};

}  // namespace test
}  // namespace mojo

#endif  // MOJO_SERVICES_TEST_SERVICE_TEST_REQUEST_TRACKER_APPLICATION_H_
