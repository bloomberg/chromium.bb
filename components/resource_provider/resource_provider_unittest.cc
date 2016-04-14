// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "components/resource_provider/test.mojom.h"
#include "mojo/public/cpp/bindings/string.h"
#include "services/shell/public/cpp/shell_test.h"

namespace resource_provider {
namespace test {

class ResourceProviderTest : public shell::test::ShellTest {
 public:
  ResourceProviderTest()
      : shell::test::ShellTest("exe:resource_provider_unittests") {}
  ~ResourceProviderTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ResourceProviderTest);
};

TEST_F(ResourceProviderTest, FetchOneResource) {
  mojom::TestPtr test_app;
  mojo::String resource1;
  connector()->ConnectToInterface("mojo:resource_provider_test_app", &test_app);
  ASSERT_TRUE(test_app->GetResource1(&resource1));
  EXPECT_EQ("test data\n", resource1);
}

TEST_F(ResourceProviderTest, FetchTwoResources) {
  mojom::TestPtr test_app;
  mojo::String resource1, resource2;
  connector()->ConnectToInterface("mojo:resource_provider_test_app", &test_app);
  ASSERT_TRUE(test_app->GetBothResources(&resource1, &resource2));
  EXPECT_EQ("test data\n", resource1);
  EXPECT_EQ("xxyy\n", resource2);
}

}  // namespace test
}  // namespace resource_provider
