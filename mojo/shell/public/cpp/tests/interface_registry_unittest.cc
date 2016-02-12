// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/public/cpp/interface_registry.h"

#include "base/memory/scoped_ptr.h"
#include "mojo/shell/public/cpp/interface_binder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace internal {
namespace {

class TestBinder : public InterfaceBinder {
 public:
  explicit TestBinder(int* delete_count) : delete_count_(delete_count) {}
  ~TestBinder() override { (*delete_count_)++; }
  void BindInterface(Connection* connection,
                     const std::string& interface_name,
                     ScopedMessagePipeHandle client_handle) override {}

 private:
  int* delete_count_;
};

TEST(InterfaceRegistryTest, Ownership) {
  int delete_count = 0;

  // Destruction.
  {
    shell::mojom::InterfaceProviderRequest ir;
    InterfaceRegistry registry(std::move(ir), nullptr);
    InterfaceRegistry::TestApi test_api(&registry);
    test_api.SetInterfaceBinderForName(new TestBinder(&delete_count), "TC1");
  }
  EXPECT_EQ(1, delete_count);

  // Removal.
  {
    shell::mojom::InterfaceProviderRequest ir;
    scoped_ptr<InterfaceRegistry> registry(
        new InterfaceRegistry(std::move(ir), nullptr));
    InterfaceBinder* b = new TestBinder(&delete_count);
    InterfaceRegistry::TestApi test_api(registry.get());
    test_api.SetInterfaceBinderForName(b, "TC1");
    test_api.RemoveInterfaceBinderForName("TC1");
    registry.reset();
    EXPECT_EQ(2, delete_count);
  }

  // Multiple.
  {
    shell::mojom::InterfaceProviderRequest ir;
    InterfaceRegistry registry(std::move(ir), nullptr);
    InterfaceRegistry::TestApi test_api(&registry);
    test_api.SetInterfaceBinderForName(new TestBinder(&delete_count), "TC1");
    test_api.SetInterfaceBinderForName(new TestBinder(&delete_count), "TC2");
  }
  EXPECT_EQ(4, delete_count);

  // Re-addition.
  {
    shell::mojom::InterfaceProviderRequest ir;
    InterfaceRegistry registry(std::move(ir), nullptr);
    InterfaceRegistry::TestApi test_api(&registry);
    test_api.SetInterfaceBinderForName(new TestBinder(&delete_count), "TC1");
    test_api.SetInterfaceBinderForName(new TestBinder(&delete_count), "TC1");
    EXPECT_EQ(5, delete_count);
  }
  EXPECT_EQ(6, delete_count);
}

}  // namespace
}  // namespace internal
}  // namespace mojo
