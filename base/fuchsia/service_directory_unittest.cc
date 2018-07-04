// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/fuchsia/service_directory.h"

#include <lib/fdio/util.h>
#include <lib/zx/channel.h>
#include <utility>

#include "base/bind.h"
#include "base/fuchsia/component_context.h"
#include "base/fuchsia/scoped_service_binding.h"
#include "base/fuchsia/test_fidl/cpp/fidl.h"
#include "base/message_loop/message_loop.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace fuchsia {

class TestInterfaceImpl : public test_fidl::TestInterface {
 public:
  void Add(int32_t a, int32_t b, AddCallback callback) override {
    callback(a + b);
  }
};

class ServiceDirectoryTest : public testing::Test {
 public:
  ServiceDirectoryTest() {
    zx::channel service_directory_channel;
    EXPECT_EQ(zx::channel::create(0, &service_directory_channel,
                                  &service_directory_client_channel_),
              ZX_OK);

    // Mount service dir and publish the service.
    service_directory_ = std::make_unique<ServiceDirectory>(
        std::move(service_directory_channel));
    service_binding_ =
        std::make_unique<ScopedServiceBinding<test_fidl::TestInterface>>(
            service_directory_.get(), &test_service_);

    ConnectClientContextToDirectory("public");
  }

  void ConnectClientContextToDirectory(const char* path) {
    // Open directory |path| from the service directory.
    zx::channel public_directory_channel;
    zx::channel public_directory_client_channel;
    EXPECT_EQ(zx::channel::create(0, &public_directory_channel,
                                  &public_directory_client_channel),
              ZX_OK);
    EXPECT_EQ(fdio_open_at(service_directory_client_channel_.get(), path, 0,
                           public_directory_channel.release()),
              ZX_OK);

    // Create ComponentContext and connect to the test service.
    client_context_ = std::make_unique<ComponentContext>(
        std::move(public_directory_client_channel));
  }

  void VerifyTestInterface(fidl::InterfacePtr<test_fidl::TestInterface>* stub,
                           bool expect_error) {
    // Call the service and wait for response.
    base::RunLoop run_loop;
    bool error = false;

    stub->set_error_handler([&run_loop, &error]() {
      error = true;
      run_loop.Quit();
    });

    (*stub)->Add(2, 2, [&run_loop](int32_t result) {
      EXPECT_EQ(result, 4);
      run_loop.Quit();
    });

    run_loop.Run();

    EXPECT_EQ(error, expect_error);

    // Reset error handler because the current one captures |run_loop| and
    // |error| references which are about to be destroyed.
    stub->set_error_handler([]() {});
  }

 protected:
  MessageLoopForIO message_loop_;
  std::unique_ptr<ServiceDirectory> service_directory_;
  zx::channel service_directory_client_channel_;
  TestInterfaceImpl test_service_;
  std::unique_ptr<ScopedServiceBinding<test_fidl::TestInterface>>
      service_binding_;
  std::unique_ptr<ComponentContext> client_context_;
};

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
