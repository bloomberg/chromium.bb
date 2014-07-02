// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/test_service/test_request_tracker_application.h"

#include <assert.h>

#include "mojo/public/cpp/application/application_connection.h"
#include "mojo/services/test_service/test_time_service_impl.h"

namespace mojo {
namespace test {

TestRequestTrackerApplication::TestRequestTrackerApplication() {
}

TestRequestTrackerApplication::~TestRequestTrackerApplication() {
}

bool TestRequestTrackerApplication::ConfigureIncomingConnection(
    ApplicationConnection* connection) {
  // Every instance of the service and recorder shares the context.
  // Note, this app is single-threaded, so this is thread safe.
  connection->AddService<TestTrackedRequestServiceImpl>(&context_);
  connection->AddService<TestRequestTrackerImpl>(&context_);
  connection->AddService<TestTimeServiceImpl>();
  return true;
}

}  // namespace test

// static
ApplicationDelegate* ApplicationDelegate::Create() {
  return new mojo::test::TestRequestTrackerApplication();
}

}  // namespace mojo
