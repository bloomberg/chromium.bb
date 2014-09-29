// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/application/lib/service_registry.h"

#include "mojo/public/cpp/application/lib/service_connector.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace internal {
namespace {

class TestConnector : public ServiceConnectorBase {
 public:
  TestConnector(const std::string& name, int* delete_count)
      : ServiceConnectorBase(name), delete_count_(delete_count) {}
  virtual ~TestConnector() { (*delete_count_)++; }
  virtual void ConnectToService(
      const std::string& name,
      ScopedMessagePipeHandle client_handle) override {}

 private:
  int* delete_count_;
};

TEST(ServiceRegistryTest, Ownership) {
  int delete_count = 0;

  // Destruction.
  {
    ServiceRegistry registry;
    registry.AddServiceConnector(new TestConnector("TC1", &delete_count));
  }
  EXPECT_EQ(1, delete_count);

  // Removal.
  {
    ServiceRegistry registry;
    ServiceConnectorBase* c = new TestConnector("TC1", &delete_count);
    registry.AddServiceConnector(c);
    registry.RemoveServiceConnector(c);
    EXPECT_EQ(2, delete_count);
  }

  // Multiple.
  {
    ServiceRegistry registry;
    registry.AddServiceConnector(new TestConnector("TC1", &delete_count));
    registry.AddServiceConnector(new TestConnector("TC2", &delete_count));
  }
  EXPECT_EQ(4, delete_count);

  // Re-addition.
  {
    ServiceRegistry registry;
    registry.AddServiceConnector(new TestConnector("TC1", &delete_count));
    registry.AddServiceConnector(new TestConnector("TC1", &delete_count));
    EXPECT_EQ(5, delete_count);
  }
  EXPECT_EQ(6, delete_count);
}

}  // namespace
}  // namespace internal
}  // namespace mojo
