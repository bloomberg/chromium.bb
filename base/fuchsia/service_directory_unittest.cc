// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/fuchsia/service_directory.h"

#include <lib/fdio/util.h>
#include <lib/zx/channel.h>
#include <utility>

#include "base/fuchsia/service_directory_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace fuchsia {

class ServiceDirectoryTest : public ServiceDirectoryTestBase {};

// Verifies that ComponentContext can consume a public service in
// ServiceDirectory.
TEST_F(ServiceDirectoryTest, Connect) {
  auto stub = client_context_->ConnectToService<test_fidl::TestInterface>();
  VerifyTestInterface(&stub, false);
}

// Verifies that we can connect to the service service more than once.
TEST_F(ServiceDirectoryTest, ConnectMulti) {
  auto stub = client_context_->ConnectToService<test_fidl::TestInterface>();
  auto stub2 = client_context_->ConnectToService<test_fidl::TestInterface>();
  VerifyTestInterface(&stub, false);
  VerifyTestInterface(&stub2, false);
}

// Verify that services are also exported to the legacy flat service namespace.
TEST_F(ServiceDirectoryTest, ConnectLegacy) {
  ConnectClientContextToDirectory(".");
  auto stub = client_context_->ConnectToService<test_fidl::TestInterface>();
  VerifyTestInterface(&stub, false);
}

// Verify that ComponentContext can handle the case when the service directory
// connection is disconnected.
TEST_F(ServiceDirectoryTest, DirectoryGone) {
  service_binding_.reset();
  service_directory_.reset();

  fidl::InterfacePtr<test_fidl::TestInterface> stub;
  zx_status_t status =
      client_context_->ConnectToService(FidlInterfaceRequest(&stub));
  EXPECT_EQ(status, ZX_ERR_PEER_CLOSED);

  VerifyTestInterface(&stub, true);
}

// Verify that the case when the service doesn't exist is handled properly.
TEST_F(ServiceDirectoryTest, NoService) {
  service_binding_.reset();
  auto stub = client_context_->ConnectToService<test_fidl::TestInterface>();
  VerifyTestInterface(&stub, true);
}

}  // namespace fuchsia
}  // namespace base
