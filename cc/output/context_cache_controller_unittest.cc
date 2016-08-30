// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/context_cache_controller.h"

#include "base/memory/ptr_util.h"
#include "cc/test/test_context_provider.h"
#include "cc/test/test_context_support.h"
#include "cc/test/test_web_graphics_context_3d.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {
using ::testing::Mock;
using ::testing::StrictMock;

class MockContextSupport : public TestContextSupport {
 public:
  MockContextSupport() {}
  MOCK_METHOD1(SetAggressivelyFreeResources,
               void(bool aggressively_free_resources));
};

TEST(ContextCacheControllerTest, ScopedVisibilityBasic) {
  StrictMock<MockContextSupport> context_support;
  ContextCacheController cache_controller(&context_support);

  EXPECT_CALL(context_support, SetAggressivelyFreeResources(false));
  std::unique_ptr<ContextCacheController::ScopedVisibility> visibility =
      cache_controller.ClientBecameVisible();
  Mock::VerifyAndClearExpectations(&context_support);

  EXPECT_CALL(context_support, SetAggressivelyFreeResources(true));
  cache_controller.ClientBecameNotVisible(std::move(visibility));
}

TEST(ContextCacheControllerTest, ScopedVisibilityMulti) {
  StrictMock<MockContextSupport> context_support;
  ContextCacheController cache_controller(&context_support);

  EXPECT_CALL(context_support, SetAggressivelyFreeResources(false));
  std::unique_ptr<ContextCacheController::ScopedVisibility> visibility_1 =
      cache_controller.ClientBecameVisible();
  Mock::VerifyAndClearExpectations(&context_support);
  std::unique_ptr<ContextCacheController::ScopedVisibility> visibility_2 =
      cache_controller.ClientBecameVisible();

  cache_controller.ClientBecameNotVisible(std::move(visibility_1));
  EXPECT_CALL(context_support, SetAggressivelyFreeResources(true));
  cache_controller.ClientBecameNotVisible(std::move(visibility_2));
}

}  // namespace
}  // namespace cc
